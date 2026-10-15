#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBBossArenaController.generated.h"

class AEnemyCharacterBase;
class AExtractionZone;
class AHBBossDoor;
class UBoxComponent;

UENUM(BlueprintType)
enum class EHBBossArenaState : uint8
{
	Waiting,
	Fighting,
	Cleared
};

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBBossArenaController : public AActor
{
	GENERATED_BODY()

public:
	AHBBossArenaController();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Boss")
	EHBBossArenaState GetArenaState() const { return ArenaState; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> ArenaTrigger;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss")
	TArray<TObjectPtr<AHBBossDoor>> ArenaDoors;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<AExtractionZone> ExitZone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0", Units = "s"))
	float LockSeconds = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TSubclassOf<AEnemyCharacterBase> BossClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0", Units = "cm"))
	float BossSearchRadius = 3000.f;

	UPROPERTY(ReplicatedUsing = OnRep_ArenaState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss")
	EHBBossArenaState ArenaState = EHBBossArenaState::Waiting;

	UFUNCTION()
	void OnRep_ArenaState();

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss", meta = (DisplayName = "On Arena State Changed"))
	void BP_OnArenaStateChanged(EHBBossArenaState NewState);

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	void SetArenaState(EHBBossArenaState NewState);
	void ApplyExitVisibility();
	void SetDoors(bool bOpen, bool bLock, bool bInstant);
	void FindBoss();
	void HandleBossDied(AEnemyCharacterBase* Enemy);
	void OnLockExpired();
	bool IsBossAlive() const;

	TWeakObjectPtr<AEnemyCharacterBase> Boss;
	FVector BossAnchor = FVector::ZeroVector;
	bool bHasBossAnchor = false;
	FTimerHandle LockTimer;
	FTimerHandle FindBossTimer;
};
