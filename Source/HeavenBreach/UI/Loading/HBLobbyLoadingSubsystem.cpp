#include "UI/Loading/HBLobbyLoadingSubsystem.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
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
#include "GameFramework/PlayerState.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"

DEFINE_LOG_CATEGORY_STATIC(LogHBLobbyLoading, Log, All);

namespace
{
	class SHBLoadingInputGuard final : public SBorder
	{
	public:
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
		virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
		virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	};

	bool IsStartupMap(const FString& Package)
	{
		return Package == TEXT("/Game/Levels/GameStart/L_Startup");
	}
}

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
	Sessions = Collection.InitializeDependency<UHBOnlineSessionSubsystem>();
	if (Sessions.IsValid())
		Sessions->OnLobbyOperationStateChanged.AddUniqueDynamic(this, &ThisClass::HandleLobbyOperation);
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
	if (Sessions.IsValid())
		Sessions->OnLobbyOperationStateChanged.RemoveDynamic(this, &ThisClass::HandleLobbyOperation);
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

bool UHBLobbyLoadingSubsystem::TravelFromStartup(TSoftObjectPtr<UWorld> Destination)
{
	UWorld* World = GetGameInstance()->GetWorld();
	const FString Package = Destination.ToSoftObjectPath().GetLongPackageName();
	if (!OwnsWorld(World) || !IsStartupMap(NormalizeMap(World->GetPackage()->GetName())) ||
		Package.IsEmpty() || !FPackageName::DoesPackageExist(Package)) return false;
	BeginLoading(World, Package);
	if (!bLoading) return false;
	// Yield before OpenLevel so the loading screen can be painted first.
	PendingStartupPackage = Package;
	return true;
}

