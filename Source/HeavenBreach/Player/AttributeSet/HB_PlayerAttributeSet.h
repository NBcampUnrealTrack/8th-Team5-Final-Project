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
	ATTRIBUTE_ACCESSORS(ThisClass,Player_Stamina);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_MaxStamina);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_StaminaRegenMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_AttackPower);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_AttackSpeed);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_Defense);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_MoveSpeedMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_ManaCostFlat);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_ManaCostMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_CooldownFlat);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_CooldownMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_LifeStealRatio);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_SprintStaminaCostMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_LowHealthAttackBonus);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_LowHealthAttackThreshold);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_MoveSpeedFlat);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_ManaRegenFlat);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_ManaRegenMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_SkillDamageMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_HeadshotDamageBonus);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_DamageTakenMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_MeleeDamageTakenMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_HealingReceivedMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_HealthRegenFlat);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_HealthRegenMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Player_FallDamageReduction);

protected:
	UPROPERTY(ReplicatedUsing= OnRep_HP, BlueprintReadOnly,Category="Attribute|Combat")
	FGameplayAttributeData Player_HP;
	
	UPROPERTY(ReplicatedUsing= OnRep_MaxHP, BlueprintReadOnly,Category="Attribute|Combat")
	FGameplayAttributeData Player_MaxHP;

	UPROPERTY(ReplicatedUsing=OnRep_Mana, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_Mana;
	UPROPERTY(ReplicatedUsing=OnRep_MaxMana, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_MaxMana;

	// 스태미나. 달리기·방어 유지가 쓴다(스킬은 마나). 화면 바 없음 — 체감으로만 느낀다.
	UPROPERTY(ReplicatedUsing=OnRep_Stamina, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_Stamina;
	UPROPERTY(ReplicatedUsing=OnRep_MaxStamina, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_MaxStamina;

	// 스태미나 회복 배율. 1.0 이 기본(사냥꾼의 호흡 +0.2).
	UPROPERTY(ReplicatedUsing=OnRep_StaminaRegenMultiplier, BlueprintReadOnly, Category="Attribute|Combat")
	FGameplayAttributeData Player_StaminaRegenMultiplier;

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

	// 달리기 스태미나 소비 % 보정. 1.0 이 기본, 0 이면 달리기에 스태미나를 안 쓴다(전투 호흡).
	// 하한이 0 이라야 해서 스킬 비용 보정(Player_ManaCostMultiplier, 하한 0.2)과 따로 둔다.
	// 2026-10-13 Player_SprintManaCostMultiplier 에서 이름 변경(DefaultEngine.ini PropertyRedirects).
	UPROPERTY(ReplicatedUsing = OnRep_SprintStaminaCostMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_SprintStaminaCostMultiplier;

	// 저체력 공격력 보너스의 최대치. 0.5 면 최대 +50%.
	// 0 이면 이 기능이 꺼진 것이므로 패시브 태그를 따로 볼 필요가 없다.
	UPROPERTY(ReplicatedUsing = OnRep_LowHealthAttackBonus, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_LowHealthAttackBonus;

	// 위 보너스가 최대가 되는 체력 비율. 0.3 이면 체력 30% 에서 최대.
	// 기본값을 0.3 으로 둬서 패시브 GE 가 보너스만 지정해도 동작하게 한다.
	UPROPERTY(ReplicatedUsing = OnRep_LowHealthAttackThreshold, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_LowHealthAttackThreshold;

	// ===== 장비 옵션이 쓰는 칸 =====
	// 값은 UGE_EquipmentStats 가 넣는다. 스탯과 칸의 연결은 Item/ItemStatBinding.cpp 표에 있다.

	// 이동속도에 더하는 진짜 속도. 0 이 기본.
	// 최종 = (걷기/달리기 속도 + 이것) × MoveSpeedMultiplier
	// 이동은 클라가 먼저 움직이므로 서버·클라 양쪽이 속도를 다시 계산해야 한다.
	// OnRep 이 빠지면 한쪽만 빨라져서 캐릭터가 뒤로 끌려간다.
	UPROPERTY(ReplicatedUsing = OnRep_MoveSpeedFlat, BlueprintReadOnly, Category = "Attribute|Movement")
	FGameplayAttributeData Player_MoveSpeedFlat;

	// 초당 마나 회복에 더하는 양. 0 이 기본.
	// 최종 = (ManaRegenPerSecond + 이것) × ManaRegenMultiplier
	UPROPERTY(ReplicatedUsing = OnRep_ManaRegenFlat, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_ManaRegenFlat;

	// 마나 회복 배율. 1.0 이 기본.
	UPROPERTY(ReplicatedUsing = OnRep_ManaRegenMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_ManaRegenMultiplier;

	// 스킬 피해 배율. 1.0 이 기본. 기본 공격에는 안 걸린다.
	UPROPERTY(ReplicatedUsing = OnRep_SkillDamageMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_SkillDamageMultiplier;

	// 헤드샷 추가 피해. 0 이 기본. 0.1 이면 머리 배율 2배 → 2.2배.
	// 맞는 쪽이 때린 사람의 이 값을 읽는다. 부위 배율은 맞는 쪽이 정하기 때문이다.
	UPROPERTY(ReplicatedUsing = OnRep_HeadshotDamageBonus, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_HeadshotDamageBonus;

	// 받는 피해 배율. 1.0 이 기본. 0.9 면 10% 덜 맞는다.
	// 방어력 곡선과 달리 직선이라 하한이 없으면 무적이 된다. 하한 0.5.
	UPROPERTY(ReplicatedUsing = OnRep_DamageTakenMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_DamageTakenMultiplier;

	// 근접으로 맞을 때만 한 번 더 곱하는 배율. 1.0 이 기본. 위 받는 피해와 곱으로 겹친다.
	UPROPERTY(ReplicatedUsing = OnRep_MeleeDamageTakenMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_MeleeDamageTakenMultiplier;

	// 낙하 피해 감소 비율. 0 이면 그대로, 0.4 면 40% 덜 받는다. 패시브·증강 GE 가 Additive 로 올린다.
	// 상한 0.9 — 감소 수단을 다 겹쳐도 낙하가 완전히 무해해지지 않게.
	UPROPERTY(ReplicatedUsing = OnRep_FallDamageReduction, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_FallDamageReduction;

	// 체력 회복량 배율. 1.0 이 기본. 포션·흡혈·전투 회복이 전부 AHBPlayerCharacter::ReceiveHeal 에서 이걸 곱한다.
	UPROPERTY(ReplicatedUsing = OnRep_HealingReceivedMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_HealingReceivedMultiplier;

	// 초당 체력 회복에 더하는 양. 0 이 기본.
	// 최종 = (HealthRegenPerSecond + 이것) × HealthRegenMultiplier. 마나 회복과 같은 모양이다.
	// 왜 넣었나: 회복 옵션이 마나 쪽에만 있었다. 체력 재생 옵션을 장비·버프로 붙일 자리.
	UPROPERTY(ReplicatedUsing = OnRep_HealthRegenFlat, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_HealthRegenFlat;

	// 체력 재생 배율. 1.0 이 기본. 기본 재생 + 옵션+ 합에 곱한다 — 둘 다 0 이면 % 만으로는 차지 않는다.
	UPROPERTY(ReplicatedUsing = OnRep_HealthRegenMultiplier, BlueprintReadOnly, Category = "Attribute|Combat")
	FGameplayAttributeData Player_HealthRegenMultiplier;


	UFUNCTION()
	void OnRep_HP(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHP(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_StaminaRegenMultiplier(const FGameplayAttributeData& OldValue);
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
	void OnRep_SprintStaminaCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_LowHealthAttackBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_LowHealthAttackThreshold(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MoveSpeedFlat(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_ManaRegenFlat(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_ManaRegenMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_SkillDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_HeadshotDamageBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MeleeDamageTakenMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_FallDamageReduction(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_HealingReceivedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_HealthRegenFlat(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_HealthRegenMultiplier(const FGameplayAttributeData& OldValue);

#pragma endregion
	
private:
	// 최대치가 바뀔 때 현재치를 어디로 옮길지. HP·마나·스태미나가 같은 규칙이라 함수로 뺐다.
	static float CalculateCurrentOnMaxChanged(float Current, float OldMax, float NewMax);
};
