#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGModule/HBDungeonRuleLayout.h"
#include "HBDungeonRuntime.generated.h"
class UPCGComponent;
class UPCGGraphInterface;
class ULevelStreamingDynamic;
class APlayerStart;
class AHBDungeonPortal;
class AHBDungeonDoorSeal;
class AHeavenBreachGameState;
UENUM(BlueprintType)
enum class EHBDungeonRuntimeState : uint8 { Idle, Generating, Loading, Ready, Failed };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHBDungeonReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHBDungeonFailed, const FString&, Reason);

/** Authority plans the layout; each world materializes it locally. */
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
    /** Each possible starting room must contain exactly one PlayerStart with this tag. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Gameplay") FName SpawnTag = TEXT("DungeonSpawn");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Gameplay") TSubclassOf<AHBDungeonDoorSeal> DoorSealClass;
    /** Half-size in cm; connectors mark the floor center, with +X pointing out. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon Gameplay", meta=(ClampMin="1")) FVector DoorSealExtent = FVector(30, 160, 220);
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") EHBDungeonRuntimeState State = EHBDungeonRuntimeState::Idle;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") FString Status;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") int32 ActiveSeed = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Result") TArray<FHBPrototypeRoom> Rooms;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Debug") TArray<FHBModuleCatalogEntry> ModuleCatalog;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon - Debug") TArray<FString> CatalogErrors;
    UPROPERTY(BlueprintAssignable) FHBDungeonReady OnReady;
    UPROPERTY(BlueprintAssignable) FHBDungeonFailed OnFailed;
    UFUNCTION(BlueprintCallable, Category="Dungeon") void StartGeneration();
    UFUNCTION(BlueprintPure, Category="Dungeon") bool IsReady() const { return State == EHBDungeonRuntimeState::Ready; }
    UFUNCTION(BlueprintPure, Category="Dungeon") APlayerStart* GetPlayerStart() const;
    UFUNCTION(CallInEditor, Category="Dungeon - Modules") void RefreshModuleCatalog();
    bool PlanLayout();
    void MaterializeLayout(int32 PointCount);
    /** Client-only entry point for a complete authoritative layout, not a generation request. */
    bool ApplyServerLayout(int32 InGenerationId, int32 InSeed, const TArray<FHBPrototypeRoom>& InRooms);
    bool CanPlanGeneration() const;
    int32 GetGenerationId() const { return GenerationId; }
    /** Server layout is validated and available in Rooms. Transport is supplied by GameState. */
    FSimpleMulticastDelegate OnLayoutPlanned;
    void Fail(const FString& Reason);
private:
    void BindSnapshotSource();
    void ConsumeServerSnapshot();
    TWeakObjectPtr<AHeavenBreachGameState> SnapshotSource;
    FDelegateHandle SnapshotChangedHandle;
    bool PrepareServerGameplay(FString& Error);
    bool PrepareClientGameplay(FString& Error);
    bool SpawnDoorSeals(const FHBPrototypeRoom& Room, const TArray<FHBDungeonConnectorDefinition>& Connectors, FString& Error);
    void LoadLocalRooms();
    bool ValidateLocalSettings(FString& Error) const;
    void ClearGenerated();
    HBConnectorLayout::FSettings ActiveSettings;
    FHBDungeonGenerationRules ActiveRules;
    UPROPERTY(Transient) TArray<TObjectPtr<ULevelStreamingDynamic>> Streams;
    UPROPERTY(Transient) TArray<TObjectPtr<AHBDungeonPortal>> LinkedPortals;
    UPROPERTY(Transient) TArray<TObjectPtr<AHBDungeonDoorSeal>> Seals;
    UPROPERTY(Transient) TObjectPtr<APlayerStart> StartPoint;
    double GenerationStartTime = 0;
    int32 GenerationId = 0;
};
