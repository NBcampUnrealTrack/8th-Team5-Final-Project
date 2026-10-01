#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGModule/HBDungeonRuleLayout.h"
#include "HBDungeonRuntime.generated.h"
class UPCGComponent;
class UPCGGraphInterface;
class ULevelStreamingDynamic;
class ULevel;
class APlayerStart;
class AHBDungeonPortal;
class AHBDungeonDoorSeal;
class ANavMeshBoundsVolume;
class AHeavenBreachGameState;
struct FHBDungeonSnapshot;
UENUM(BlueprintType)
enum class EHBDungeonRuntimeState : uint8 { Idle, Generating, Loading, PreparingEncounters, Ready, Failed };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHBDungeonReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHBDungeonFailed, const FString&, Reason);

/** Authority plans and prepares room spawns; each world loads its rooms locally. */
UCLASS(Blueprintable)
class HEAVENBREACH_API AHBDungeonRuntime : public AActor
{
    GENERATED_BODY()
public:
    AHBDungeonRuntime();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(BlueprintReadOnly, Category="Dungeon - Advanced") TObjectPtr<UPCGComponent> PCG;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Advanced") TObjectPtr<UPCGGraphInterface> Graph;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Modules") TArray<TSoftObjectPtr<UWorld>> ModuleMaps;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Generation") FHBDungeonGenerationRules DungeonRules;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Generation") bool bRandomSeedOnStart = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Generation") int32 Seed = 12345;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Search") bool bAllowModuleReuse = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Search", meta=(ClampMin="1")) int32 MaxConnectionAttempts = 20000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Search", meta=(ClampMin="0.1")) float ConnectionSearchSeconds = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Search", meta=(ClampMin="5")) float LoadTimeoutSeconds = 120;
    /** Separate upper bound for navigation readiness and room encounter preparation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon - Search", meta=(ClampMin="1")) float EncounterTimeoutSeconds = 30;
    /** Each possible starting room must contain exactly one PlayerStart with this tag. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Gameplay") FName SpawnTag = TEXT("DungeonSpawn");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Gameplay") TSubclassOf<AHBDungeonDoorSeal> DoorSealClass;
    /** Half-size in cm; connectors mark the floor center, with +X pointing out. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Gameplay", meta=(ClampMin="1")) FVector DoorSealExtent = FVector(30, 160, 220);
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") EHBDungeonRuntimeState State = EHBDungeonRuntimeState::Idle;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") FString Status;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") int32 ActiveSeed = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") int32 GenerationId = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") TArray<FHBPrototypeRoom> Rooms;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Debug") TArray<FHBModuleCatalogEntry> ModuleCatalog;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Debug") TArray<FString> CatalogErrors;
    UPROPERTY(BlueprintAssignable) FHBDungeonReady OnReady;
    UPROPERTY(BlueprintAssignable) FHBDungeonFailed OnFailed;
    UFUNCTION(BlueprintCallable, Category="Dungeon") void StartGeneration();
    UFUNCTION(BlueprintPure, Category="Dungeon") bool IsReady() const { return State == EHBDungeonRuntimeState::Ready; }
    UFUNCTION(BlueprintPure, Category="Dungeon") APlayerStart* GetPlayerStart() const;
	/** Read-only access to the exact streamed instance used by a planned room. */
	ULevel* GetLoadedRoomLevel(int32 RoomId) const;
    UFUNCTION(CallInEditor, Category="Dungeon - Modules") void RefreshModuleCatalog();
    bool PlanLayout();
    void MaterializeLayout(int32 PointCount);
    bool CanPlanGeneration() const;
    /** Server-only reset before gameplay; generation IDs are never reused. */
    bool ResetGeneration();
    /** Also used by EndPlay; permanently stop work in a world being left. */
    void ShutdownGeneration();
    bool ApplyServerLayout(const FHBDungeonSnapshot& Snapshot);
    int32 GetGenerationId() const { return GenerationId; }
    bool ActivateEncounters(FString& Error);
    bool AreEncountersActive() const { return bEncountersActive; }
    FSimpleMulticastDelegate OnLayoutPlanned;
    void Fail(const FString& Reason);
private:
    void BindSnapshotSource();
    void ConsumeServerSnapshot();
    bool ValidateLocalSettings(FString& Error) const;
    void LoadLocalRooms();
    bool PrepareServerGameplay(FString& Error);
    bool PrepareClientGameplay(FString& Error);
    bool SpawnDoorSeals(const FHBPrototypeRoom& Room, const TArray<FHBDungeonConnectorDefinition>& Connectors, FString& Error);
    bool PrepareEncounters(FString& Error);
    bool CreateDungeonNavigationBounds(FString& Error);
    bool IsEncounterNavigationReady() const;
    void MarkReady();
    void ClearGenerated();
    bool bEncountersActive = false;
    bool bEndingPlay = false;
    bool bClearingGeneration = false;
    HBConnectorLayout::FSettings ActiveSettings;
    FHBDungeonGenerationRules ActiveRules;
    UPROPERTY(Transient) TSubclassOf<AHBDungeonDoorSeal> ActiveDoorSealClass;
    FVector ActiveDoorSealExtent = FVector::ZeroVector;
    UPROPERTY(Transient) TArray<TObjectPtr<ULevelStreamingDynamic>> Streams;
    UPROPERTY(Transient) TArray<TObjectPtr<AHBDungeonPortal>> LinkedPortals;
    UPROPERTY(Transient) TArray<TObjectPtr<AHBDungeonDoorSeal>> Seals;
    UPROPERTY(Transient) TObjectPtr<APlayerStart> StartPoint;
    UPROPERTY(Transient) TObjectPtr<ANavMeshBoundsVolume> DungeonNavigationBounds;
    double GenerationStartTime = 0;
    double EncounterStartTime = 0;
    TWeakObjectPtr<AHeavenBreachGameState> SnapshotSource;
    FDelegateHandle SnapshotChangedHandle;
};
