#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ArcherPassives.generated.h"

UCLASS(Abstract) class HEAVENBREACH_API UGA_ArcherPassiveBase : public UGameplayAbility
{ GENERATED_BODY() public: UGA_ArcherPassiveBase(); protected: virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override; };
UCLASS() class HEAVENBREACH_API UGA_ArcherEagleEye : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherEagleEye(); };
UCLASS() class HEAVENBREACH_API UGA_ArcherLongRangeShot : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherLongRangeShot(); };
UCLASS() class HEAVENBREACH_API UGA_ArcherWeakPointShot : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherWeakPointShot(); };
UCLASS() class HEAVENBREACH_API UGA_ArcherLightFootsteps : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherLightFootsteps(); };
UCLASS() class HEAVENBREACH_API UGA_ArcherHunterSense : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherHunterSense(); };
UCLASS() class HEAVENBREACH_API UGA_ArcherTrapExpert : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherTrapExpert(); };
UCLASS() class HEAVENBREACH_API UGA_ArcherTacticalRetreat : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherTacticalRetreat(); };
UCLASS() class HEAVENBREACH_API UGA_ArcherQuickReload : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherQuickReload(); };
UCLASS() class HEAVENBREACH_API UGA_ArcherPoisonArrow : public UGA_ArcherPassiveBase { GENERATED_BODY() public: UGA_ArcherPoisonArrow(); };
