#include "Player/Archer/Abilities/GA_ArcherPassives.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
UGA_ArcherPassiveBase::UGA_ArcherPassiveBase(){ InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor; NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly; }
void UGA_ArcherPassiveBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,const FGameplayAbilityActivationInfo ActivationInfo,const FGameplayEventData*) { if(!CommitAbility(Handle,ActorInfo,ActivationInfo)) EndAbility(Handle,ActorInfo,ActivationInfo,true,true); }
#define HB_ARCHER_TAG_CTOR(Class, Tag) Class::Class(){ ActivationOwnedTags.AddTag(Tag); }
HB_ARCHER_TAG_CTOR(UGA_ArcherEagleEye, ArcherTag::TAG_Passive_EagleEye)
HB_ARCHER_TAG_CTOR(UGA_ArcherLongRangeShot, ArcherTag::TAG_Passive_LongRangeShot)
HB_ARCHER_TAG_CTOR(UGA_ArcherWeakPointShot, ArcherTag::TAG_Passive_WeakPointShot)
HB_ARCHER_TAG_CTOR(UGA_ArcherLightFootsteps, ArcherTag::TAG_Passive_LightFootsteps)
HB_ARCHER_TAG_CTOR(UGA_ArcherHunterSense, ArcherTag::TAG_Passive_HunterSense)
HB_ARCHER_TAG_CTOR(UGA_ArcherTrapExpert, ArcherTag::TAG_Passive_TrapExpert)
HB_ARCHER_TAG_CTOR(UGA_ArcherTacticalRetreat, ArcherTag::TAG_Passive_TacticalRetreat)
HB_ARCHER_TAG_CTOR(UGA_ArcherQuickReload, ArcherTag::TAG_Passive_QuickReload)
HB_ARCHER_TAG_CTOR(UGA_ArcherPoisonArrow, ArcherTag::TAG_Passive_PoisonArrow)
#undef HB_ARCHER_TAG_CTOR
