#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MagePassives.generated.h"

UCLASS(Abstract) class HEAVENBREACH_API UGA_MagePassiveBase : public UGameplayAbility
{ GENERATED_BODY() public: UGA_MagePassiveBase(); protected: virtual void ActivateAbility(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo*, const FGameplayAbilityActivationInfo, const FGameplayEventData*) override; };
UCLASS() class HEAVENBREACH_API UGA_MageManaCycle : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageManaCycle(); };
UCLASS() class HEAVENBREACH_API UGA_MageFlameSpread : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageFlameSpread(); };
UCLASS() class HEAVENBREACH_API UGA_MageFrostAccumulation : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageFrostAccumulation(); };
UCLASS() class HEAVENBREACH_API UGA_MageManaAmplification : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageManaAmplification(); };
UCLASS() class HEAVENBREACH_API UGA_MageArcaneCycle : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageArcaneCycle(); };
UCLASS() class HEAVENBREACH_API UGA_MageElementalResonance : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageElementalResonance(); };
UCLASS() class HEAVENBREACH_API UGA_MageGlassCannon : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageGlassCannon(); };
UCLASS() class HEAVENBREACH_API UGA_MageManaAbsorption : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageManaAbsorption(); };
UCLASS() class HEAVENBREACH_API UGA_MageEmergencyShield : public UGA_MagePassiveBase { GENERATED_BODY() public: UGA_MageEmergencyShield(); };
