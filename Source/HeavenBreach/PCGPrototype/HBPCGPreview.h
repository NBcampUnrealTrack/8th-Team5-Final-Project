#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "PCGPrototype/HBDungeonLayout.h"
#include "HBPCGPreview.generated.h"

class UPCGComponent;
class UPCGGraphInterface;
class ULevelStreamingDynamic;
class UCameraComponent;
class USpotLightComponent;

/** Collision-free inspection camera; deliberately has no gameplay components. */
UCLASS()
class HEAVENBREACH_API AHBPCGExplorerPawn : public APawn
{
	GENERATED_BODY()
public:
	AHBPCGExplorerPawn();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exploration") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exploration") TObjectPtr<USpotLightComponent> InspectionLight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exploration") float MoveSpeed = 1200;
	UFUNCTION(BlueprintCallable, Category="Exploration") void Fly(FVector LocalInput, FVector2D MouseDelta, float DeltaSeconds, bool bFast);
};

/** Preview only: no gameplay interactions or multiplayer generation. */
UCLASS()
class HEAVENBREACH_API AHBPCGPreviewGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AHBPCGPreviewGameMode();
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};

UCLASS()
class HEAVENBREACH_API AHBPCGPreviewHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};

UCLASS(BlueprintType, Blueprintable)
class HEAVENBREACH_API AHBPCGPreviewGenerator : public AActor
{
	GENERATED_BODY()
public:
	AHBPCGPreviewGenerator();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview") TObjectPtr<UPCGComponent> PCG;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview") TObjectPtr<UCameraComponent> PreviewCamera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview") TObjectPtr<UPCGGraphInterface> Graph;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Modules") TSoftObjectPtr<UWorld> AdventureModule;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Modules") TSoftObjectPtr<UWorld> BossModule;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Modules") FBox AdventureBounds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Modules") FBox BossBounds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Generation") bool bRandomSeedOnStart = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Generation") int32 Seed = 12345;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Generation", meta=(ClampMin="1",ClampMax="1024")) int32 MaxAttempts = 64;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Generation", meta=(ClampMin="15000")) float CellSize = 16000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Generation", meta=(ClampMin="3000")) float FloorHeight = 5000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Generation", meta=(ClampMin="5")) float LoadTimeoutSeconds = 120;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Exploration") bool bStartInExploration = true;
	// Original PlayerStart positions, adjusted for module recentering and eye height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Exploration") FVector AdventureEntry = FVector(1867.125,3100,160);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview|Exploration") FVector BossEntry = FVector(2663,3045.5,985);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview|Exploration") TObjectPtr<AHBPCGExplorerPawn> Explorer;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview|Exploration") bool bExploring = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview|Exploration") int32 InspectionRoom = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview|Result") FString Status = TEXT("Press Play: generates modules, then enters exploration.");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview|Result") int32 ActiveSeed = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview|Result") TArray<FHBPrototypeRoom> Rooms;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview|Result") int32 LoadedRooms = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview|Result") bool bReady = false;

	UFUNCTION(BlueprintCallable, Category="PCG Preview") void Regenerate(bool bNewSeed = true);
	UFUNCTION(BlueprintCallable, Category="PCG Preview") FString GetLayoutSignature() const;
	UFUNCTION(BlueprintCallable, Category="PCG Preview") FString GetVerificationSummary() const;
	UFUNCTION(BlueprintCallable, Category="PCG Preview") void SetPreviewView(int32 Floor);
	UFUNCTION(BlueprintCallable, Category="PCG Preview") void SetExploration(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category="PCG Preview") void FocusRoom(int32 RoomIndex);
	bool PlanLayout();
	void MaterializeLayout(int32 PointCount);
	void ClearGenerated();
	void Fail(const FString& Reason);

private:
	UPROPERTY(Transient) TArray<TObjectPtr<ULevelStreamingDynamic>> Streams;
	UPROPERTY(Transient) TArray<TObjectPtr<ULevelStreamingDynamic>> RetiringStreams;
	int32 GenerationSerial = 0;
	double GenerationStartTime = 0;
	bool bAwaitingGeneration = false;
	bool bRegeneratePending = false;
	bool bNextSeedIsNew = false;
	void BeginGeneration();
};
