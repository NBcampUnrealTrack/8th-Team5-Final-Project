#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "HB_PlayerState.generated.h"


UCLASS()
class HEAVENBREACH_API AHB_PlayerState : public APlayerState,public IAbilitySystemInterface
{
	GENERATED_BODY()
	

public:
	AHB_PlayerState();

#pragma region AbilitySystem
public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override {return AbilitySystemComponent;}
private:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
#pragma endregion 
};
