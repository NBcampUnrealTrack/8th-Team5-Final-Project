// ================ 구시대 스킬 ================
// 새 전투의 함성(UHBPlayerActiveGABase 기반)으로 대체 중인 코드다. 지우지 않는다.
//
// 스킬 레벨(0~3) 모델이 증강 태그로 바뀌면서 UpgradeLevel 은 항상 0 이다.
// 아래 Lv.1~3 분기는 전부 기본값 쪽으로만 간다. 의도된 상태다.
//
// 왜 호출을 끊었나: AHB_PlayerState::GetSkillUpgradeLevel 은 폐기 대기 중인
// 껍데기다. 지우는 순간 여기가 컴파일 에러가 되므로 미리 끊었다.
//
// 쳐낼 조건: 새 함성이 이걸 대체하면 파일째로 지운다.

#include "Player/Warrior/Abilities/GA_WarriorBattleCry.h"

#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "Combat/Effect/HBEffectLibrary.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Player/HBPlayerCharacter.h"

UGA_WarriorBattleCry::UGA_WarriorBattleCry()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_WarriorBattleCry::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || HBEffects::IsCooldownActive(ASC, WarriorTag::TAG_Cooldown_BattleCry))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority())
	{
		const int32 UpgradeLevel = 0;   // 레벨 모델 폐기. 파일 맨 위 설명 참조
		const float Duration = UpgradeLevel >= 1 ? 7.f : 5.f;
		const float AttackMultiplier = UpgradeLevel >= 2 ? 1.3f : 1.2f;
		const float DefenseBonus = UpgradeLevel >= 3 ? 20.f : 0.f;
		HBEffects::TryApplyCooldown(ASC, WarriorTag::TAG_Cooldown_BattleCry, CooldownDuration);
		HBEffects::ApplyBattleCryBuff(
			ASC, Duration, AttackMultiplier, 0.2f, DefenseBonus);
		// Future Lv.3 hook: find nearby teammates and apply a reduced version of this buff.
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
