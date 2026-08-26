#pragma once

#include "CoreMinimal.h"
#include "Player/HBPlayerCharacter.h"
#include "HBWarriorCharacter.generated.h"

class UAbilitySystemComponent;

/** 전사 전용 외형이나 시작 장비 연결이 필요할 때 사용하는 선택적 캐릭터 클래스입니다. */
UCLASS(Blueprintable)
class HEAVENBREACH_API AHBWarriorCharacter : public AHBPlayerCharacter
{
	GENERATED_BODY()

public:
	AHBWarriorCharacter();

protected:
	virtual void OnAbilitySystemInitialized(UAbilitySystemComponent* AbilitySystem) override;

	UFUNCTION(BlueprintImplementableEvent, Category="Warrior|Loadout")
	void InitializeWarriorLoadout();

private:
	bool bWarriorLoadoutInitialized = false;
};