void UHBLobbyLoadingSubsystem::HandleLobbyOperation(EHBLobbyOperationState State, const FString& Error)
{
	if (State == EHBLobbyOperationState::Creating && bLoading)
	{
		// The standalone lobby can immediately become a Steam listen hub.
		CancelReadyFade();
	}
	else if (State == EHBLobbyOperationState::OpeningHostMap)
	{
		BeginLoading(GetGameInstance()->GetWorld(), Sessions.IsValid() ? Sessions->GetOwnHubMap() : FString());
	}
	else if (State == EHBLobbyOperationState::Joining || State == EHBLobbyOperationState::ConnectingToLobby)
	{
		BeginLoading(GetGameInstance()->GetWorld(), FString());
		bJoiningSession = true;
		bAwaitingNetworkWorld = true;
		DestinationPackage.Reset();
	}
	else if (State == EHBLobbyOperationState::Leaving)
	{
		const auto HubState = Sessions.IsValid() ? Sessions->GetInviteHubState() : EHBInviteHubState::Disabled;
		// Leaving also reports Steam session cleanup during a local LoadMap, including
		// death -> ExtractionResult. It does not request a new map by itself. Preserve
		// the destination/outgoing world supplied by the actual travel notification.
		if (bLoading) CancelReadyFade();
		else BeginLoading(GetGameInstance()->GetWorld(), Sessions.IsValid() ? Sessions->GetOwnHubMap() : FString());
		bJoiningSession = HubState == EHBInviteHubState::LeavingForInvite || HubState == EHBInviteHubState::JoiningInvite;
		bAwaitingNetworkWorld = bJoiningSession;
	}
	else if (bLoading && bJoiningSession &&
		(State == EHBLobbyOperationState::Failed || State == EHBLobbyOperationState::CleaningUpJoin))
	{
		UE_LOG(LogHBLobbyLoading, Warning, TEXT("Session join ended: %s"), *Error.Left(512));
		FinishLoading(false);
	}
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
	const FURL URL(nullptr, *Destination, TRAVEL_Absolute);
	BeginLoading(GetGameInstance()->GetWorld(), Destination);
	if (!URL.Host.IsEmpty()) bAwaitingNetworkWorld = true;
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
	const FURL URL(nullptr, *Destination, TRAVEL_Absolute);
	const FString Map = Destination.IsEmpty() || !URL.Host.IsEmpty() ? FString() : NormalizeMap(Destination);
	if (IsStartupMap(Map)) return; // The startup GameMode owns its splash sequence.
	if (bLoading)
	{
		// Cancel the fade before OpenLevel can block the ticker. Keep the same opaque
		// overlay through chained hub loads rather than removing and recreating it.
		CancelReadyFade();
		if (!Map.IsEmpty()) DestinationPackage = Map;
		OutgoingWorld = SourceWorld;
		ArrivedWorld.Reset();
		return;
	}

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

	// GI ownership and a direct Slate viewport slot survive the outgoing world's cleanup.
	// AddToViewport would register this widget with the outgoing world's UMG cleanup.
	ViewportContent = SNew(SHBLoadingInputGuard)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor::Black)
		.Padding(0.0f)
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		.OnMouseButtonUp_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[LoadingWidget->TakeWidget()];
	if (const FBoolProperty* Test = FindFProperty<FBoolProperty>(LoadingWidget->GetClass(), TEXT("bStartupProgressTestActive")))
		Test->SetPropertyValue_InContainer(LoadingWidget, false);
	if (UProgressBar* Progress = Cast<UProgressBar>(LoadingWidget->GetWidgetFromName(TEXT("Progress_Loading"))))
		Progress->SetIsMarquee(false);
	if (UTextBlock* Percent = Cast<UTextBlock>(LoadingWidget->GetWidgetFromName(TEXT("Text_LoadingPercent"))))
		Percent->SetVisibility(ESlateVisibility::HitTestInvisible);
	LoadingProgress = 0.0f;
	UpdateLoadingProgress(0.0f);
	AttachedViewport = Viewport;
	bPreviousIgnoreInput = Viewport->IgnoreInput();
	bDestinationIgnoreInput = bPreviousIgnoreInput;
	bPreviousWorldRenderingDisabled = Viewport->bDisableWorldRendering;
	Viewport->bDisableWorldRendering = true;
	PreviousFocus = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetKeyboardFocusedWidget() : nullptr;
	OutgoingWorld = SourceWorld;
	ArrivedWorld.Reset();
	DestinationPackage = Map;
	LoadingStartedAt = FPlatformTime::Seconds();
	FadeCompleteAt = 0.0;
	DestinationReadySince = 0.0;
	bGameplayReady = false;
	bLoading = true;
	AttachLoadingScreen();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickLoading));
	UE_LOG(LogHBLobbyLoading, Log, TEXT("SHOW | %s | destination=%s"), *GetGameInstance()->GetName(), *DestinationPackage);
}

void UHBLobbyLoadingSubsystem::CancelReadyFade()
{
	bGameplayReady = false;
	DestinationReadySince = 0.0;
	FadeCompleteAt = 0.0;
	if (LoadingWidget)
	{
		if (const FObjectProperty* Property = FindFProperty<FObjectProperty>(LoadingWidget->GetClass(), TEXT("Anim_LoadingFadeOut")))
			if (const UWidgetAnimation* Fade = Cast<UWidgetAnimation>(Property->GetObjectPropertyValue_InContainer(LoadingWidget)))
				LoadingWidget->StopAnimation(Fade);
		if (UWidget* Root = LoadingWidget->GetRootWidget()) Root->SetRenderOpacity(1.0f);
		UpdateLoadingProgress(LoadingProgress);
	}
	if (ViewportContent.IsValid())
		StaticCastSharedPtr<SBorder>(ViewportContent)->SetBorderBackgroundColor(FLinearColor::Black);
	if (UGameViewportClient* Viewport = AttachedViewport.Get()) Viewport->bDisableWorldRendering = true;
}

