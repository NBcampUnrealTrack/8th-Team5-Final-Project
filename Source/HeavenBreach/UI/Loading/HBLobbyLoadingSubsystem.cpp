#include "UI/Loading/HBLobbyLoadingSubsystem.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"
#include "Core/GameFlow/Lobby/HBLobbyGameState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"

DEFINE_LOG_CATEGORY_STATIC(LogHBLobbyLoading, Log, All);

UHBLobbyLoadingSubsystem::UHBLobbyLoadingSubsystem()
{
	static ConstructorHelpers::FClassFinder<UUserWidget> Screen(TEXT("/Game/UI/Startup/WBP_LoadingScreen"));
	if (Screen.Succeeded()) LoadingWidgetClass = Screen.Class;
}

bool UHBLobbyLoadingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && !IsRunningCommandlet() && Super::ShouldCreateSubsystem(Outer);
}

void UHBLobbyLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SeamlessStartHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravelStart);
	PreClientTravelHandle = GetGameInstance()->OnNotifyPreClientTravel().AddUObject(this, &ThisClass::HandlePreClientTravel);
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
}

void UHBLobbyLoadingSubsystem::Deinitialize()
{
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessStartHandle);
	GetGameInstance()->OnNotifyPreClientTravel().Remove(PreClientTravelHandle);
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	FinishLoading(false);
	Super::Deinitialize();
}

bool UHBLobbyLoadingSubsystem::OwnsWorld(const UWorld* World) const
{
	return World && World->GetGameInstance() == GetGameInstance() && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}

FString UHBLobbyLoadingSubsystem::NormalizeMap(const FString& URL)
{
	const FURL TravelURL(nullptr, *URL, TRAVEL_Absolute);
	return UWorld::RemovePIEPrefix(FPackageName::ObjectPathToPackageName(TravelURL.Map));
}

bool UHBLobbyLoadingSubsystem::IsLobbyWorld(const UWorld* World)
{
	return World && (World->GetAuthGameMode<AHBLobbyGameMode>() || World->GetGameState<AHBLobbyGameState>());
}

void UHBLobbyLoadingSubsystem::HandlePreClientTravel(const FString& Destination, ETravelType TravelType, bool bSeamless)
{
	UE_LOG(LogHBLobbyLoading, Verbose, TEXT("PRE CLIENT | %s | world=%s | destination=%s"), *GetGameInstance()->GetName(), *GetNameSafe(GetGameInstance()->GetWorld()), *Destination);
	BeginLoading(GetGameInstance()->GetWorld(), Destination);
}

void UHBLobbyLoadingSubsystem::HandleSeamlessTravelStart(UWorld* World, const FString& Destination)
{
	if (World && World->GetGameInstance() == GetGameInstance()) UE_LOG(LogHBLobbyLoading, Verbose, TEXT("SEAMLESS START | %s | world=%s | lobby=%d | net=%d | destination=%s"), *GetGameInstance()->GetName(), *GetNameSafe(World), IsLobbyWorld(World), static_cast<int32>(World->GetNetMode()), *Destination);
	BeginLoading(World, Destination);
}

void UHBLobbyLoadingSubsystem::HandlePreLoadMap(const FWorldContext& Context, const FString& Destination)
{
	if (Context.OwningGameInstance == GetGameInstance()) BeginLoading(Context.World(), Destination);
}

