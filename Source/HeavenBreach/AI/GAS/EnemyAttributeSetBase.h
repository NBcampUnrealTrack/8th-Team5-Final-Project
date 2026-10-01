// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EnemyAttributeSetBase.generated.h"


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
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSetBase, Defense);
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSetBase, MoveSpeedRate);
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSetBase, AttackSpeedRate);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	virtual bool PreGameplayEffectExecute(struct FGameplayEffectModCallbackData& Data) override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// ====== ^^

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData HP;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData MaxHP;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData AttackPower;

	// 방어력. 피해 감소는 100/(100+Defense) 곡선이라 100 이면 절반이다.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData Defense;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData MoveSpeedRate;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttributeData AttackSpeedRate;

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
