// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EnemyAttributeSetBase.generated.h"


// getter, setter, initter자동으로 만들어줌
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UEnemyAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()
public:
	UEnemyAttributeSetBase();

	ATTRIBUTE_ACCESSORS(UEnemyAttributeSetBase, HP);
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSetBase, MaxHP);
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSetBase, AttackPower);
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSetBase, MoveSpeedRate);
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSetBase, AttackSpeedRate);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ====== vv 호출 순서 순

	// 명령을 받은 직후
	// false : 잘못된거로 보고 취소함(필터링, 무적상태같은거)
	virtual bool PreGameplayEffectExecute(struct FGameplayEffectModCallbackData& Data) override;

	// 값이 바뀌기 전 호출
	// 체력이 maxHP를 넘지못하게 막을때...
	// 체력이 0 미틍로 내려가지않게 막을 때...
	// 이동 속도가 너무 빨라지지않게...
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// 값이 바뀐 후 호출
	// 피가 깍였으니까 UI체력바 갱신하라...
	// 피가 깍엿으니 사운드를 재생해라...
	// 특정 스텟이 변했으니까 캐릭터 외형을 바꿔라...
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// 명령을 수행하고 난 후
	// 죽었어? 경험치 지급? 
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// ====== ^^

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData HP;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData MaxHP;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData AttackPower;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData MoveSpeedRate;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData AttackSpeedRate;

	// 더미데이터
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