void UHBLobbyLoadingSubsystem::BeginLoading(UWorld* SourceWorld, const FString& Destination)
{
	if (!OwnsWorld(SourceWorld)) return;
	const FString Map = NormalizeMap(Destination);
	if (bLoading)
	{
		// PreClientTravel and OnSeamlessTravelStart describe the same trip.
		if (Map == DestinationPackage) return;
		FinishLoading(false);
	}
	if (!IsLobbyWorld(SourceWorld) || Map.IsEmpty() || Map == NormalizeMap(SourceWorld->GetPackage()->GetName())) return;
	// A leave/recovery trip to the menu/lobby is not a departure into gameplay.
	if (Map.StartsWith(TEXT("/Game/Levels/Lobby/")) || Map.StartsWith(TEXT("/Game/Levels/Test_CW/Multiplayer/"))) return;

	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (!Viewport || !LoadingWidgetClass)
	{
		UE_LOG(LogHBLobbyLoading, Warning, TEXT("No loading viewport/class | %s | viewport=%s | class=%s"), *GetGameInstance()->GetName(), *GetNameSafe(Viewport), *GetNameSafe(LoadingWidgetClass));
		return;
	}
	LoadingWidget = CreateWidget<UUserWidget>(GetGameInstance(), LoadingWidgetClass);
	if (!LoadingWidget)
	{
		UE_LOG(LogHBLobbyLoading, Error, TEXT("Could not create WBP_LoadingScreen for %s"), *Map);
		return;
	}

	// GI ownership and a direct Slate viewport slot survive the old lobby world's cleanup.
	// AddToViewport would register this widget with the outgoing world's UMG cleanup.
	ViewportContent = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))
		.Padding(0.0f)
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		.OnMouseButtonUp_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[LoadingWidget->TakeWidget()];
	if (UProgressBar* Progress = Cast<UProgressBar>(LoadingWidget->GetWidgetFromName(TEXT("Progress_Loading"))))
		Progress->SetIsMarquee(false);
	if (UTextBlock* Percent = Cast<UTextBlock>(LoadingWidget->GetWidgetFromName(TEXT("Text_LoadingPercent"))))
		Percent->SetVisibility(ESlateVisibility::HitTestInvisible);
	LoadingProgress = 0.0f;
	UpdateLoadingProgress(0.0f);
	AttachedViewport = Viewport;
	bPreviousIgnoreInput = Viewport->IgnoreInput();
	Viewport->SetIgnoreInput(true);
	Viewport->AddViewportWidgetContent(ViewportContent.ToSharedRef(), 10000);
	DestinationPackage = Map;
	LoadingStartedAt = FPlatformTime::Seconds();
	FadeCompleteAt = 0.0;
	bGameplayReady = false;
	bLoading = true;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickLoading));
	UE_LOG(LogHBLobbyLoading, Log, TEXT("SHOW | %s | destination=%s"), *GetGameInstance()->GetName(), *DestinationPackage);
}

void UHBLobbyLoadingSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!bLoading || !OwnsWorld(World)) return;
	if (NormalizeMap(World->GetPackage()->GetName()) != DestinationPackage) return;
	if (UGameViewportClient* Viewport = AttachedViewport.Get(); Viewport && ViewportContent.IsValid())
	{
		// Also repair the slot when a non-seamless LoadMap cleared the viewport overlay.
		Viewport->RemoveViewportWidgetContent(ViewportContent.ToSharedRef());
		Viewport->AddViewportWidgetContent(ViewportContent.ToSharedRef(), 10000);
		Viewport->SetIgnoreInput(true);
	}
	UE_LOG(LogHBLobbyLoading, Log, TEXT("MAP ARRIVED | %s | waiting for dungeon/match/pawn"), *GetGameInstance()->GetName());
}

bool UHBLobbyLoadingSubsystem::IsDestinationReady(UWorld* World) const
{
	if (!OwnsWorld(World) || NormalizeMap(World->GetPackage()->GetName()) != DestinationPackage ||
		World->IsInSeamlessTravel() || !World->NextURL.IsEmpty() || !World->HasBegunPlay()) return false;
	const AHeavenBreachGameState* State = World->GetGameState<AHeavenBreachGameState>();
	if (!State || State->GetMatchState() != MatchState::InProgress) return false;
	for (const ULevelStreaming* Level : World->GetStreamingLevels())
	{
		if (!Level) continue;
		if ((Level->ShouldBeLoaded() && !Level->IsLevelLoaded()) || (Level->ShouldBeVisible() && !Level->IsLevelVisible())) return false;
	}
	for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It)
	{
		if (!It->IsReady() || It->GetGenerationId() != State->GetDungeonSnapshot().GenerationId) return false;
	}
	const APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController();
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	return Player && Player->GetWorld() == World && Player->IsLocalController() && IsValid(Pawn) &&
		Pawn->HasActorBegunPlay() && Pawn->GetController() == Player;
}

