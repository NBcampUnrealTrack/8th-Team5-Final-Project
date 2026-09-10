#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_PlayerBlock.generated.h"

UCLASS()
class HEAVENBREACH_API UGA_PlayerBlock : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_PlayerBlock();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	// 사용 의도를 몰라서 제거
	//virtual void CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	
	// 방어 해제 신호
	UFUNCTION()
	void OnBlockReleaseReceive(FGameplayEventData Payload);
	UFUNCTION()
	void OnBlockMontageCompleted();
	UFUNCTION()
	void OnBlockMontageCancelled();
	
	//UPROPERTY(EditDefaultsOnly,Category="Block")
	//float BlockMoveSpeedMultiplier = 0.5f;
	
	UPROPERTY(EditDefaultsOnly,Category="Block")
	TObjectPtr<UAnimMontage> AM_Block;

	// 방어해제시 호출될 섹션이름. 업스면 바로종료
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	FName ReleaseSectionName = TEXT("End");

	// 방어중 이속감소 GE
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	TSubclassOf<UGameplayEffect> BlockSlowEffect;

	// 패링 설정은 여기 없다. UGA_WarriorParry 패시브 애셋이 갖는다.
	// 패링은 패시브로 주어지는 것이므로 그 패시브가 값의 주인이어야 한다.
	// 방어는 "패링 패시브가 있으면 그 설정대로 창을 연다" 만 한다.

private:
	// 방어해제신호를 받았는지 검출
	bool bReleaseBlock = false;

	// 사용중인 이속감소 GE의 핸들. 취소시켜야하니 들고있는다.
	FActiveGameplayEffectHandle SlowHandle;

	void RemoveBlockSlow(const FGameplayAbilityActorInfo* ActorInfo);

	// 패링 패시브를 배웠으면 그 설정으로 창을 연다. 안 배웠으면 아무것도 안 한다.
	// 핸들을 안 들고 있는다. 스스로 만료되는 게 목적이고, 방어를 일찍 놓아서
	// 창이 남더라도 패링 판정이 State.Blocking 을 함께 보므로 무해하다.
	void ApplyParryWindow(const FGameplayAbilityActorInfo* ActorInfo);
};
