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
 * 장착 장비가 주는 어트리뷰트 보정. 무기 공격력 + 방어구 방어력 + 모든 칸의 숫자 옵션.
 * 어떤 스탯이 어느 칸으로 가는지는 Item/ItemStatBinding.cpp 의 표가 정한다.
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
 * 미검증: 장비의 공격력 % (Multiplicitive) 와 전투의 함성 배율이 같은 칸에서 만날 때
 * GAS 가 둘을 곱하는지 더하는지 확인하지 않았다. 둘 다 켜고 [Hit] Raw 로 확인할 것.
 */
UCLASS()
class HEAVENBREACH_API UGE_EquipmentStats : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_EquipmentStats();
};

/**
 * 아이템 동작 효과의 표시등. 태그만 붙이고 어트리뷰트는 건드리지 않는다.
 *
 * 태그는 붙일 때 DynamicGrantedTags 로 끼운다. UGE_WarriorTaggedDuration 과 같은 방식이다.
 * 효과끼리 다른 건 태그뿐이라 클래스는 이것 하나다. 효과가 늘어도 여기는 안 고친다.
 *
 * 붙이고 떼는 건 UEquipmentComponent::RefreshItemEffects 가 한다.
 * 실제 동작은 ItemEffectRules 가 효과 정의의 원형을 보고 한다. 이 태그는 "꼈다" 는 표시다
 * (오라가 같은 오라를 낀 사람을 건너뛸 때, showdebug 로 확인할 때 쓴다).
 */
UCLASS()
class HEAVENBREACH_API UGE_ItemEffectTag : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_ItemEffectTag();
};

/**
 * 아이템 동작 효과가 스탯을 바꿀 때 쓰는 GE. 장비 스탯 GE 의 칸(스탯 표 전체)을 그대로 물려받는다.
 *
 * 바꿀 스탯 칸만 채우고 나머지는 0 이다 (ItemStats::FillSpec). 그래서 어떤 스탯이든 이것 하나로 된다.
 * 오라(파티원에게) · 조건 동안(나에게) 이 같이 쓴다. 걸고 떼는 건 ItemEffectRules.
 *
 * 따로 두는 이유: 같은 클래스면 showdebug 에서 장비 스탯과 구분이 안 된다.
 * 생성자를 안 두는 이유: 부모 생성자가 Infinite 와 모디파이어를 이미 다 만든다.
 */
UCLASS()
class HEAVENBREACH_API UGE_ItemEffectStats : public UGE_EquipmentStats
{
	GENERATED_BODY()
};

/**
 * 아이템 부작용의 자기 피해. 한 번 깎고 끝난다. 양은 SetByCaller(Data.ItemEffect.Magnitude)로 받는다 (음수).
 *
 * 체력 1 을 남기도록 자르는 건 ItemEffectRules 가 한다 — 기획상 아이템 부작용으로는 죽지 않는다.
 * 피격용 GE_PlayerDamage 를 안 쓰는 이유: 캐릭터 BP 에 꽂힌 애셋이라 규칙 코드가 직접 집을 수 없다.
 */
UCLASS()
class HEAVENBREACH_API UGE_ItemSelfDamage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_ItemSelfDamage();
};

/** 낙하 피해. 체력을 직접 깎는다(방어·방패·경직 없음). 양은 SetByCaller.Damage 에 음수로 */
UCLASS()
class HEAVENBREACH_API UGE_FallDamage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_FallDamage();
};

/** 패시브 "가벼운 착지" 기본 효과. 낙하 피해 감소 +0.4 */
UCLASS()
class HEAVENBREACH_API UGE_LightLandingPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_LightLandingPassive();
};

/** "가벼운 착지 강화" 증강. 단계 값은 SetByCaller.AugmentMagnitude 로 받는다 */
UCLASS()
class HEAVENBREACH_API UGE_LightLandingAugment : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_LightLandingAugment();
};

/** 스킬 마나 비용. 즉발, 양은 SetByCaller.ManaCost 에 음수로 (UHBPlayerActiveGABase::ApplyCost) */
UCLASS()
class HEAVENBREACH_API UGE_SkillManaCost : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_SkillManaCost();
};

/** 관통 사격 쿨다운. 시간은 SetByCaller.Cooldown, Cooldown.Archer.PiercingArrow 부여 */
UCLASS()
class HEAVENBREACH_API UGE_BowPiercingCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_BowPiercingCooldown();
};

/** 사냥꾼의 표식 쿨다운. 시간은 SetByCaller.Cooldown, Cooldown.Archer.TrackingMark 부여 */
UCLASS()
class HEAVENBREACH_API UGE_BowHunterMarkCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_BowHunterMarkCooldown();
};

