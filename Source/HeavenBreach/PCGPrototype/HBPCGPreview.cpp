#include "PCGPrototype/HBPCGPreview.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

DEFINE_LOG_CATEGORY_STATIC(LogHBPCGPreview, Log, All);

AHBPCGExplorerPawn::AHBPCGExplorerPawn()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->FieldOfView = 90;
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = 0;
	Camera->PostProcessSettings.bOverride_BloomIntensity = true;
	Camera->PostProcessSettings.BloomIntensity = 0;
	InspectionLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("InspectionLight"));
	InspectionLight->SetupAttachment(Camera);
	InspectionLight->SetIntensityUnits(ELightUnits::Lumens);
	InspectionLight->SetIntensity(1000);
	InspectionLight->SetAttenuationRadius(6000);
	InspectionLight->SetInnerConeAngle(30);
	InspectionLight->SetOuterConeAngle(65);
}

void AHBPCGExplorerPawn::Fly(FVector LocalInput, FVector2D MouseDelta, float DeltaSeconds, bool bFast)
{
	if (DeltaSeconds <= 0 || !FMath::IsFinite(DeltaSeconds)) return;
	FRotator Look = GetActorRotation();
	Look.Yaw += MouseDelta.X * 0.12;
	Look.Pitch = FMath::Clamp(Look.Pitch + MouseDelta.Y * 0.12, -89.0, 89.0);
	Look.Roll = 0;
	SetActorRotation(Look);
	const FVector Direction = GetActorForwardVector()*LocalInput.X + GetActorRightVector()*LocalInput.Y + FVector::UpVector*LocalInput.Z;
	// No sweep and no collision primitive: doors, walls and ceilings never block inspection.
	AddActorWorldOffset(Direction.GetClampedToMaxSize(1)*MoveSpeed*(bFast ? 5.f : 1.f)*DeltaSeconds, false);
}

AHBPCGPreviewGameMode::AHBPCGPreviewGameMode()
{
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	HUDClass = AHBPCGPreviewHUD::StaticClass();
}

void AHBPCGPreviewGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// The generator creates and possesses the inspector only after all modules load.
	for (TActorIterator<AHBPCGPreviewGenerator> It(GetWorld()); It; ++It)
	{
		NewPlayer->bAutoManageActiveCameraTarget = false;
		NewPlayer->SetViewTarget(*It);
		break;
	}
}

void AHBPCGPreviewHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	for (TActorIterator<AHBPCGPreviewGenerator> It(GetWorld()); It; ++It)
	{
		const auto* G = *It;
		if (!G->bExploring)
		{
			// Screen-space annotations remain legible at the kilometre-wide overview.
			TArray<FBox2D> Boxes;
			for (const auto& R : G->Rooms)
			{
				FBox2D Box(ForceInit);
				for (int32 X = 0; X < 2; ++X) for (int32 Y = 0; Y < 2; ++Y)
				{
					const FVector P = Project(FVector(X ? R.Bounds.Max.X : R.Bounds.Min.X,
						Y ? R.Bounds.Max.Y : R.Bounds.Min.Y, R.Bounds.Max.Z));
					Box += FVector2D(P.X, P.Y);
				}
				Boxes.Add(Box);
			}
			for (int32 I = 0; I < G->Rooms.Num(); ++I)
			{
				const auto& R = G->Rooms[I];
				if (!Boxes.IsValidIndex(R.ParentId)) continue;
				const FBox2D& From = Boxes[R.ParentId];
				const FBox2D& To = Boxes[I];
				const FVector2D Direction = (To.GetCenter() - From.GetCenter()).GetSafeNormal();
				auto EdgeDistance = [&](const FBox2D& Box)
				{
					const FVector2D E = Box.GetExtent() + FVector2D(5, 5);
					return FMath::Min(E.X / FMath::Max(FMath::Abs(Direction.X), 0.0001),
						E.Y / FMath::Max(FMath::Abs(Direction.Y), 0.0001));
				};
				const FVector2D A = From.GetCenter() + Direction * EdgeDistance(From);
				const FVector2D B = To.GetCenter() - Direction * EdgeDistance(To);
				const FLinearColor C = G->Rooms[R.ParentId].Floor != R.Floor ? FLinearColor(1,0.2f,1) :
					R.bBranch ? FLinearColor(1,0.8f,0.15f) : FLinearColor(0.75f,0.85f,1);
				DrawLine(A.X,A.Y,B.X,B.Y,C,2);
				const FVector2D Side(-Direction.Y, Direction.X);
				for (int32 Sign : {-1, 1})
				{
					const FVector2D Tip = B - Direction * 9 + Side * (Sign * 5);
					DrawLine(B.X,B.Y,Tip.X,Tip.Y,C,2);
				}
			}
			for (int32 I = 0; I < G->Rooms.Num(); ++I)
			{
				const auto& R = G->Rooms[I];
				const auto& B = Boxes[I];
				const FLinearColor C(HBDungeonLayout::RoleColor(R.Role));
				DrawLine(B.Min.X,B.Min.Y,B.Max.X,B.Min.Y,C,2);
				DrawLine(B.Max.X,B.Min.Y,B.Max.X,B.Max.Y,C,2);
				DrawLine(B.Max.X,B.Max.Y,B.Min.X,B.Max.Y,C,2);
				DrawLine(B.Min.X,B.Max.Y,B.Min.X,B.Min.Y,C,2);
				DrawText(FString::Printf(TEXT("F%d R%02d %s"),R.Floor,R.Id,HBDungeonLayout::RoleName(R.Role)),
					C,B.Min.X,B.Max.Y+4,nullptr,0.9f);
			}
		}
		DrawRect(FLinearColor(0.01f,0.015f,0.025f,0.9f), 12, 12, 960, 153);
		DrawText(FString::Printf(TEXT("PCG MODULE PREVIEW | Seed %d | %d/%d loaded"), G->ActiveSeed, G->LoadedRooms, G->Rooms.Num()), FLinearColor::White, 24, 22, nullptr, 1.35f);
		DrawText(G->Status, G->bReady ? FLinearColor::Green : FLinearColor::Yellow, 24, 50);
		DrawText(G->bExploring ? TEXT("EXPLORE | Mouse: look  WASD: fly  Q/E: down/up  Shift: fast  Tab: overview") :
			TEXT("OVERVIEW | Tab: explore  1: all  2: floor 1  3: floor 2  WASD: pan  Q/E: zoom"), FLinearColor::White, 24, 74);
		DrawText(TEXT("R: new seed  T: replay seed  Home: starting room  PgUp/PgDn: previous/next room"), FLinearColor::White, 24, 96);
		DrawText(TEXT("Lines = planned links. Door openings, boss corridor and stairs are NOT generated."), FLinearColor(1,0.65f,0.25f), 24, 118);
		DrawText(G->bExploring ? FString::Printf(TEXT("NO COLLISION | Room shortcut: R%02d | Walls and doors can be crossed freely."),G->InspectionRoom) :
			TEXT("Blue: adventure  Green: recovery  Gold: upgrade  Red: boss"),FLinearColor(0.4f,0.85f,1),24,140);
		break;
	}
}

AHBPCGPreviewGenerator::AHBPCGPreviewGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	auto* GenerationBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("GenerationBounds"));
	GenerationBounds->SetupAttachment(RootComponent);
	GenerationBounds->InitBoxExtent(FVector(180000, 80000, 15000));
	GenerationBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GenerationBounds->SetHiddenInGame(true);
	PCG = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
	PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
#if WITH_EDITORONLY_DATA
	PCG->bRegenerateInEditor = false;
#endif
	PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
	PreviewCamera->SetupAttachment(RootComponent);
	PreviewCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	PreviewCamera->SetAutoCalculateOrthoPlanes(false);
	PreviewCamera->SetOrthoNearClipPlane(10);
	PreviewCamera->SetOrthoFarClipPlane(200000);
	PreviewCamera->OrthoWidth = 180000;
	PreviewCamera->bOverrideAspectRatioAxisConstraint = true;
	PreviewCamera->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	PreviewCamera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PreviewCamera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	PreviewCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
	PreviewCamera->PostProcessSettings.AutoExposureBias = 5;
	const HBDungeonLayout::FSettings Defaults;
	AdventureBounds = Defaults.AdventureBounds;
	BossBounds = Defaults.BossBounds;
}

