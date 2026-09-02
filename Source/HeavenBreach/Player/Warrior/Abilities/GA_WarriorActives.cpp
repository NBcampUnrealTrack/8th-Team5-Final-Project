#include "Player/Warrior/Abilities/GA_WarriorActives.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "GameFramework/Character.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/Warrior/Effects/HBWarriorEffectLibrary.h"

namespace
{
	AActor* FindTarget(AActor* Avatar, float Range, float Radius = 80.f)
	{
		if (!Avatar || !Avatar->GetWorld()) return nullptr;
		const FVector Start = Avatar->GetActorLocation() + FVector::UpVector * 50.f;
		const FVector End = Start + Avatar->GetActorForwardVector() * Range;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Avatar);
		FHitResult Hit;
		Avatar->GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);
		return Hit.GetActor();
	}

	bool GetHealthRatio(AActor* Target, float& OutRatio)
	{
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!ASC) return false;
		float HP = 0.f;
		float MaxHP = 0.f;
		if (ASC->HasAttributeSetForAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute()))
		{
			HP = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute());
			MaxHP = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute());
		}
		else if (ASC->HasAttributeSetForAttribute(UEnemyAttributeSetBase::GetHPAttribute()))
		{
			HP = ASC->GetNumericAttribute(UEnemyAttributeSetBase::GetHPAttribute());
			MaxHP = ASC->GetNumericAttribute(UEnemyAttributeSetBase::GetMaxHPAttribute());
		}
		else return false;
		OutRatio = MaxHP > 0.f ? HP / MaxHP : 1.f;
		return true;
	}

	bool ApplyWarriorHit(AHBPlayerCharacter* Player, AActor* Target, float Damage)
	{
		if (!Player || !Target || !Target->Implements<UDamageableInterface>()) return false;
		FHitResult Hit;
		Hit.Location = Target->GetActorLocation();
		Hit.ImpactPoint = Hit.Location;
		Damage = Player->ModifyOutgoingDamage(Target, Damage);
		IDamageableInterface::Execute_ApplyMeleeHit(Target, Player, Damage, Hit);
		const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		const bool bKilled = TargetASC && TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
		Player->NotifyDamageDealt(Target, Damage, bKilled);
		return bKilled;
	}
}

UGA_WarriorTimedAbility::UGA_WarriorTimedAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UGA_WarriorTimedAbility::IsOnCooldown(
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTag CooldownTag) const
{
	return !ActorInfo || HBWarriorEffects::IsCooldownActive(
		ActorInfo->AbilitySystemComponent.Get(), CooldownTag);
}

void UGA_WarriorTimedAbility::ApplyCooldownAndEnd(
	const float Duration, const FGameplayTag CooldownTag)
{
	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
		ActorInfo && ActorInfo->IsNetAuthority())
	{
		HBWarriorEffects::TryApplyCooldown(
			ActorInfo->AbilitySystemComponent.Get(), CooldownTag, Duration);
	}
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(),
		GetCurrentActivationInfo(), true, false);
}

UGA_WarriorChargeSlash::UGA_WarriorChargeSlash() = default;
void UGA_WarriorChargeSlash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (IsOnCooldown(ActorInfo, WarriorTag::TAG_Cooldown_ChargeSlash)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!Player) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	const int32 UpgradeLevel = Player->GetSkillUpgradeLevel(TEXT("Warrior.ChargeSlash"));
	const float UpgradedDamageMultiplier =
		UpgradeLevel >= 3 ? 1.8f : UpgradeLevel >= 1 ? 1.4f : DamageMultiplier;
	const float UpgradedDashStrength = UpgradeLevel >= 2 ? DashStrength * 1.2f : DashStrength;
	const float UpgradedHitRadius = UpgradeLevel >= 2 ? 120.f : 80.f;
	const float UpgradedCooldown = UpgradeLevel >= 3 ? 4.5f : Cooldown;
	Player->LaunchCharacter(Player->GetActorForwardVector() * UpgradedDashStrength, true, false);
	if (AActor* Target = FindTarget(Player, Player->GetAttackRange() + 100.f, UpgradedHitRadius))
	{
		ApplyWarriorHit(Player, Target, Player->GetAttackPower() * UpgradedDamageMultiplier);
		// Future Lv.2/Lv.3 hook: replace FindTarget with a multi-target sweep and add knockback here.
	}
	ApplyCooldownAndEnd(UpgradedCooldown, WarriorTag::TAG_Cooldown_ChargeSlash);
}