bool UHBLobbyLoadingSubsystem::TickLoading(float DeltaSeconds)
{
	if (!bLoading) return false;
	UWorld* World = GetGameInstance()->GetWorld();
	const double Now = FPlatformTime::Seconds();
	if (UGameViewportClient* Viewport = AttachedViewport.Get()) Viewport->SetIgnoreInput(true);
	else { FinishLoading(false); return false; }

	if (bGameplayReady)
	{
		if (!IsDestinationReady(World)) { FinishLoading(false); return false; }
		if (Now >= FadeCompleteAt) { FinishLoading(true); return false; }
		return true;
	}
	if (OwnsWorld(World) && NormalizeMap(World->GetPackage()->GetName()) == DestinationPackage)
	{
		for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It)
		{
			if (It->State == EHBDungeonRuntimeState::Failed)
			{
				UE_LOG(LogHBLobbyLoading, Warning, TEXT("Dungeon load failed: %s"), *It->Status.Left(512));
				FinishLoading(false);
				return false;
			}
		}
		if (const AHeavenBreachGameState* State = World->GetGameState<AHeavenBreachGameState>();
			State && State->GetMatchState() == MatchState::Aborted)
		{
			FinishLoading(false);
			return false;
		}
	}
	if (IsDestinationReady(World))
	{
		bGameplayReady = true;
		UpdateLoadingProgress(1.0f);
		float FadeDuration = 0.0f;
		if (const FObjectProperty* Property = FindFProperty<FObjectProperty>(LoadingWidget->GetClass(), TEXT("Anim_LoadingFadeOut")))
		{
			if (const UWidgetAnimation* Animation = Cast<UWidgetAnimation>(Property->GetObjectPropertyValue_InContainer(LoadingWidget)))
				FadeDuration = FMath::Max(0.0f, Animation->GetEndTime() - Animation->GetStartTime());
		}
		if (UFunction* Fade = LoadingWidget->FindFunction(TEXT("PlayLoadingFadeOut")); Fade && Fade->NumParms == 0)
			LoadingWidget->ProcessEvent(Fade, nullptr);
		FadeCompleteAt = Now + FadeDuration;
		UE_LOG(LogHBLobbyLoading, Log, TEXT("READY | %s | dungeon, match and possessed pawn ready | fade=%.3f"), *GetGameInstance()->GetName(), FadeDuration);
	}
	else if (Now - LoadingStartedAt > 150.0)
	{
		// Failure cleanup never marks the destination ready or grants gameplay input.
		UE_LOG(LogHBLobbyLoading, Warning, TEXT("Loading UI timed out while waiting for %s"), *DestinationPackage);
		FinishLoading(false);
		return false;
	}
	else
	{
		UpdateLoadingProgress(CalculateLoadingProgress(World));
	}
	return true;
}