void AHBPCGPreviewGenerator::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_Standalone) { Fail(TEXT("This visual prototype supports Standalone/one-player PIE only.")); return; }
	SetPreviewView(0);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->bAutoManageActiveCameraTarget = false;
		PC->SetViewTarget(this);
	}
	Regenerate(bRandomSeedOnStart);
}

void AHBPCGPreviewGenerator::Regenerate(bool bNewSeed)
{
	if (!GetWorld() || !GetWorld()->IsGameWorld() || GetNetMode() != NM_Standalone) return;
	if (PCG->IsGenerating() || bAwaitingGeneration || bRegeneratePending) return;
	bNextSeedIsNew = bNewSeed;
	bRegeneratePending = true;
	GenerationStartTime = FPlatformTime::Seconds();
	ClearGenerated();
	PCG->CleanupLocalImmediate(true);
	Status = TEXT("Unloading previous modules...");
}

void AHBPCGPreviewGenerator::BeginGeneration()
{
	bRegeneratePending = false;
	if (!Graph || AdventureModule.IsNull() || BossModule.IsNull()) { Fail(TEXT("Missing PCG graph or module map references.")); return; }
	ActiveSeed = bNextSeedIsNew ? static_cast<int32>(FGuid::NewGuid().A & 0x7fffffff) : Seed;
	Seed = ActiveSeed;
	++GenerationSerial;
	GenerationStartTime = FPlatformTime::Seconds();
	bAwaitingGeneration = true;
	PCG->Seed = ActiveSeed;
	PCG->SetGraphLocal(Graph);
	Status = TEXT("PCG: planning and validating layout...");
	PCG->GenerateLocal(true);
}

bool AHBPCGPreviewGenerator::PlanLayout()
{
	// Editor property notifications can execute the graph outside our explicit
	// regeneration lifecycle. They must not replace an already loaded layout.
	if (!bAwaitingGeneration || !Streams.IsEmpty()) return false;
	HBDungeonLayout::FSettings S;
	S.Seed = ActiveSeed; S.MaxAttempts = MaxAttempts; S.CellSize = CellSize; S.FloorHeight = FloorHeight;
	S.AdventureBounds = AdventureBounds; S.BossBounds = BossBounds;
	FString Error;
	if (!HBDungeonLayout::Generate(S, Rooms, Error)) { Fail(Error); return false; }
	// Keep planner coordinates local so changing the generator position is supported.
	for (auto& R : Rooms)
	{
		R.Transform = R.Transform * GetActorTransform();
		R.Bounds = R.Bounds.TransformBy(GetActorTransform());
	}
	UE_LOG(LogHBPCGPreview, Display, TEXT("PLAN Seed=%d Rooms=%d Signature=%s"), ActiveSeed, Rooms.Num(), *GetLayoutSignature());
	return true;
}

void AHBPCGPreviewGenerator::MaterializeLayout(int32 PointCount)
{
	if (!bAwaitingGeneration || !Streams.IsEmpty()) return;
	if (Rooms.IsEmpty() || PointCount != Rooms.Num()) { Fail(TEXT("PCG room point data does not match the layout.")); return; }
	FString Error;
	if (!HBDungeonLayout::Validate(Rooms, Error)) { Fail(Error); return; }
	for (const auto& Room : Rooms)
	{
		bool bSuccess = false;
		const FString Name = FString::Printf(TEXT("HBPreview_%s_G%d_R%d"), *GetName(), GenerationSerial, Room.Id);
		ULevelStreamingDynamic* Stream = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this,
			Room.Role == EHBPrototypeRoomRole::Boss ? BossModule : AdventureModule, Room.Transform, bSuccess, Name);
		if (!bSuccess || !Stream) { Fail(FString::Printf(TEXT("Could not load module for room %d."), Room.Id)); return; }
		Streams.Add(Stream);
	}
	Status = TEXT("Loading all room modules...");
}

