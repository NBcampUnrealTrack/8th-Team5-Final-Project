#pragma once

#include "CoreMinimal.h"
#include "Player/Abilities/HBPrototypeAbilityBase.h"
#include "GA_MageAbilities.generated.h"

#define HB_MAGE_SIG protected: virtual void ActivateAbility(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo*, const FGameplayAbilityActivationInfo, const FGameplayEventData*) override;
UCLASS()
class HEAVENBREACH_API UGA_MageMagicBolt : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageTeleport : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageFireball : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageIceLance : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageChainLightning : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageIceWall : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageFireZone : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageMagicShield : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageGravityField : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
UCLASS()
class HEAVENBREACH_API UGA_MageManaExplosion : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_MAGE_SIG };
#undef HB_MAGE_SIG
