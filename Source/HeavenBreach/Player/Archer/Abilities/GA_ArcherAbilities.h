#pragma once
#include "CoreMinimal.h"
#include "Player/Abilities/HBPrototypeAbilityBase.h"
#include "GA_ArcherAbilities.generated.h"

#define HB_ARCHER_SIG protected: virtual void ActivateAbility(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo*, const FGameplayAbilityActivationInfo, const FGameplayEventData*) override;
UCLASS()
class HEAVENBREACH_API UGA_ArcherEvasiveShot : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
UCLASS()
class HEAVENBREACH_API UGA_ArcherSwiftMovement : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
UCLASS()
class HEAVENBREACH_API UGA_ArcherPiercingArrow : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
UCLASS()
class HEAVENBREACH_API UGA_ArcherBindingTrap : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
UCLASS()
class HEAVENBREACH_API UGA_ArcherMultiShot : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
UCLASS()
class HEAVENBREACH_API UGA_ArcherTrackingMark : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
UCLASS()
class HEAVENBREACH_API UGA_ArcherExplosiveArrow : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
UCLASS()
class HEAVENBREACH_API UGA_ArcherRapidFire : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
UCLASS()
class HEAVENBREACH_API UGA_ArcherSmokeArrow : public UHBPrototypeAbilityBase { GENERATED_BODY() HB_ARCHER_SIG };
#undef HB_ARCHER_SIG
