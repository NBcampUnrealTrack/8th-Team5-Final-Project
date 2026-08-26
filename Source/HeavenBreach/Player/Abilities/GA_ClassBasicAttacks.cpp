#include "Player/Abilities/GA_ClassBasicAttacks.h"

#include "AbilitySystemComponent.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
#include "Core/Tag/Mage/HB_MageTag.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"

namespace
{
	float GetAttackInterval(const FGameplayAbilityActorInfo* ActorInfo, const float BaseInterval)
	{
		const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		const float Speed = ASC
			? FMath::Max(0.1f, ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute()))
			: 1.f;
		return BaseInterval / Speed;
	}
}

void UGA_ArcherBasicAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Player || !BeginAbilityWithCooldown(Handle, ActorInfo, ActivationInfo,
		ArcherTag::TAG_Cooldown_BasicAttack, GetAttackInterval(ActorInfo, 0.9f)))
	{
		return;
	}

	if (AActor* Target = FindLineTarget(Player, 1800.f, 35.f))
	{
		ApplySkillDamage(Player, Target, Player->GetAttackPower());
	}
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_MageBasicAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Player || !BeginAbilityWithCooldown(Handle, ActorInfo, ActivationInfo,
		MageTag::TAG_Cooldown_BasicAttack, GetAttackInterval(ActorInfo, 1.f)))
	{
		return;
	}

	if (AActor* Target = FindLineTarget(Player, 1500.f, 55.f))
	{
		ApplySkillDamage(Player, Target, Player->GetAttackPower());
	}
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}
