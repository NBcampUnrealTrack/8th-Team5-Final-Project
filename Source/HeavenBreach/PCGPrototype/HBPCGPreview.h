#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "PCGPrototype/HBDungeonLayout.h"
#include "PCGModule/HBDungeonModuleCatalog.h"
#include "PCGModule/HBDungeonRuleLayout.h"
#include "HBPCGPreview.generated.h"

class UPCGComponent;
class UPCGGraphInterface;
class ULevelStreamingDynamic;
class UCameraComponent;
class USpotLightComponent;
class AHBDungeonPortal;

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

	UPROPERTY(BlueprintReadOnly, Category="PCG Preview - Advanced") TObjectPtr<UPCGComponent> PCG;
	UPROPERTY(BlueprintReadOnly, Category="PCG Preview - Advanced") TObjectPtr<UCameraComponent> PreviewCamera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Advanced", AdvancedDisplay) TObjectPtr<UPCGGraphInterface> Graph;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Modules") TArray<TSoftObjectPtr<UWorld>> ModuleMaps;
    UPROPERTY() TArray<TSoftObjectPtr<UWorld>> RoomModules;
    UPROPERTY() TArray<TSoftObjectPtr<UWorld>> CorridorModules;
    UPROPERTY() bool bModuleRegistrationMigrated = false;
    virtual void PostActorCreated() override;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="PCG Preview - Debug") TArray<FHBModuleCatalogEntry> ModuleCatalog;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="PCG Preview - Debug") TArray<FString> CatalogErrors;
    /** Reads each map's definition/components. Rebuilt again before runtime generation. */
    UFUNCTION(CallInEditor, BlueprintCallable, Category="PCG Preview - Modules") void RefreshModuleCatalog();
    virtual void PostLoad() override;
    // Serialized only to migrate old assets. Hidden from both Details and Blueprint access.
    UPROPERTY() TSoftObjectPtr<UWorld> AdventureModule;
    UPROPERTY() TSoftObjectPtr<UWorld> BossModule;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Generation") bool bRandomSeedOnStart = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Generation") int32 Seed = 12345;
	/** Disable only for unrestricted room-to-room LD connection tests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Generation") bool bUseDungeonRules = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Generation", meta=(EditCondition="bUseDungeonRules", EditConditionHides)) FHBDungeonGenerationRules DungeonRules;
	/** LD connection test: fixed module roles are preserved, gameplay role quotas are not applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Room Counts", meta=(EditCondition="!bUseDungeonRules", EditConditionHides, ClampMin="1", ClampMax="64")) int32 TestRoomCount = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Search") bool bAllowModuleReuse = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Search", meta=(ClampMin="1", ClampMax="1000000")) int32 MaxConnectionAttempts = 20000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Search", meta=(ClampMin="0.1", ClampMax="10", Units="s")) float ConnectionSearchSeconds = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Debug") bool bShowConnectionDebug = false;
	// Serialized legacy grid settings. Direct connection tests derive spacing/height from connectors.
	UPROPERTY() int32 MaxAttempts = 64;
	UPROPERTY() float CellSize = 16000;
	UPROPERTY() float FloorHeight = 5000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Search", meta=(ClampMin="5")) float LoadTimeoutSeconds = 120;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Exploration") bool bStartInExploration = true;
	/** Preview look sensitivity multiplier. 1 preserves the original speed; changes apply immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PCG Preview - Exploration", meta=(ClampMin="0.1", UIMax="5.0")) float MouseSensitivity = 1.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview - Debug", AdvancedDisplay) TObjectPtr<AHBPCGExplorerPawn> Explorer;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview - Result", meta=(DisplayName="Currently Exploring")) bool bExploring = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview - Result", meta=(DisplayName="Current Inspection Room")) int32 InspectionRoom = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview - Result") FString Status = TEXT("Press Play: generates modules, then enters exploration.");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview - Result") int32 ActiveSeed = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview - Result") TArray<FHBPrototypeRoom> Rooms;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview - Result") int32 LoadedRooms = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Preview - Result") bool bReady = false;

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
	bool BuildCatalog();
	bool ValidateGeneratedLayout(FString& Error) const;
	bool LinkGeneratedPortals(FString& Error);
	bool ValidatePortalLinks(FString& Error) const;
	HBConnectorLayout::FSettings ActiveSettings;
	FHBDungeonGenerationRules ActiveRules;
	bool bActiveDungeonRules = false;
	UPROPERTY(Transient) TArray<TObjectPtr<AHBDungeonPortal>> LinkedPortals;
	UPROPERTY(Transient) TArray<TObjectPtr<ULevelStreamingDynamic>> Streams;
	UPROPERTY(Transient) TArray<TObjectPtr<ULevelStreamingDynamic>> RetiringStreams;
	int32 GenerationSerial = 0;
	double GenerationStartTime = 0;
	bool bAwaitingGeneration = false;
	bool bRegeneratePending = false;
	bool bNextSeedIsNew = false;
	void BeginGeneration();
};
