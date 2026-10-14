#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBGimmickChestTrigger.generated.h"

class ABoxActor;
class AEnemyCharacterBase;

USTRUCT(BlueprintType)
struct FHBGimmickSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick")
	TSubclassOf<AEnemyCharacterBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick", meta = (MakeEditWidget = true))
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick")
	float Yaw = 0.f;
};

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBGimmickChestTrigger : public AActor
{
	GENERATED_BODY()

public:
	AHBGimmickChestTrigger();

	UFUNCTION(BlueprintPure, Category = "Gimmick")
	bool HasTriggered() const { return bTriggered; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick")
	FName ChestGroupId = TEXT("Gimmick");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick", meta = (ClampMin = "0", Units = "cm"))
	float ChestMatchRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick")
	TArray<FHBGimmickSpawn> Spawns;

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick", meta = (DisplayName = "On Gimmick Triggered"))
	void BP_OnGimmickTriggered(const TArray<AEnemyCharacterBase*>& SpawnedEnemies);

private:
	void Poll();
	void Trigger();
	FTransform MakeSpawnTransform(const FHBGimmickSpawn& Spawn) const;

	TArray<FVector> ChestPointLocations;
	bool bTriggered = false;
	FTimerHandle PollTimer;
};
