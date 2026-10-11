#include "Core/Perf/HBPerfSubsystem.h"

#include "Components/LocalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/TimelineComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"

static TAutoConsoleVariable<int32> CVarHBPerfEnable(
	TEXT("hb.Perf.Enable"),
	1,
	TEXT("1: throttle minimap capture and far torch flicker, 0: original behaviour"));

static TAutoConsoleVariable<float> CVarHBMinimapInterval(
	TEXT("hb.Perf.MinimapInterval"),
	0.066f,
	TEXT("Seconds between minimap scene captures of the local player"));

static TAutoConsoleVariable<float> CVarHBTorchRadius(
	TEXT("hb.Perf.TorchRadius"),
	2500.f,
	TEXT("Torch timelines farther than this from the local camera stop ticking"));

static TAutoConsoleVariable<float> CVarHBTorchTickInterval(
	TEXT("hb.Perf.TorchTickInterval"),
	0.066f,
	TEXT("Tick interval of torch timelines near the local camera"));

static TAutoConsoleVariable<int32> CVarHBTorchShadow(
	TEXT("hb.Perf.TorchShadow"),
	1,
	TEXT("1: only the nearest local lights cast shadows, 0: original shadow settings"));

static TAutoConsoleVariable<float> CVarHBShadowRadius(
	TEXT("hb.Perf.ShadowRadius"),
	1500.f,
	TEXT("Local lights within this distance from the local camera may cast shadows"));

static TAutoConsoleVariable<int32> CVarHBShadowMaxCount(
	TEXT("hb.Perf.ShadowMaxCount"),
	8,
	TEXT("Maximum number of local lights casting shadows at the same time"));

namespace HBPerf
{
	static constexpr float ShadowHysteresis = 300.f;

	static bool IsTorchActor(const AActor* Actor)
	{
		return Actor && Actor->GetClass()->GetName().StartsWith(TEXT("Torch"));
	}
}

void UHBPerfSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UHBPerfSubsystem::HandleLevelAdded);
}

void UHBPerfSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	TorchTimelines.Reset();
	ShadowLights.Reset();
	PendingLevels.Reset();
	Super::Deinitialize();
}

bool UHBPerfSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UHBPerfSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (const ULevel* Level : InWorld.GetLevels())
	{
		ScanLevel(Level);
	}
}

TStatId UHBPerfSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHBPerfSubsystem, STATGROUP_Tickables);
}

void UHBPerfSubsystem::HandleLevelAdded(ULevel* Level, UWorld* World)
{
	if (World == GetWorld() && Level)
	{
		PendingLevels.AddUnique(Level);
		PendingScanTimer = 0.f;
	}
}

void UHBPerfSubsystem::ScanLevel(const ULevel* Level)
{
	if (!Level)
	{
		return;
	}

	for (const AActor* Actor : Level->Actors)
	{
		if (!Actor || Actor->IsA<APawn>())
		{
			continue;
		}

		TInlineComponentArray<ULocalLightComponent*> Lights(Actor);
		for (ULocalLightComponent* Light : Lights)
		{
			if (!Light || !Light->CastShadows || Light->Mobility == EComponentMobility::Static)
			{
				continue;
			}

			const bool bKnown = ShadowLights.ContainsByPredicate([Light](const FShadowLight& Entry)
			{
				return Entry.Light.Get() == Light;
			});
			if (!bKnown)
			{
				FShadowLight& Entry = ShadowLights.AddDefaulted_GetRef();
				Entry.Light = Light;
				Entry.bShadowOn = true;
			}
		}

		if (!HBPerf::IsTorchActor(Actor))
		{
			continue;
		}

		TInlineComponentArray<UTimelineComponent*> Timelines(Actor);
		for (UTimelineComponent* Timeline : Timelines)
		{
			TorchTimelines.AddUnique(Timeline);
		}
	}
}