float UHBLobbyLoadingSubsystem::CalculateLoadingProgress(UWorld* World) const
{
	// Stage weights describe completed work, not elapsed time or total disk bytes.
	// Package load: 0..25%, layout/visible rooms: 25..80%, dungeon ready: 90%,
	// server match: 95%, local pawn: 99%. Only IsDestinationReady grants 100%.
	if (!OwnsWorld(World) || NormalizeMap(World->GetPackage()->GetName()) != DestinationPackage)
	{
		const float PackagePercent = GetAsyncLoadPercentage(FName(*DestinationPackage));
		return PackagePercent < 0.0f ? 0.0f : 0.25f * FMath::Clamp(PackagePercent / 100.0f, 0.0f, 1.0f);
	}
	if (!World->HasBegunPlay()) return 0.25f;

	float DungeonProgress = 1.0f;
	const AHeavenBreachGameState* State = World->GetGameState<AHeavenBreachGameState>();
	for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It)
	{
		float Completed = 0.0f;
		if (!It->Rooms.IsEmpty())
		{
			int32 VisibleRooms = 0;
			for (const auto& Room : It->Rooms)
			{
				const ULevel* Level = It->GetLoadedRoomLevel(Room.Id);
				if (Level && Level->bIsVisible) ++VisibleRooms;
			}
			Completed = (0.05f + 0.50f * VisibleRooms / It->Rooms.Num()) / 0.65f;
		}
		if (It->IsReady() && State && It->GetGenerationId() == State->GetDungeonSnapshot().GenerationId)
			Completed = 1.0f;
		DungeonProgress = FMath::Min(DungeonProgress, Completed);
	}

	int32 RequiredLevels = 0;
	float CompletedLevels = 0.0f;
	for (const ULevelStreaming* Level : World->GetStreamingLevels())
	{
		if (!Level || (!Level->ShouldBeLoaded() && !Level->ShouldBeVisible())) continue;
		++RequiredLevels;
		if (Level->IsLevelLoaded())
			CompletedLevels += !Level->ShouldBeVisible() || Level->IsLevelVisible() ? 1.0f : 0.5f;
	}
	if (RequiredLevels > 0)
		DungeonProgress = FMath::Min(DungeonProgress, CompletedLevels / RequiredLevels);

	float Progress = 0.25f + 0.65f * DungeonProgress;
	if (DungeonProgress < 1.0f) return Progress;
	if (!State || State->GetMatchState() != MatchState::InProgress) return Progress;
	Progress = 0.95f;
	const APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController();
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (Player && Player->GetWorld() == World && Player->IsLocalController() && IsValid(Pawn) &&
		Pawn->HasActorBegunPlay() && Pawn->GetController() == Player)
		Progress = 0.99f;
	return Progress;
}

void UHBLobbyLoadingSubsystem::UpdateLoadingProgress(float Progress)
{
	// Requests can be discovered after map arrival; completed stages must not move backwards.
	LoadingProgress = FMath::Max(LoadingProgress, FMath::Clamp(Progress, 0.0f, bGameplayReady ? 1.0f : 0.99f));
	if (!LoadingWidget) return;
	if (UProgressBar* Bar = Cast<UProgressBar>(LoadingWidget->GetWidgetFromName(TEXT("Progress_Loading"))))
		Bar->SetPercent(LoadingProgress);
	if (UTextBlock* Percent = Cast<UTextBlock>(LoadingWidget->GetWidgetFromName(TEXT("Text_LoadingPercent"))))
	{
		const int32 Value = FMath::Min(FMath::RoundToInt(LoadingProgress * 100.0f), bGameplayReady ? 100 : 99);
		Percent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Value)));
	}
}

void UHBLobbyLoadingSubsystem::FinishLoading(bool bSucceeded)
{
	if (TickHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();
	if (UGameViewportClient* Viewport = AttachedViewport.Get())
	{
		if (ViewportContent.IsValid()) Viewport->RemoveViewportWidgetContent(ViewportContent.ToSharedRef());
		Viewport->SetIgnoreInput(bSucceeded ? false : bPreviousIgnoreInput);
	}
	if (bSucceeded)
	{
		if (APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController(); Player && Player->IsLocalController())
		{
			Player->SetInputMode(FInputModeGameOnly());
			Player->SetShowMouseCursor(false);
		}
	}
	if (bLoading) UE_LOG(LogHBLobbyLoading, Log, TEXT("HIDE | %s | success=%d"), *GetGameInstance()->GetName(), bSucceeded);
	ViewportContent.Reset();
	LoadingWidget = nullptr;
	AttachedViewport.Reset();
	DestinationPackage.Reset();
	bLoading = false;
	bGameplayReady = bSucceeded;
}

void UHBLobbyLoadingSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	if (bLoading && OwnsWorld(World))
	{
		UE_LOG(LogHBLobbyLoading, Warning, TEXT("Travel failure: %s"), *Error);
		FinishLoading(false);
	}
}

void UHBLobbyLoadingSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
	if (bLoading && (OwnsWorld(World) || (Driver && OwnsWorld(Driver->GetWorld()))))
	{
		UE_LOG(LogHBLobbyLoading, Warning, TEXT("Network failure: %s"), *Error);
		FinishLoading(false);
	}
}
