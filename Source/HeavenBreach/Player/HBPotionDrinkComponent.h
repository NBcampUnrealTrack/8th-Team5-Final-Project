#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HBPotionDrinkComponent.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UInventoryComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 포션 음용. 인벤토리가 "사용 시작"을 알리면 마시는 동작을 재생하고,
 * 끝까지 마시면 인벤토리에 소모를 확정(CommitItemUse)하고, 도중에 행동하면 취소(CancelItemUse)한다.
 *
 * 흐름은 서버가 쥔다. 재생은 멀티캐스트로 모든 화면에 보내고, 각 화면이
 * 본인이면 1인칭용, 남이면 3인칭용 애니메이션을 고른다.
 *
 * 왜 캐릭터에 두지 않았나: 캐릭터가 이미 크다. 음용은 인벤토리·애니메이션·태그만 알면 되는 독립 기능이다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UHBPotionDrinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHBPotionDrinkComponent();

	// 캐릭터가 PlayerState 의 인벤토리를 찾은 뒤 부른다. 서버에서만 의미가 있다.
	void BindInventory(UInventoryComponent* InInventory);

	bool IsDrinking() const { return PendingInstanceId.IsValid(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 본인 화면용. 병을 화면 아래쪽에 들고 마신다.
	UPROPERTY(EditDefaultsOnly, Category = "Potion")
	TSoftObjectPtr<UAnimSequenceBase> FirstPersonDrinkAnim;

	// 남이 보는 화면용.
	UPROPERTY(EditDefaultsOnly, Category = "Potion")
	TSoftObjectPtr<UAnimSequenceBase> ThirdPersonDrinkAnim;

	// 정식 전사 ABP(ABP_Warrior_F) 에 있는 상체 슬롯.
	UPROPERTY(EditDefaultsOnly, Category = "Potion")
	FName DrinkSlotName = TEXT("UpperBody");

	UPROPERTY(EditDefaultsOnly, Category = "Potion", meta = (ClampMin = "0.0", Units = "s"))
	float DrinkBlendTime = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Potion|Bottle")
	TSoftObjectPtr<UStaticMesh> BottleMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Potion|Bottle")
	FName BottleSocketName = TEXT("hand_r");

	// 값은 포션 테스트(BP_Test_PotionSyncWarrior)에서 맞춘 것이다.
	UPROPERTY(EditDefaultsOnly, Category = "Potion|Bottle")
	FTransform BottleRelativeTransform;

	// 이 태그 중 하나라도 붙으면 음용을 끊는다. 공격·스킬(State.Attacking), 방어·패링, 경직, 사망, 상자 뒤지기.
	UPROPERTY(EditDefaultsOnly, Category = "Potion")
	FGameplayTagContainer InterruptTags;

private:
	UFUNCTION()
	void HandleItemUseBegan(FGuid InstanceId, FName ItemId);

	void FinishDrink();
	void InterruptDrink();
	void StopListeningInterrupts();

	// 손의 무기·방패 숨김(포션 이유). 각 머신에서 부른다.
	void SetWeaponHidden(bool bHidden);
	void HandleInterruptTagChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartDrink(const FString& InBottleMeshPath);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopDrink(bool bInterrupted);

	UPROPERTY()
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BottleComp;

	UPROPERTY()
	TObjectPtr<UAnimMontage> PlayingMontage;

	// 서버 전용. 마시는 중인 아이템.
	FGuid PendingInstanceId;

	FTimerHandle DrinkTimerHandle;

	TArray<TPair<FGameplayTag, FDelegateHandle>> InterruptHandles;
};
