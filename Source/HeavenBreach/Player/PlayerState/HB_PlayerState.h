#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "HB_PlayerState.generated.h"


class UHB_PlayerAttributeSet;
class UStashComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, CurrentHP, float, MaxHP);

UCLASS()
class HEAVENBREACH_API AHB_PlayerState : public APlayerState,public IAbilitySystemInterface
{
	GENERATED_BODY()
	

public:
	AHB_PlayerState();

protected:
	virtual void BeginPlay() override;

#pragma region AbilitySystem
public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override {return AbilitySystemComponent;}
private:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
#pragma endregion 
	
#pragma region AttributeSet
public:
	UHB_PlayerAttributeSet* GetPlayerAttributeSet() const {return AttributeSet;}
	
private:
	UPROPERTY()
	TObjectPtr<UHB_PlayerAttributeSet> AttributeSet;
#pragma endregion 
	
#pragma region BindingWidget

public:
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnHPChanged OnHPChanged;

protected:
	void OnHealthChanged(const struct FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const struct FOnAttributeChangeData& Data);

#pragma endregion

#pragma region Stash
public:
	UFUNCTION(BlueprintPure, Category = "Stash")
	UStashComponent* GetStash() const { return Stash; }

private:
	UPROPERTY()
	TObjectPtr<UStashComponent> Stash;
#pragma endregion
};
