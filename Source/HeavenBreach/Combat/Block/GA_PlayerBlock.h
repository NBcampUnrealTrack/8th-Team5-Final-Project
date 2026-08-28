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
	
private:
	// 방어해제신호를 받았는지 검출
	bool bReleaseBlock = false;

	// 사용중인 이속감소 GE의 핸들. 취소시켜야하니 들고있는다.
	FActiveGameplayEffectHandle SlowHandle;
};
