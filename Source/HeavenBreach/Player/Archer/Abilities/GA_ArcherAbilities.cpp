#include "Player/Archer/Abilities/GA_ArcherAbilities.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/Archer/Effects/HBArcherGameplayEffects.h"
#include "Player/HBPlayerCharacter.h"
#include "Combat/Effect/HBEffectLibrary.h"
#include "TimerManager.h"

#define HB_BEGIN_ARCHER(Tag, Seconds) \
if (!BeginAbilityWithCooldown(Handle, ActorInfo, ActivationInfo, Tag, Seconds)) \
{ EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; } \
AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()); \
if (!Player) { FinishAbility(Handle, ActorInfo, ActivationInfo); return; }

void UGA_ArcherEvasiveShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_EvasiveShot, 5.f)
	Player->LaunchCharacter(-Player->GetActorForwardVector() * 700.f, true, false);
	if (AActor* Target = FindLineTarget(Player, 1200.f)) ApplySkillDamage(Player, Target, Player->GetAttackPower());
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_ArcherSwiftMovement::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_SwiftMovement, 8.f)
	HBEffects::ApplyEffect(ActorInfo->AbilitySystemComponent.Get(), UGE_ArcherSwiftMovement::StaticClass());
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_ArcherPiercingArrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_PiercingArrow, 7.f)
	for (AActor* Target : FindLineTargets(Player, 1600.f, 75.f))
		ApplySkillDamage(Player, Target, Player->GetAttackPower() * 1.4f);
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_ArcherBindingTrap::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_BindingTrap, 12.f)
	if (ACharacter* Target = Cast<ACharacter>(FindLineTarget(Player, 700.f, 100.f)))
	{
		if (UCharacterMovementComponent* Move = Target->GetCharacterMovement()) Move->DisableMovement();
		TWeakObjectPtr<ACharacter> WeakTarget(Target);
		FTimerDelegate Restore;
		Restore.BindLambda([WeakTarget]()
		{
			if (ACharacter* Character = WeakTarget.Get())
				if (UCharacterMovementComponent* Move = Character->GetCharacterMovement()) Move->SetMovementMode(MOVE_Walking);
		});
		FTimerHandle Timer;
		Target->GetWorldTimerManager().SetTimer(Timer, Restore, 2.5f, false);
	}
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_ArcherMultiShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_MultiShot, 6.f)
	for (AActor* Target : FindTargetsInCone(Player, 1000.f, 35.f))
		ApplySkillDamage(Player, Target, Player->GetAttackPower() * 0.8f);
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_ArcherTrackingMark::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_TrackingMark, 10.f)
	if (AActor* Target = FindLineTarget(Player, 1500.f))
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
			HBEffects::ApplyTaggedDuration(TargetASC, 8.f, ArcherTag::TAG_State_Tracked);
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_ArcherExplosiveArrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_ExplosiveArrow, 9.f)
	AActor* Primary = FindLineTarget(Player, 1400.f);
	const FVector Center = Primary ? Primary->GetActorLocation() : Player->GetActorLocation() + Player->GetActorForwardVector() * 700.f;
	for (AActor* Target : FindTargetsInRadius(Player, Center, 300.f))
		ApplySkillDamage(Player, Target, Player->GetAttackPower() * 1.2f);
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_ArcherRapidFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_RapidFire, 12.f)
	HBEffects::ApplyEffect(ActorInfo->AbilitySystemComponent.Get(), UGE_ArcherRapidFire::StaticClass());
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}

void UGA_ArcherSmokeArrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	HB_BEGIN_ARCHER(ArcherTag::TAG_Cooldown_SmokeArrow, 14.f)
	AActor* Primary = FindLineTarget(Player, 1300.f);
	const FVector Center = Primary ? Primary->GetActorLocation() : Player->GetActorLocation() + Player->GetActorForwardVector() * 650.f;
	for (AActor* Target : FindTargetsInRadius(Player, Center, 350.f))
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
			HBEffects::ApplyTaggedDuration(TargetASC, 5.f, ArcherTag::TAG_State_Smoked);
	FinishAbility(Handle, ActorInfo, ActivationInfo);
}
#undef HB_BEGIN_ARCHER
