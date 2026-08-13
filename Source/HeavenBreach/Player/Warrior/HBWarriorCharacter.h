#pragma once

#include "CoreMinimal.h"
#include "Player/HBPlayerCharacter.h"
#include "HBWarriorCharacter.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBWarriorCharacter : public AHBPlayerCharacter
{
	GENERATED_BODY()

public:
	AHBWarriorCharacter();

protected:
	virtual void OnAbilitySystemInitialized(UAbilitySystemComponent* AbilitySystem) override;

	/**
	 * 전사의 기본 능력치를 설정하는 Instant Gameplay Effect
	 * 에디터에서 Health, Mana, AttackPower, Defense 값을 가진 GE로 연결
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Abilities")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffect;

	/** 서버가 전사에게 최초 한 번 지급할 Gameplay Ability 목록입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Warrior|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/**
	 * 장비/인벤토리 담당 시스템이 완성되면 검, 방패, 포션 지급을 연결할 지점입니다.
	 * 서버에서 전사 초기화 시 최초 한 번 호출됩니다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Warrior|Loadout")
	void InitializeWarriorLoadout();

private:
	void ApplyDefaultAttributes(UAbilitySystemComponent* AbilitySystem);
	void GrantStartupAbilities(UAbilitySystemComponent* AbilitySystem);

	bool bWarriorInitialized = false;
};