void UHBLobbyLoadingSubsystem::AttachLoadingScreen()
{
	if (UGameViewportClient* Viewport = AttachedViewport.Get(); Viewport && ViewportContent.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(ViewportContent.ToSharedRef());
		Viewport->AddViewportWidgetContent(ViewportContent.ToSharedRef(), 10000);
		Viewport->SetIgnoreInput(true);
		if (FSlateApplication::IsInitialized())
			FSlateApplication::Get().SetKeyboardFocus(ViewportContent, EFocusCause::SetDirectly);
	}
}

void UHBLobbyLoadingSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!bLoading || !OwnsWorld(World)) return;
	const FString Package = NormalizeMap(World->GetPackage()->GetName());
	if (IsStartupMap(Package) || (bAwaitingNetworkWorld && World->GetNetMode() != NM_Client)) return;
	if (!DestinationPackage.IsEmpty() && Package != DestinationPackage) return;
	DestinationPackage = Package;
	ArrivedWorld = World;
	if (UGameViewportClient* Viewport = AttachedViewport.Get()) bDestinationIgnoreInput = Viewport->IgnoreInput();
	AttachLoadingScreen();
	UE_LOG(LogHBLobbyLoading, Log, TEXT("MAP ARRIVED | %s | waiting for destination readiness | %s"), *GetGameInstance()->GetName(), *Package);
}

bool UHBLobbyLoadingSubsystem::HasArrived(UWorld* World) const
{
	if (!OwnsWorld(World) || World == OutgoingWorld.Get() || !World->HasBegunPlay() ||
		World->IsInSeamlessTravel() || !World->NextURL.IsEmpty() ||
		(bAwaitingNetworkWorld && World->GetNetMode() != NM_Client)) return false;
	return !DestinationPackage.IsEmpty() && NormalizeMap(World->GetPackage()->GetName()) == DestinationPackage;
}

bool UHBLobbyLoadingSubsystem::IsSessionTransitionPending() const
{
	if (!Sessions.IsValid()) return false;
	const auto State = Sessions->GetLobbyOperationState();
	const auto Hub = Sessions->GetInviteHubState();
	return State == EHBLobbyOperationState::Creating || State == EHBLobbyOperationState::OpeningHostMap ||
		State == EHBLobbyOperationState::Joining || State == EHBLobbyOperationState::ConnectingToLobby ||
		State == EHBLobbyOperationState::Leaving || State == EHBLobbyOperationState::CleaningUpJoin ||
		State == EHBLobbyOperationState::CleaningUpHost ||
		Hub == EHBInviteHubState::LeavingForInvite || Hub == EHBInviteHubState::JoiningInvite ||
		Hub == EHBInviteHubState::PreparingHost ||
		(bJoiningSession && State != EHBLobbyOperationState::InLobby);
}

UUserWidget* UHBLobbyLoadingSubsystem::ReadWidgetProperty(const UObject* Object, FName Name)
{
	const FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
	return Property ? Cast<UUserWidget>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
}

bool UHBLobbyLoadingSubsystem::IsMenuReady(const APlayerController* Player, UWorld* World) const
{
	if (IsSessionTransitionPending()) return false;
	if (IsLobbyWorld(World) || FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("LobbyWidget")))
	{
		if (!IsValid(Player->PlayerState)) return false;
		// Read the existing reflected UI reference without changing the owning controller.
		if (FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("LobbyWidget")))
		{
			const UUserWidget* Widget = ReadWidgetProperty(Player, TEXT("LobbyWidget"));
			if (IsValid(Widget) && Widget->IsInViewport()) return true;
			// The isolated multiplayer test controller intentionally uses its own Screen.
			const UUserWidget* TestScreen = ReadWidgetProperty(Player, TEXT("Screen"));
			return IsValid(TestScreen) && TestScreen->IsInViewport();
		}
	}
	if (DestinationPackage.Contains(TEXT("ExtractionResult")))
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UUserWidget::StaticClass(), true);
		return Widgets.ContainsByPredicate([this, Player](const UUserWidget* Widget)
		{
			return Widget != LoadingWidget && Widget->GetOwningPlayer() == Player && Widget->IsInViewport();
		});
	}
	return true;
}