/** 회피 사격 쿨다운. 시간은 SetByCaller.Cooldown, Cooldown.Archer.EvasiveShot 부여 */
UCLASS()
class HEAVENBREACH_API UGE_BowEvasiveCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_BowEvasiveCooldown();
};

/** 표식 상태. 시간은 SetByCaller.Duration, State.Archer.Tracked 부여(단계 태그는 스펙의 DynamicGrantedTags) */
UCLASS()
class HEAVENBREACH_API UGE_HunterMark : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_HunterMark();
};

/** 패시브 "민첩한 발걸음". 이동속도 배율 +0.10 */
UCLASS()
class HEAVENBREACH_API UGE_SwiftStepPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_SwiftStepPassive();
};

/** "민첩한 발걸음 강화" 증강. 값은 SetByCaller.AugmentMagnitude */
UCLASS()
class HEAVENBREACH_API UGE_SwiftStepAugment : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_SwiftStepAugment();
};

/** 패시브 "급소 포착". 헤드샷 피해 +0.30 */
UCLASS()
class HEAVENBREACH_API UGE_WeakPointPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WeakPointPassive();
};

/** "급소 포착 강화" 증강. 값은 SetByCaller.AugmentMagnitude */
UCLASS()
class HEAVENBREACH_API UGE_WeakPointAugment : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_WeakPointAugment();
};

/** 패시브 "사냥꾼의 호흡". 달리기 마나 소모 -0.30, 마나 회복 +0.20 */
UCLASS()
class HEAVENBREACH_API UGE_HunterBreathPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_HunterBreathPassive();
};

/** "사냥꾼의 호흡 강화" 증강. 달리기 소모 배율에 SetByCaller.AugmentMagnitude(음수) */
UCLASS()
class HEAVENBREACH_API UGE_HunterBreathAugment : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_HunterBreathAugment();
};

/** 마법 "원소 구체" 쿨다운. 시간은 SetByCaller.Cooldown */
UCLASS()
class HEAVENBREACH_API UGE_MagicOrbCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_MagicOrbCooldown();
};

/** 마법 "순간이동" 쿨다운. 시간은 SetByCaller.Cooldown */
UCLASS()
class HEAVENBREACH_API UGE_MagicTeleportCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_MagicTeleportCooldown();
};

/** 마법 "서리 지대" 쿨다운. 시간은 SetByCaller.Cooldown */
UCLASS()
class HEAVENBREACH_API UGE_MagicFrostCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_MagicFrostCooldown();
};

/** 마법 "마력 폭발" 쿨다운. 시간은 SetByCaller.Cooldown */
UCLASS()
class HEAVENBREACH_API UGE_MagicExplosionCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_MagicExplosionCooldown();
};

/** 시전 중 이동속도 감소. 이동속도 배율 -0.5, 무한(GA 가 직접 뗀다) */
UCLASS()
class HEAVENBREACH_API UGE_CastingSlow : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_CastingSlow();
};

/** 서리 지대 안의 적 감속. 적 MoveSpeedRate 에 SetByCaller.AugmentMagnitude(음수), State.Magic.Frosted 부여. 무한(영역 GA 가 뗀다) */
UCLASS()
class HEAVENBREACH_API UGE_FrostSlow : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_FrostSlow();
};

/** 패시브 "집중". State.Magic.Focus 태그만 부여한다(시전 시간 배율은 코드가 읽는다) */
UCLASS()
class HEAVENBREACH_API UGE_FocusPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_FocusPassive();
};

/** 패시브 "마력 순환". State.Magic.ManaCycle 태그만 부여한다(액티브 스태미나 배율은 코드가 읽는다, 달리기·방어 제외) */
UCLASS()
class HEAVENBREACH_API UGE_ManaCyclePassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_ManaCyclePassive();
};

/** 패시브 "잔류 마력". State.Magic.ResidualMana 태그만 부여한다 */
UCLASS()
class HEAVENBREACH_API UGE_ResidualManaPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_ResidualManaPassive();
};

/** 패시브 "유리 대포". 스킬 피해 배율 +0.25, 받는 피해 배율 +0.15 */
UCLASS()
class HEAVENBREACH_API UGE_GlassCannonPassive : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_GlassCannonPassive();
};

/** "유리 대포 강화" 증강. 스킬 피해 배율에 SetByCaller.AugmentMagnitude(추가분) */
UCLASS()
class HEAVENBREACH_API UGE_GlassCannonAugment : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGE_GlassCannonAugment();
};
