#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Combat/Ability/HBSkillAugment.h"
#include "HBPlayerSkillGABase.generated.h"

class AHBPlayerCharacter;
class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * 플레이어 스킬 GA 의 공통 뿌리. 액티브와 패시브가 여기서 갈린다.
 *
 * 왜 넣었나: 액티브와 패시브가 "스킬"로서 같아야 하는 부분 — 증강 목록,
 * 출처 태그, 아바타 접근 — 을 양쪽에 복사해 두면 한쪽만 고치는 사고가 난다.
 * 그리고 UI 가 스킬 정보를 꺼낼 때 두 타입을 각각 캐스팅해야 한다.
 *
 * 작동방식: UI 는 이 타입 하나로 캐스팅해서 GetAugments 를 부른다.
 * 실행에 관한 것(쿨타임, 비용, 몽타주, 상시 GE)은 여기 없다. 자식이 가진다.
 *
 * 쳐낼 조건: 액티브와 패시브가 공유할 것이 더 늘면 여기로 올린다. 반대로
 * 한쪽만 쓰는 것을 여기 두지 말 것. 그러면 다른 쪽 애셋에 쓰지 않는 칸이 생긴다.
 */
UCLASS(Abstract)
class HEAVENBREACH_API UHBPlayerSkillGABase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UHBPlayerSkillGABase();

	// ================ UI 용. CDO 에서 불러도 안전하다 ================
	// 아래 둘은 애셋에 박힌 값만 읽는다. 인스턴스가 없어도 정확한 답이 나온다.

	// 이 스킬이 가질 수 있는 증강 전부. 보유 여부와 무관한 목록이다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	const TArray<FHBSkillAugment>& GetAugments() const { return Augments; }

	// 태그로 증강 하나를 찾는다. 없으면 false 이고 Out 은 손대지 않는다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	bool FindAugment(FGameplayTag AugmentTag, FHBSkillAugment& Out) const;

	/**
	 * 목록에 없는 이 스킬만의 수치. 설명 치환이 부른다.
	 *
	 * 왜 넣었나: 패링의 창 길이처럼 어트리뷰트도 GE 도 아닌 곳에 사는 값이 있다.
	 * 이게 없으면 치환 코드가 GA_WarriorParry 를 캐스팅해야 하고,
	 * 그러면 공용 파일에 직업별 분기가 쌓인다.
	 *
	 * CDO 에서 불러도 안전해야 한다. 아바타를 거치는 값을 답하지 말 것.
	 *
	 * 쳐낼 조건: 이걸 오버라이드하는 스킬이 없어지면 지운다.
	 */
	virtual bool GetNamedSkillValue(FName InName, float& OutValue) const { return false; }

protected:
	// ================ 애셋에서 지정 ================

	// SourceTag 는 없앴다.
	//
	// 원래는 증강 조회 루트였는데, 그 역할을 Augments 배열과 SkillParam 태그가
	// 가져갔다. 남은 쓰임은 "이 비용 요청이 일반공격인가" 판별 하나뿐이었고,
	// 그건 Ability.BasicAttack 태그로 분리했다. 이름은 Augment.* 인데 하는 일은
	// 일반공격 판별이라 읽는 사람이 매번 헷갈렸다.

	// 이 스킬의 증강 목록. 값의 주인은 여기 하나다.
	//
	// 왜 GA 에 두나: UI 가 스킬 하나를 잡으면 그 스킬의 증강까지 바로 꺼내야 하는데,
	// DA 에 두면 스킬과 증강이 두 애셋으로 갈라져 한쪽만 고치는 사고가 난다.
	//
	// 액티브의 AugmentBuffEffects 와 헷갈리지 말 것.
	//   Augments           이 스킬에 어떤 증강이 있는가 (목록, 설명, 수치)
	//   AugmentBuffEffects 그중 가진 증강이 있을 때 발동 시 같이 붙일 버프 GE
	// 하나는 목록이고 하나는 실행이다. 태그로 서로를 가리킨다.
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Augment", meta = (TitleProperty = "AugmentTag"))
	TArray<FHBSkillAugment> Augments;

	// ================ 인스턴스 전용 ================
	// 아래는 아바타를 거치므로 CDO 에서 부르면 항상 false / 1.0 / nullptr 이다.
	// UI 는 위의 GetAugments / FindAugment 를 쓸 것.

	UFUNCTION(BlueprintPure, Category = "Augment")
	bool HasAugment(FGameplayTag AugmentTag) const;

	// 보유한 증강들이 정한 이 항목의 수치. 아무도 안 정했으면 Default 를 그대로 준다.
	//
	// 숫자의 뜻은 부르는 쪽이 정한다.
	//   배율로 쓸 때  GetAugmentParam(Param.Cooldown, 1.f)  → 미보유면 1.0, 그대로 곱함
	//   더할 때       GetAugmentParam(Param.Duration, 0.f)  → 미보유면 0, 그대로 더함
	//
	// 한 항목은 한 증강만 정한다는 전제다. 둘이 정하면 먼저 찾은 것을 쓴다.
	// 합산 규칙을 만들지 않는 이유는, 그 규칙이 애셋만 보고는 안 보이기 때문이다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	float GetAugmentParam(FGameplayTag ParamTag, float Default) const;

	// ================ ActorInfo 를 직접 받는 것 ================
	// 발동 판정과 비용 계산은 CurrentActorInfo 가 아직 없는 시점에 돈다.
	// 그래서 인자로 받는 형태가 따로 있어야 한다.

	// ActorInfo 에서 ASC 를 꺼낸다. 쓰이는 곳이 많아 널 검사를 여기 모았다.
	UAbilitySystemComponent* GetASC(const FGameplayAbilityActorInfo* ActorInfo) const;

	// 캐릭터에 공격, 방어, 달리기 자원이 GAS 밖에 있어서 캐릭터로 접근해야 한다.
	AHBPlayerCharacter* GetPlayerFrom(const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION(BlueprintPure, Category = "Skill")
	AHBPlayerCharacter* GetPlayer() const;

	// 위의 HasAugment / GetAugmentParam 을 ActorInfo 로 부르는 형태.
	// 비용과 쿨타임 계산이 CurrentActorInfo 가 없는 시점에 돌기 때문에 필요하다.
	// 편의형은 이것들을 GetCurrentActorInfo() 로 부를 뿐이다.
	bool HasAugmentFrom(const FGameplayAbilityActorInfo* ActorInfo, FGameplayTag AugmentTag) const;
	float GetAugmentParamFrom(const FGameplayAbilityActorInfo* ActorInfo, FGameplayTag ParamTag, float Default) const;

	// 보유한 증강이 요구하는 추가 코스트 GE. 없으면 nullptr.
	// 발동할 때 원래 코스트에 더해서 낸다.
	TSubclassOf<UGameplayEffect> FindAdditionalCostEffect(const FGameplayAbilityActorInfo* ActorInfo) const;
};
