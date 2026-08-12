#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/HitResult.h"
#include "DamageableInterface.generated.h"


UINTERFACE(MinimalAPI, BlueprintType, meta=(DisplayName = "Damageable Interface"))
class UDamageableInterface : public UInterface
{
	GENERATED_BODY()
};

class HEAVENBREACH_API IDamageableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Combat")
	bool CanBeDamaged(AActor* InstigatorActor) const;
	
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Combat")
	void ApplyMeleeHit(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit);
};