UGA_WarriorCombatRoll::UGA_WarriorCombatRoll() = default;
void UGA_WarriorCombatRoll::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (IsOnCooldown(ActorInfo, WarriorTag::TAG_Cooldown_CombatRoll)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	{
		const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(Character);
		const int32 UpgradeLevel = Player
			? Player->GetSkillUpgradeLevel(TEXT("Warrior.CombatRoll"))
			: 0;
		FVector Direction = Character->GetLastMovementInputVector().GetSafeNormal();
		if (Direction.IsNearlyZero()) Direction = Character->GetActorForwardVector();
		const float UpgradedRollStrength = UpgradeLevel >= 1 ? RollStrength * 1.2f : RollStrength;
		Character->LaunchCharacter(Direction * UpgradedRollStrength, true, false);
		const float UpgradedCooldown = UpgradeLevel >= 3 ? 2.5f : UpgradeLevel >= 2 ? 2.75f : Cooldown;
		// Future Lv.2/Lv.3 hook: grant invulnerability and a short movement-speed effect here.
		ApplyCooldownAndEnd(UpgradedCooldown, WarriorTag::TAG_Cooldown_CombatRoll);
		return;
	}
	ApplyCooldownAndEnd(Cooldown, WarriorTag::TAG_Cooldown_CombatRoll);
}

UGA_WarriorExecute::UGA_WarriorExecute() = default;
void UGA_WarriorExecute::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (IsOnCooldown(ActorInfo, WarriorTag::TAG_Cooldown_Execute)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
	AActor* Target = Player ? FindTarget(Player, Player->GetAttackRange()) : nullptr;
	const int32 UpgradeLevel = Player
		? Player->GetSkillUpgradeLevel(TEXT("Warrior.Execute"))
		: 0;
	const float UpgradedHealthRatio = UpgradeLevel >= 3 ? 0.4f : UpgradeLevel >= 1 ? 0.35f : ExecuteHealthRatio;
	const float UpgradedDamageMultiplier = UpgradeLevel >= 2 ? 3.f : DamageMultiplier;
	float Ratio = 1.f;
	if (!Player || !Target || !GetHealthRatio(Target, Ratio) || Ratio > UpgradedHealthRatio)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}
	const bool bKilled = ApplyWarriorHit(Player, Target, Player->GetAttackPower() * UpgradedDamageMultiplier);
	if (bKilled)
	{
		// Future Lv.3 hook: restore 10% max HP here.
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
	else ApplyCooldownAndEnd(Cooldown, WarriorTag::TAG_Cooldown_Execute);
}

UGA_WarriorChainPull::UGA_WarriorChainPull() = default;
void UGA_WarriorChainPull::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (IsOnCooldown(ActorInfo, WarriorTag::TAG_Cooldown_ChainPull)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	AActor* Avatar = ActorInfo->AvatarActor.Get();
	const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(Avatar);
	const int32 UpgradeLevel = Player
		? Player->GetSkillUpgradeLevel(TEXT("Warrior.ChainPull"))
		: 0;
	const float UpgradedRange = UpgradeLevel >= 1 ? 900.f : Range;
	const float UpgradedPullStrength = UpgradeLevel >= 2 ? 1400.f : PullStrength;
	const float UpgradedCooldown = UpgradeLevel >= 3 ? 8.f : Cooldown;
	ACharacter* Target = Cast<ACharacter>(FindTarget(Avatar, UpgradedRange));
	if (Target)
	{
		const FVector PullDirection = (Avatar->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal();
		Target->LaunchCharacter(PullDirection * UpgradedPullStrength, true, true);
		// Future Lv.2/Lv.3 hook: apply 0.8x damage and a 1.5-second root here.
	}
	ApplyCooldownAndEnd(UpgradedCooldown, WarriorTag::TAG_Cooldown_ChainPull);
}

UGA_WarriorBloodThirst::UGA_WarriorBloodThirst()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_WarriorBloodThirst::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || HBWarriorEffects::IsCooldownActive(ASC, WarriorTag::TAG_Cooldown_BloodThirst))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	if (ActorInfo->IsNetAuthority())
	{
		const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
		const int32 UpgradeLevel = Player
			? Player->GetSkillUpgradeLevel(TEXT("Warrior.BloodThirst"))
			: 0;
		const float UpgradedDuration = UpgradeLevel >= 2 ? 7.f : Duration;
		const float UpgradedCooldown = UpgradeLevel >= 3 ? 13.f : Cooldown;
		HBWarriorEffects::ApplyTaggedDuration(ASC, UpgradedDuration, WarriorTag::TAG_State_BloodThirst);
		HBWarriorEffects::TryApplyCooldown(ASC, WarriorTag::TAG_Cooldown_BloodThirst, UpgradedCooldown);
		// Future Lv.3 hook: extend the active effect on kill, capped at 10 seconds.
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
