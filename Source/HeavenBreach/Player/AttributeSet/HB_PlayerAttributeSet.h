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
	ATTRIBUTE_ACCESSORS(ThisClass, Player_ManaCostFlat);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_ManaCostMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_CooldownFlat);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_CooldownMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_LifeStealRatio);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_BasicAttackManaCostMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_LowHealthAttackBonus);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_LowHealthAttackThreshold);

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

	// 마나 소비 + 보정. -5 면 5 감소
	UPROPERTY(ReplicatedUsing = OnRep_ManaCostFlat, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_ManaCostFlat;

	// 마나 소비 % 보정. 1.0 이 기본, 0.9 는 10% 감소
	UPROPERTY(ReplicatedUsing = OnRep_ManaCostMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_ManaCostMultiplier;

	UPROPERTY(ReplicatedUsing = OnRep_CooldownFlat, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_CooldownFlat;

	UPROPERTY(ReplicatedUsing = OnRep_CooldownMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_CooldownMultiplier;

	// 흡혈률. 0 이면 흡혈 없음, 0.2 면 준 피해의 20% 를 체력으로 되돌린다.
	// 버프 GE(피의 광란)·증강·장비의 흡혈이 전부 이 하나에 모인다.
	// 읽는 곳은 AHBPlayerCharacter::NotifyDamageDealt 하나뿐이다.
	UPROPERTY(ReplicatedUsing = OnRep_LifeStealRatio, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_LifeStealRatio;

	// 일반공격 마나 소비 % 보정. 1.0 이 기본, 0.5 는 절반.
	// 기존 Player_ManaCostMultiplier 와 별개인 이유: 그쪽은 하한이 0.2 라 0 을 못 만든다.
	// 전투 호흡의 증강 Free(마나 0)를 표현하려면 하한 0 인 어트리뷰트가 필요하다.
	UPROPERTY(ReplicatedUsing = OnRep_BasicAttackManaCostMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_BasicAttackManaCostMultiplier;

	// 저체력 공격력 보너스의 최대치. 0.5 면 최대 +50%.
	// 0 이면 이 기능이 꺼진 것이므로 패시브 태그를 따로 볼 필요가 없다.
	UPROPERTY(ReplicatedUsing = OnRep_LowHealthAttackBonus, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_LowHealthAttackBonus;

	// 위 보너스가 최대가 되는 체력 비율. 0.3 이면 체력 30% 에서 최대.
	// 기본값을 0.3 으로 둬서 패시브 GE 가 보너스만 지정해도 동작하게 한다.
	UPROPERTY(ReplicatedUsing = OnRep_LowHealthAttackThreshold, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_LowHealthAttackThreshold;

	
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
	UFUNCTION() 
	void OnRep_ManaCostFlat(const FGameplayAttributeData& OldValue);
	UFUNCTION() 
	void OnRep_ManaCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() 
	void OnRep_CooldownFlat(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_CooldownMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_LifeStealRatio(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_BasicAttackManaCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_LowHealthAttackBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_LowHealthAttackThreshold(const FGameplayAttributeData& OldValue);

#pragma endregion 
	
private:
	// 최대치가 바뀔 때 현재치를 어디로 옮길지. HP 와 마나가 같은 규칙이라 함수로 뺐다.
	static float CalculateCurrentOnMaxChanged(float Current, float OldMax, float NewMax);
};
