#include "Player/Mage/Abilities/GA_MagePassives.h"
#include "AbilitySystemComponent.h"
#include "Core/Tag/Mage/HB_MageTag.h"
#include "Player/Mage/Effects/HBMageGameplayEffects.h"
#include "Player/Warrior/Effects/HBWarriorEffectLibrary.h"

UGA_MagePassiveBase::UGA_MagePassiveBase() { InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor; NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly; }
void UGA_MagePassiveBase::ActivateAbility(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo*, const FGameplayAbilityActivationInfo, const FGameplayEventData*) {}
#define HB_MP_CTOR(Name, Tag) Name::Name() { ActivationOwnedTags.AddTag(Tag); }
HB_MP_CTOR(UGA_MageManaCycle, MageTag::TAG_Passive_ManaCycle)
HB_MP_CTOR(UGA_MageFlameSpread, MageTag::TAG_Passive_FlameSpread)
HB_MP_CTOR(UGA_MageFrostAccumulation, MageTag::TAG_Passive_FrostAccumulation)
HB_MP_CTOR(UGA_MageManaAmplification, MageTag::TAG_Passive_ManaAmplification)
HB_MP_CTOR(UGA_MageArcaneCycle, MageTag::TAG_Passive_ArcaneCycle)
HB_MP_CTOR(UGA_MageElementalResonance, MageTag::TAG_Passive_ElementalResonance)
UGA_MageGlassCannon::UGA_MageGlassCannon() { ActivationOwnedTags.AddTag(MageTag::TAG_Passive_GlassCannon); }
HB_MP_CTOR(UGA_MageManaAbsorption, MageTag::TAG_Passive_ManaAbsorption)
HB_MP_CTOR(UGA_MageEmergencyShield, MageTag::TAG_Passive_EmergencyShield)
#undef HB_MP_CTOR
