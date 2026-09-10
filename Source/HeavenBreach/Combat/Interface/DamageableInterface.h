#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/HitResult.h"
#include "DamageableInterface.generated.h"


/**
 * 피해가 어디서 왔는지. 맞는 쪽이 반응을 다르게 해야 할 때 본다.
 *
 * 작동방식: 때리는 쪽이 ApplyMeleeHit 에 같이 넘긴다.
 * 패링은 근접만 스태거를 주고, 원거리는 피해 감소만 적용한다.
 *
 * 지속피해(출혈 등)는 여기 없다. GE 의 Period 로 대상 체력을 직접 깎으므로
 * 이 인터페이스를 지나가지 않고, 그래서 방어·흡수 계산도 자동으로 건너뛴다.
 *
 * 쳐낼 조건: 마법·폭발처럼 구분이 더 필요해지면 항목만 늘린다. 시그니처는 그대로다.
 */
UENUM(BlueprintType)
enum class EHBDamageSource : uint8
{
	Melee    UMETA(DisplayName = "근접"),
	Ranged   UMETA(DisplayName = "원거리")
};

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
	
	// 이름이 Melee 지만 원거리 피해도 이 함수를 지나간다 (투사체가 같이 부른다).
	// 이름 변경은 BP 오버라이드가 깨질 수 있어 5단계 대청소에서 ApplyHit 으로 정리한다.
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Combat")
	void ApplyMeleeHit(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit, EHBDamageSource Source);
};