bool UHBLobbyLoadingSubsystem::IsDestinationReady(UWorld* World) const
{
	if (!HasArrived(World) || !World->GetGameState()) return false;
	const AHeavenBreachGameState* State = World->GetGameState<AHeavenBreachGameState>();
	for (const ULevelStreaming* Level : World->GetStreamingLevels())
	{
		if (!Level) continue;
		if ((Level->ShouldBeLoaded() && !Level->IsLevelLoaded()) || (Level->ShouldBeVisible() && !Level->IsLevelVisible())) return false;
	}
	const APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController();
	if (!Player || Player->GetWorld() != World || !Player->IsLocalController() || !Player->HasActorBegunPlay()) return false;
	if (!State || DestinationPackage.Contains(TEXT("ExtractionResult"))) return IsMenuReady(Player, World);
	if (State->GetMatchState() != MatchState::InProgress) return false;
	for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It)
	{
		if (!It->IsReady() || It->GetGenerationId() != State->GetDungeonSnapshot().GenerationId) return false;
	}
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	return Player && Player->GetWorld() == World && Player->IsLocalController() && IsValid(Pawn) &&
		Pawn->HasActorBegunPlay() && Pawn->GetController() == Player;
}

bool UHBLobbyLoadingSubsystem::TickLoading(float DeltaSeconds)
{
	if (!bLoading) return false;
	UWorld* World = GetGameInstance()->GetWorld();
	const double Now = FPlatformTime::Seconds();
	if (UGameViewportClient* Viewport = AttachedViewport.Get())
	{
		if (HasArrived(World) && ArrivedWorld.Get() != World)
		{
			// Seamless travel does not emit PostLoadMapWithWorld.
			bDestinationIgnoreInput = Viewport->IgnoreInput();
			ArrivedWorld = World;
			AttachLoadingScreen();
		}
		Viewport->SetIgnoreInput(true);
	}
	else { FinishLoading(false); return false; }
	if (!PendingStartupPackage.IsEmpty())
	{
		if (Now - LoadingStartedAt >= 0.15)
		{
			const FString Target = MoveTemp(PendingStartupPackage);
			PendingStartupPackage.Reset();
			UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Target), true);
		}
		return bLoading;
	}

	if (bGameplayReady)
	{
		if (IsDestinationReady(World))
		{
			if (Now >= FadeCompleteAt) { FinishLoading(true); return false; }
			return true;
		}
		// A chained hub/session operation can begin during the destination fade.
		CancelReadyFade();
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
			State && State->GetMatchState() == MatchState::Aborted && !DestinationPackage.Contains(TEXT("ExtractionResult")))
		{
			FinishLoading(false);
			return false;
		}
	}
	if (IsDestinationReady(World) && Now - LoadingStartedAt >= 0.15)
	{
		// Let the existing 0.1s invite-hub ticker discover an automatic host transition
		// before exposing the first standalone lobby. This is a stability gate, not progress.
		if (DestinationReadySince == 0.0) DestinationReadySince = Now;
		if (Now - DestinationReadySince < 0.25)
		{
			UpdateLoadingProgress(CalculateLoadingProgress(World));
			return true;
		}
		bGameplayReady = true;
		UpdateLoadingProgress(1.0f);
		if (UGameViewportClient* Viewport = AttachedViewport.Get())
			Viewport->bDisableWorldRendering = bPreviousWorldRenderingDisabled;
		if (ViewportContent.IsValid())
			StaticCastSharedPtr<SBorder>(ViewportContent)->SetBorderBackgroundColor(FLinearColor::Transparent);
		float FadeDuration = 0.0f;
		if (const FObjectProperty* Property = FindFProperty<FObjectProperty>(LoadingWidget->GetClass(), TEXT("Anim_LoadingFadeOut")))
		{
			if (const UWidgetAnimation* Animation = Cast<UWidgetAnimation>(Property->GetObjectPropertyValue_InContainer(LoadingWidget)))
				FadeDuration = FMath::Max(0.0f, Animation->GetEndTime() - Animation->GetStartTime());
		}
		if (UFunction* Fade = LoadingWidget->FindFunction(TEXT("PlayLoadingFadeOut")); Fade && Fade->NumParms == 0)
			LoadingWidget->ProcessEvent(Fade, nullptr);
		FadeCompleteAt = Now + FadeDuration;
		UE_LOG(LogHBLobbyLoading, Log, TEXT("READY | %s | destination ready | %s | fade=%.3f"), *GetGameInstance()->GetName(), *DestinationPackage, FadeDuration);
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
		DestinationReadySince = 0.0;
		UpdateLoadingProgress(CalculateLoadingProgress(World));
	}
	return true;
}