void AHBPCGPreviewGenerator::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!GetWorld()->IsGameWorld()) return;
	RetiringStreams.RemoveAll([this](const TObjectPtr<ULevelStreamingDynamic>& S)
	{
		return !IsValid(S) || !GetWorld()->GetStreamingLevels().Contains(S);
	});
	if (bRegeneratePending && RetiringStreams.IsEmpty()) BeginGeneration();
	else if (bRegeneratePending && FPlatformTime::Seconds()-GenerationStartTime > LoadTimeoutSeconds)
		Fail(TEXT("Previous module unloading timed out."));
	if (bAwaitingGeneration)
	{
		LoadedRooms = 0;
		for (const auto& S : Streams) if (IsValid(S) && S->IsLevelLoaded() && S->IsLevelVisible()) ++LoadedRooms;
		if (!Streams.IsEmpty() && LoadedRooms == Rooms.Num() && !PCG->IsGenerating())
		{
			bAwaitingGeneration = false; bReady = true;
			Status = TEXT("READY - layout valid; all modules visible. Connections are annotations only.");
			UE_LOG(LogHBPCGPreview, Display, TEXT("READY Seed=%d Rooms=%d Loaded=%d Elapsed=%.2fs"), ActiveSeed, Rooms.Num(), LoadedRooms, FPlatformTime::Seconds()-GenerationStartTime);
			if (bStartInExploration) FocusRoom(0);
		}
		else if (FPlatformTime::Seconds()-GenerationStartTime > LoadTimeoutSeconds) Fail(TEXT("Generation/loading timed out. Check module references and PCG graph."));
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->WasInputKeyJustPressed(EKeys::R)) Regenerate(true);
		if (PC->WasInputKeyJustPressed(EKeys::T)) Regenerate(false);
		if (PC->WasInputKeyJustPressed(EKeys::One)) SetPreviewView(0);
		if (PC->WasInputKeyJustPressed(EKeys::Two)) SetPreviewView(1);
		if (PC->WasInputKeyJustPressed(EKeys::Three)) SetPreviewView(2);
		if (PC->WasInputKeyJustPressed(EKeys::Tab)) SetExploration(!bExploring);
		if (PC->WasInputKeyJustPressed(EKeys::Home)) FocusRoom(0);
		if (PC->WasInputKeyJustPressed(EKeys::PageUp)) FocusRoom(InspectionRoom-1);
		if (PC->WasInputKeyJustPressed(EKeys::PageDown)) FocusRoom(InspectionRoom+1);
		if (bExploring && IsValid(Explorer))
		{
			const FVector Move((PC->IsInputKeyDown(EKeys::W) ? 1 : 0)-(PC->IsInputKeyDown(EKeys::S) ? 1 : 0),
				(PC->IsInputKeyDown(EKeys::D) ? 1 : 0)-(PC->IsInputKeyDown(EKeys::A) ? 1 : 0),
				(PC->IsInputKeyDown(EKeys::E) ? 1 : 0)-(PC->IsInputKeyDown(EKeys::Q) ? 1 : 0));
			float MouseX=0, MouseY=0;
			PC->GetInputMouseDelta(MouseX, MouseY);
			Explorer->Fly(Move,FVector2D(MouseX,MouseY),Dt,PC->IsInputKeyDown(EKeys::LeftShift)||PC->IsInputKeyDown(EKeys::RightShift));
			return;
		}
		FVector Pan = FVector::ZeroVector;
		if (PC->IsInputKeyDown(EKeys::W)) Pan.Y += 1;
		if (PC->IsInputKeyDown(EKeys::S)) Pan.Y -= 1;
		if (PC->IsInputKeyDown(EKeys::D)) Pan.X += 1;
		if (PC->IsInputKeyDown(EKeys::A)) Pan.X -= 1;
		PreviewCamera->AddWorldOffset(Pan * Dt * PreviewCamera->OrthoWidth * 0.35f);
		const float Zoom = (PC->IsInputKeyDown(EKeys::E) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Q) ? 1.f : 0.f);
		PreviewCamera->OrthoWidth = FMath::Clamp(PreviewCamera->OrthoWidth * (1+Zoom*Dt), 6000.f, 300000.f);
	}
}

