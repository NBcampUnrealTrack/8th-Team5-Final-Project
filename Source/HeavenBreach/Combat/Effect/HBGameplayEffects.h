#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "HBGameplayEffects.generated.h"

/** Stable replicated definition used for cooldown/state tags with per-spec duration. */
UCLASS()
class HEAVENBREACH_API UGE_WarriorTaggedDuration : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorTaggedDuration();
};

UCLASS()
class HEAVENBREACH_API UGE_WarriorBattleCryBuff : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorBattleCryBuff();
};

UCLASS()
class HEAVENBREACH_API UGE_WarriorCombatInstinctStack : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorCombatInstinctStack();
};

UCLASS()
class HEAVENBREACH_API UGE_WarriorKillingMomentumBuff : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorKillingMomentumBuff();
};

UCLASS()
class HEAVENBREACH_API UGE_WarriorStrongBodyPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WarriorStrongBodyPassive();
};

/**
 * 방어태세 유지 중 이동속도 감소.
 *
 * Infinite 다. 방어를 놓는 시점을 GE 가 알 수 없으므로 GA_GuardStance 가
 * 핸들을 들고 있다가 EndAbility 에서 직접 뗀다.
 *
 * 왜 C++ 인가: BP_GE_PlayerBlockSlow 가 있었지만 구 방어와 함께 지웠고,
 * 이 프로젝트는 수치만 있는 GE 를 여기 모아 두는 쪽이다. 애셋을 하나 더
 * 만들면 그걸 GA 에 꽂는 단계가 생기고, 빠뜨리면 감속 없이 방어가 된다.
 */
UCLASS()
class HEAVENBREACH_API UGE_GuardStanceSlow : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_GuardStanceSlow();
};

/**
 * 장착 장비가 주는 어트리뷰트 보정. 방어구 방어력 + 무기 공격력.
 *
 * Infinite + Additive 다. 직업 GE 는 Instant Override 라 BaseValue 를 직접 박는데,
 * 이건 그 위에 얹히는 모디파이어라 나중에 직업 GE 가 와도 살아남는다.
 * 덕분에 세이브 복원이 직업 초기화보다 먼저 와도 값이 사라지지 않는다.
 *
 * 값은 UEquipmentComponent::RefreshEquipmentStats 가 SetByCaller 로 채운다.
 * 슬롯 하나가 바뀌면 이 GE 를 통째로 떼고 다시 붙인다 — 슬롯별 핸들을 들면
 * 교체·파손·복원마다 어느 걸 떼야 하는지가 갈라져서 언젠가 하나를 빠뜨린다.
 *
 * 주의: Additive 라 전투의 함성(Multiplicitive)이 장비까지 곱한다.
 * (기본 + 무기) × 배율 이다. 무기가 좋을수록 버프가 세진다.
 *
 * 쳐낼 조건: 악세서리를 붙일 때 MaxHP / MoveSpeed 모디파이어를 여기 더한다.
 * 값이 0 이면 0 을 더할 뿐이라 호출부에 분기가 생기지 않는다.
 */
UCLASS()
class HEAVENBREACH_API UGE_EquipmentStats : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_EquipmentStats();
};