float UHBLobbyLoadingSubsystem::CalculateLoadingProgress(UWorld* World) const
{
	// Stage weights describe completed work, not elapsed time or total disk bytes.
	// Package load: 0..25%, layout/visible rooms: 25..80%, dungeon ready: 90%,
	// server match: 95%, local pawn: 99%. Only IsDestinationReady grants 100%.
	if (!HasArrived(World))
	{
		if (DestinationPackage.IsEmpty())
			return Sessions.IsValid() && Sessions->GetLobbyOperationState() == EHBLobbyOperationState::ConnectingToLobby ? 0.15f : 0.0f;
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
	if (!State || DestinationPackage.Contains(TEXT("ExtractionResult")))
	{
		float Progress = 0.25f + 0.60f * DungeonProgress;
		if (IsSessionTransitionPending()) return Progress;
		const APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController();
		if (Player && Player->GetWorld() == World && Player->HasActorBegunPlay()) Progress = 0.95f;
		if (Player && IsMenuReady(Player, World)) Progress = 0.99f;
		return Progress;
	}

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
	LoadingProgress = FMath::Clamp(FMath::Max(LoadingProgress, Progress), 0.0f, bGameplayReady ? 1.0f : 0.99f);
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
		Viewport->SetIgnoreInput(bSucceeded ? bDestinationIgnoreInput : bPreviousIgnoreInput);
		Viewport->bDisableWorldRendering = bPreviousWorldRenderingDisabled;
	}
	if (bSucceeded)
	{
		if (APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController(); Player && Player->IsLocalController())
		{
			UWorld* World = Player->GetWorld();
			if (World->GetGameState<AHeavenBreachGameState>() && !DestinationPackage.Contains(TEXT("ExtractionResult")))
			{
				Player->SetInputMode(FInputModeGameOnly());
				Player->SetShowMouseCursor(false);
			}
			else if (IsLobbyWorld(World) || DestinationPackage.Contains(TEXT("ExtractionResult")))
			{
				Player->SetInputMode(FInputModeUIOnly());
				Player->SetShowMouseCursor(true);
			}
		}
	}
	if (bLoading) UE_LOG(LogHBLobbyLoading, Log, TEXT("HIDE | %s | success=%d"), *GetGameInstance()->GetName(), bSucceeded);
	if (FSlateApplication::IsInitialized() && ViewportContent.IsValid() && ViewportContent->HasKeyboardFocus())
	{
		if (bSucceeded) FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
		else if (TSharedPtr<SWidget> Focus = PreviousFocus.Pin()) FSlateApplication::Get().SetKeyboardFocus(Focus);
	}
	ViewportContent.Reset();
	LoadingWidget = nullptr;
	AttachedViewport.Reset();
	DestinationPackage.Reset();
	PendingStartupPackage.Reset();
	OutgoingWorld.Reset();
	ArrivedWorld.Reset();
	PreviousFocus.Reset();
	bJoiningSession = false;
	bAwaitingNetworkWorld = false;
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
		// Existing session recovery may already have queued a return to the local hub.
		if (Sessions.IsValid() && Sessions->GetLobbyOperationState() == EHBLobbyOperationState::Leaving)
		{
			bJoiningSession = false;
			bAwaitingNetworkWorld = false;
			return;
		}
		FinishLoading(false);
	}
}
