#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Item/ItemOptionTypes.h"

class AHBPlayerCharacter;
struct FGameplayAbilityActorInfo;

// 효과 정의와 아이템의 숫자를 합친 것. 규칙 코드는 이것만 본다.
struct FResolvedItemEffect
{
	FItemEffectDef Def;
	float Amount = 0.f;
	float Condition = 0.f;
};

// 오라 하나로 파티원 한 명에게 건 GE. 끼운 사람이 들고 있다가 뗄 때 쓴다.
struct FItemAuraApplied
{
	TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
	FActiveGameplayEffectHandle Handle;
};

// 원형별로 들고 있어야 하는 것. 캐릭터가 하나 든다. 키는 효과 정의의 Tag.
struct FItemEffectRuntime
{
	TMap<FGameplayTag, TArray<FItemAuraApplied>> Auras;          // 내가 파티원에게 건 것
	TMap<FGameplayTag, FActiveGameplayEffectHandle> Conditions;  // 조건 동안 나에게 건 것
	TMap<FGameplayTag, float> PeriodicElapsed;                   // 주기 효과의 쌓인 시간
};

// 아이템 동작 효과 규칙. 전부 여기 모은다.
//
// 원칙
//   수치 변화는 GE 가 한다. 여기는 "언제" 를 판단하고 GE 를 걸거나 뗀다.
//   수치는 전부 DT 에서 온다 (정의의 주기, 아이템의 효과량 · 조건값). 여기에 숫자를 박지 않는다.
//   효과 하나하나를 모른다. 원형(EItemEffectArchetype)과 조건(EItemEffectCondition)만 안다.
//   핸들 · 쌓인 시간 같은 상태는 들지 않는다. 캐릭터가 FItemEffectRuntime 으로 들고 넘긴다.
//
// 새 효과를 붙이는 법
//   기존 원형 · 조건으로 되면   DT_ItemEffect 에 한 줄. C++ 없음
//   새 조건이 필요하면           EItemEffectCondition 에 하나 + IsConditionMet 에 case 하나
//   새 원형이 필요하면           EItemEffectArchetype 에 하나 + 여기에 처리 하나
namespace ItemEffectRules
{
	// 효과 줄을 정의와 합친다. 정의를 안 골랐거나 다른 표를 골랐으면 false.
	bool Resolve(const FItemEffect& Effect, FResolvedItemEffect& OutResolved);

	// 서버 틱. 오라 · 조건 동안 · 주기 를 돈다.
	void Tick(AHBPlayerCharacter* Owner, float DeltaSeconds, FItemEffectRuntime& State);

	// 들고 있는 것을 전부 거둔다. 죽을 때 · 나갈 때. 안 거두면 버프가 영원히 남는다.
	void Clear(AHBPlayerCharacter* Owner, FItemEffectRuntime& State);

	// 스킬 비용을 낼 때 부른다. 스킬 사용 시 원형을 돈다. 서버에서만 동작한다.
	void OnSkillUsed(const FGameplayAbilityActorInfo* ActorInfo);
}
