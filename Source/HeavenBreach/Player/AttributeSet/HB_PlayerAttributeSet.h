#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "HB_PlayerAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class HEAVENBREACH_API UHB_PlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	UHB_PlayerAttributeSet();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	
#pragma region AttributeData
public:
	ATTRIBUTE_ACCESSORS(ThisClass,Player_HP);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_MaxHP);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_Mana);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_MaxMana);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_AttackPower);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_AttackSpeed);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_Defense);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_MoveSpeedMultiplier);
	
protected:
	UPROPERTY(ReplicatedUsing= OnRep_HP, BlueprintReadOnly,Category="Attribute|Combat")
	FGameplayAttributeData Player_HP;
	
	UPROPERTY(ReplicatedUsing= OnRep_MaxHP, BlueprintReadOnly,Category="Attribute|Combat")
	FGameplayAttributeData Player_MaxHP;

	UPROPERTY(ReplicatedUsing=OnRep_Mana, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_Mana;
	UPROPERTY(ReplicatedUsing=OnRep_MaxMana, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_MaxMana;

	UPROPERTY(ReplicatedUsing=OnRep_AttackPower, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_AttackPower;

	/** 1.0은 기본 공격속도이며 1.2는 20% 증가를 의미합니다. */
	UPROPERTY(ReplicatedUsing=OnRep_AttackSpeed, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_AttackSpeed;

	UPROPERTY(ReplicatedUsing=OnRep_Defense, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_Defense;

	/** 1.0 is normal movement speed. All temporary movement buffs modify this attribute. */
	UPROPERTY(ReplicatedUsing=OnRep_MoveSpeedMultiplier, BlueprintReadOnly, Category="Attribute|Movement")
	FGameplayAttributeData Player_MoveSpeedMultiplier;

	
	UFUNCTION()
	void OnRep_HP(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHP(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttackSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
#pragma endregion 
	

};