void AHBPCGPreviewGenerator::SetPreviewView(int32 Floor)
{
	SetExploration(false);
	const FVector Center(Floor == 1 ? CellSize*1.5 : Floor == 2 ? CellSize*7.5 : CellSize*4.5, CellSize*1.5, 0);
	PreviewCamera->SetWorldLocation(GetActorTransform().TransformPosition(Center + FVector(0,0,100000)));
	PreviewCamera->SetWorldRotation(FRotator(-90,-90,0));
	PreviewCamera->OrthoWidth = Floor == 0 ? CellSize*11.5f : CellSize*5.5f;
}

void AHBPCGPreviewGenerator::SetExploration(bool bEnabled)
{
	if (!GetWorld() || !GetWorld()->IsGameWorld()) return;
	if (bEnabled && !bReady) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;
	if (bEnabled && !IsValid(Explorer))
	{
		Explorer = GetWorld()->SpawnActor<AHBPCGExplorerPawn>();
		if (!Explorer) { Status = TEXT("Could not create inspection pawn."); return; }
		InspectionRoom = 0;
		Explorer->SetActorLocation(Rooms[0].Transform.TransformPosition(AdventureEntry), false);
		Explorer->SetActorRotation(Rooms[0].Transform.TransformVectorNoScale(FVector(-AdventureEntry.X,-AdventureEntry.Y,0)).Rotation());
		PC->Possess(Explorer);
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	bExploring = bEnabled;
	if (IsValid(Explorer)) Explorer->InspectionLight->SetVisibility(bEnabled);
	PC->bAutoManageActiveCameraTarget = false;
	PC->SetViewTarget(bEnabled ? static_cast<AActor*>(Explorer) : this);
}

void AHBPCGPreviewGenerator::FocusRoom(int32 RoomIndex)
{
	if (!bReady || Rooms.IsEmpty()) return;
	SetExploration(true);
	if (!IsValid(Explorer)) return;
	InspectionRoom = (RoomIndex % Rooms.Num() + Rooms.Num()) % Rooms.Num();
	const auto& Room = Rooms[InspectionRoom];
	const FVector Entry = Room.Role == EHBPrototypeRoomRole::Boss ? BossEntry : AdventureEntry;
	Explorer->SetActorLocation(Room.Transform.TransformPosition(Entry), false);
	Explorer->SetActorRotation(Room.Transform.TransformVectorNoScale(FVector(-Entry.X,-Entry.Y,0)).Rotation());
}

void AHBPCGPreviewGenerator::ClearGenerated()
{
	SetExploration(false);
	bReady = false; LoadedRooms = 0;
	for (auto& S : Streams) if (IsValid(S))
	{
		S->SetShouldBeVisible(false); S->SetShouldBeLoaded(false); S->SetIsRequestingUnloadAndRemoval(true);
		RetiringStreams.AddUnique(S);
	}
	Streams.Reset();
	Rooms.Reset();
}

void AHBPCGPreviewGenerator::Fail(const FString& Reason)
{
	bAwaitingGeneration = false; bRegeneratePending = false;
	ClearGenerated();
	Status = TEXT("FAILED: ") + Reason;
	UE_LOG(LogHBPCGPreview, Error, TEXT("%s"), *Status);
}

void AHBPCGPreviewGenerator::EndPlay(const EEndPlayReason::Type Reason)
{
	bAwaitingGeneration = false; bRegeneratePending = false;
	ClearGenerated();
	if (IsValid(Explorer)) Explorer->Destroy();
	Explorer = nullptr;
	Super::EndPlay(Reason);
}

FString AHBPCGPreviewGenerator::GetLayoutSignature() const { return HBDungeonLayout::Signature(Rooms); }

FString AHBPCGPreviewGenerator::GetVerificationSummary() const
{
	FString Error;
	const bool bValid = HBDungeonLayout::Validate(Rooms, Error);
	int32 PawnCount = 0;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It) ++PawnCount;
	return FString::Printf(TEXT("Seed=%d Valid=%d Ready=%d Rooms=%d Loaded=%d Streams=%d Retiring=%d Pawns=%d Error=%s"),
		ActiveSeed, bValid, bReady, Rooms.Num(), LoadedRooms, Streams.Num(), RetiringStreams.Num(), PawnCount, *Error);
}
