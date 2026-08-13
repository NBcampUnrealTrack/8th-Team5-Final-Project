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
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	
#pragma region AttributeData
public:
	ATTRIBUTE_ACCESSORS(ThisClass,Player_HP);
	ATTRIBUTE_ACCESSORS(ThisClass,Player_MaxHP);
	
protected:
	UPROPERTY(ReplicatedUsing= OnRep_HP, BlueprintReadOnly,Category="Attribute|Combat")
	FGameplayAttributeData Player_HP;
	
	UPROPERTY(ReplicatedUsing= OnRep_MaxHP, BlueprintReadOnly,Category="Attribute|Combat")
	FGameplayAttributeData Player_MaxHP;
	
	UFUNCTION()
	void OnRep_HP(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHP(const FGameplayAttributeData& OldValue);
#pragma endregion 
	

};
