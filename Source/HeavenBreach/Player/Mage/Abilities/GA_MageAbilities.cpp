#include "Player/Mage/Abilities/GA_MageAbilities.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Ability/Magic/HBMagic.h"
#include "Core/Tag/Mage/HB_MageTag.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "Combat/Effect/HBEffectLibrary.h"

// Active-skill mana is charged from FHBJobSkillData by AHBPlayerCharacter.
#define HB_BEGIN_MAGE(Tag, Cooldown, Cost) AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()); if (!Player || !BeginAbilityWithCooldown(Handle, ActorInfo, ActivationInfo, Tag, Cooldown, 0.f)) return
#define HB_END_MAGE() FinishAbility(Handle, ActorInfo, ActivationInfo)

// 마법 스킬의 피해 기준값. 공격력에 마법 피해 배율을 곱한다 (HBMagic::DamageMultiplier).
static float MagicPower(const AHBPlayerCharacter* Player) { return Player->GetAttackPower() * HBMagic::DamageMultiplier; }

void UGA_MageMagicBolt::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_MagicBolt, 1.f, 5.f);
	if (AActor* Target = FindLineTarget(Player, 1400.f)) { ApplySkillDamage(Player, Target, MagicPower(Player)); AddMana(ActorInfo, 8.f); }
	HB_END_MAGE();
}

void UGA_MageTeleport::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_Teleport, 8.f, 10.f);
	Player->SetActorLocation(Player->GetActorLocation() + Player->GetActorForwardVector() * 600.f, true);
	HB_END_MAGE();
}

void UGA_MageFireball::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_Fireball, 6.f, 20.f);
	AActor* Direct = FindLineTarget(Player, 1400.f);
	const FVector Center = Direct ? Direct->GetActorLocation() : Player->GetActorLocation() + Player->GetActorForwardVector() * 900.f;
	for (AActor* Target : FindTargetsInRadius(Player, Center, 300.f)) { ApplySkillDamage(Player, Target, MagicPower(Player) * 1.4f); if (UAbilitySystemComponent* TASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target)) HBEffects::ApplyTaggedDuration(TASC, 5.f, MageTag::TAG_State_Burning); }
	HB_END_MAGE();
}

void UGA_MageIceLance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_IceLance, 5.f, 15.f);
	for (AActor* Target : FindLineTargets(Player, 1300.f)) { ApplySkillDamage(Player, Target, MagicPower(Player) * 1.1f); if (UAbilitySystemComponent* TASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target)) HBEffects::ApplyTaggedDuration(TASC, 4.f, MageTag::TAG_State_Chilled); if (ACharacter* C = Cast<ACharacter>(Target)) C->GetCharacterMovement()->Velocity *= 0.5f; }
	HB_END_MAGE();
}

void UGA_MageChainLightning::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_ChainLightning, 8.f, 25.f);
	if (AActor* First = FindLineTarget(Player, 1100.f)) { ApplySkillDamage(Player, First, MagicPower(Player) * 1.2f); int32 Count = 0; for (AActor* Target : FindTargetsInRadius(Player, First->GetActorLocation(), 450.f)) { if (Target != First && Count++ < 3) ApplySkillDamage(Player, Target, MagicPower(Player) * (0.9f - Count * 0.1f)); } }
	HB_END_MAGE();
}

void UGA_MageIceWall::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_IceWall, 12.f, 25.f);
	// Final wall actor/mesh is intentionally deferred. This prototype pushes nearby enemies away from the placement line.
	const FVector Center = Player->GetActorLocation() + Player->GetActorForwardVector() * 450.f;
	for (AActor* Target : FindTargetsInRadius(Player, Center, 220.f)) if (ACharacter* C = Cast<ACharacter>(Target)) C->LaunchCharacter((Target->GetActorLocation() - Center).GetSafeNormal() * 700.f, true, true);
	HB_END_MAGE();
}

void UGA_MageFireZone::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_FireZone, 10.f, 25.f);
	const FVector Center = Player->GetActorLocation() + Player->GetActorForwardVector() * 650.f;
	for (AActor* Target : FindTargetsInRadius(Player, Center, 400.f)) { ApplySkillDamage(Player, Target, MagicPower(Player)); if (UAbilitySystemComponent* TASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target)) HBEffects::ApplyTaggedDuration(TASC, 6.f, MageTag::TAG_State_Burning); }
	HB_END_MAGE();
}

void UGA_MageMagicShield::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_MagicShield, 15.f, 20.f);
	HBEffects::ApplyTaggedDuration(ActorInfo->AbilitySystemComponent.Get(), 6.f, MageTag::TAG_State_MagicShield);
	HB_END_MAGE();
}

void UGA_MageGravityField::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_GravityField, 12.f, 30.f);
	const FVector Center = Player->GetActorLocation() + Player->GetActorForwardVector() * 600.f;
	for (AActor* Target : FindTargetsInRadius(Player, Center, 450.f)) { if (ACharacter* C = Cast<ACharacter>(Target)) C->LaunchCharacter((Center - Target->GetActorLocation()).GetSafeNormal() * 800.f, true, true); if (UAbilitySystemComponent* TASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target)) HBEffects::ApplyTaggedDuration(TASC, 4.f, MageTag::TAG_State_Chilled); }
	HB_END_MAGE();
}

void UGA_MageManaExplosion::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const float Mana = ASC ? ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute()) : 0.f;
	const float Spent = Mana * 0.5f;
	HB_BEGIN_MAGE(MageTag::TAG_Cooldown_ManaExplosion, 10.f, Spent);
	for (AActor* Target : FindTargetsInRadius(Player, Player->GetActorLocation(), 450.f)) ApplySkillDamage(Player, Target, MagicPower(Player) * (1.f + Spent / 50.f));
	HB_END_MAGE();
}

#undef HB_BEGIN_MAGE
#undef HB_END_MAGE