bool UHBPerfSubsystem::GetLocalView(FVector& OutLocation, APawn*& OutPawn) const
{
	OutPawn = nullptr;
	const UWorld* World = GetWorld();
	APlayerController* PC = (World && GEngine) ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	if (!PC)
	{
		return false;
	}

	OutPawn = PC->GetPawn();
	if (PC->PlayerCameraManager)
	{
		OutLocation = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	if (OutPawn)
	{
		OutLocation = OutPawn->GetActorLocation();
		return true;
	}
	return false;
}

void UHBPerfSubsystem::Tick(float DeltaTime)
{
	const bool bEnabled = CVarHBPerfEnable.GetValueOnGameThread() != 0;

	if (PendingLevels.Num() > 0)
	{
		PendingScanTimer += DeltaTime;
		if (PendingScanTimer >= 0.5f)
		{
			for (const TWeakObjectPtr<ULevel>& Level : PendingLevels)
			{
				ScanLevel(Level.Get());
			}
			PendingLevels.Reset();
			PendingScanTimer = 0.f;
		}
	}

	TorchTimer += DeltaTime;
	if (TorchTimer >= 0.25f || bEnabled != bWasEnabled)
	{
		TorchTimer = 0.f;
		UpdateTorches(bEnabled);
	}

	const bool bShadowEnabled = bEnabled && CVarHBTorchShadow.GetValueOnGameThread() != 0;
	ShadowTimer += DeltaTime;
	if (ShadowTimer >= 0.25f || bShadowEnabled != bWasShadowEnabled)
	{
		ShadowTimer = 0.f;
		UpdateShadows(bShadowEnabled);
	}

	UpdateMinimap(DeltaTime, bEnabled);
	bWasEnabled = bEnabled;
	bWasShadowEnabled = bShadowEnabled;
}

void UHBPerfSubsystem::UpdateTorches(bool bEnabled)
{
	FVector ViewLocation = FVector::ZeroVector;
	APawn* LocalPawn = nullptr;
	const bool bHasView = GetLocalView(ViewLocation, LocalPawn);
	const float RadiusSq = FMath::Square(CVarHBTorchRadius.GetValueOnGameThread());
	const float Interval = FMath::Max(0.f, CVarHBTorchTickInterval.GetValueOnGameThread());

	for (int32 Index = TorchTimelines.Num() - 1; Index >= 0; --Index)
	{
		UTimelineComponent* Timeline = TorchTimelines[Index].Get();
		if (!Timeline)
		{
			TorchTimelines.RemoveAtSwap(Index);
			continue;
		}

		if (!bEnabled)
		{
			Timeline->SetComponentTickInterval(0.f);
			if (Timeline->IsPlaying())
			{
				Timeline->SetComponentTickEnabled(true);
			}
			continue;
		}

		const AActor* Owner = Timeline->GetOwner();
		const bool bNear = bHasView && Owner && FVector::DistSquared(ViewLocation, Owner->GetActorLocation()) <= RadiusSq;
		if (bNear)
		{
			Timeline->SetComponentTickInterval(Interval);
			if (Timeline->IsPlaying() && !Timeline->IsComponentTickEnabled())
			{
				Timeline->SetComponentTickEnabled(true);
			}
		}
		else if (Timeline->IsComponentTickEnabled())
		{
			Timeline->SetComponentTickEnabled(false);
		}
	}
}

void UHBPerfSubsystem::UpdateShadows(bool bEnabled)
{
	FVector ViewLocation = FVector::ZeroVector;
	APawn* LocalPawn = nullptr;
	const bool bHasView = GetLocalView(ViewLocation, LocalPawn);
	const bool bLimit = bEnabled && bHasView;

	const float Radius = FMath::Max(0.f, CVarHBShadowRadius.GetValueOnGameThread());
	const float OnRadiusSq = FMath::Square(Radius);
	const float OffRadiusSq = FMath::Square(Radius + HBPerf::ShadowHysteresis);
	const int32 MaxCount = FMath::Max(0, CVarHBShadowMaxCount.GetValueOnGameThread());

	TArray<TPair<float, int32>> Candidates;
	for (int32 Index = ShadowLights.Num() - 1; Index >= 0; --Index)
	{
		if (!ShadowLights[Index].Light.IsValid())
		{
			ShadowLights.RemoveAtSwap(Index);
		}
	}

	if (bLimit)
	{
		for (int32 Index = 0; Index < ShadowLights.Num(); ++Index)
		{
			const FShadowLight& Entry = ShadowLights[Index];
			const float DistSq = FVector::DistSquared(ViewLocation, Entry.Light->GetComponentLocation());
			if (DistSq <= (Entry.bShadowOn ? OffRadiusSq : OnRadiusSq))
			{
				Candidates.Emplace(DistSq, Index);
			}
		}

		Candidates.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
		{
			return A.Key < B.Key;
		});
		if (Candidates.Num() > MaxCount)
		{
			Candidates.SetNum(MaxCount);
		}
	}

	TBitArray<> WantOn(false, ShadowLights.Num());
	for (const TPair<float, int32>& Candidate : Candidates)
	{
		WantOn[Candidate.Value] = true;
	}

	for (int32 Index = 0; Index < ShadowLights.Num(); ++Index)
	{
		FShadowLight& Entry = ShadowLights[Index];
		const bool bOn = bLimit ? static_cast<bool>(WantOn[Index]) : true;
		if (Entry.bShadowOn != bOn)
		{
			Entry.bShadowOn = bOn;
			Entry.Light->SetCastShadows(bOn);
		}
	}
}

void UHBPerfSubsystem::UpdateMinimap(float DeltaTime, bool bEnabled)
{
	FVector ViewLocation = FVector::ZeroVector;
	APawn* LocalPawn = nullptr;
	GetLocalView(ViewLocation, LocalPawn);

	if (LocalCapturePawn.Get() != LocalPawn)
	{
		LocalCapturePawn = LocalPawn;
		LocalCapture = LocalPawn ? LocalPawn->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
	}

	RemoteCaptureTimer += DeltaTime;
	if (RemoteCaptureTimer >= 1.f)
	{
		RemoteCaptureTimer = 0.f;
		const UWorld* World = GetWorld();
		if (const AGameStateBase* GS = World ? World->GetGameState() : nullptr)
		{
			for (const APlayerState* PS : GS->PlayerArray)
			{
				APawn* Other = PS ? PS->GetPawn() : nullptr;
				if (!Other || Other == LocalPawn)
				{
					continue;
				}

				TInlineComponentArray<USceneCaptureComponent2D*> Captures(Other);
				for (USceneCaptureComponent2D* Capture : Captures)
				{
					Capture->bCaptureEveryFrame = !bEnabled;
					Capture->bCaptureOnMovement = !bEnabled;
				}
			}
		}
	}

	USceneCaptureComponent2D* Capture = LocalCapture.Get();
	if (!Capture)
	{
		return;
	}

	if (!bEnabled)
	{
		Capture->bCaptureEveryFrame = true;
		Capture->bCaptureOnMovement = true;
		return;
	}

	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;

	CaptureTimer += DeltaTime;
	if (CaptureTimer >= CVarHBMinimapInterval.GetValueOnGameThread() && Capture->TextureTarget)
	{
		CaptureTimer = 0.f;
		Capture->CaptureScene();
	}
}
