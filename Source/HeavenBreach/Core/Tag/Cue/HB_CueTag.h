#pragma once
#include "NativeGameplayTags.h"

namespace CueTag
{
	// 아래 모든 타격 큐의 부모. 공격자가 들고 있는 큐 태그를 찾을 때 이걸로 거른다.
	//
	// 왜 큐 태그를 공격자가 직접 드는가: 예전에는 Impact.X 를 달고 ResolveCueTag 가
	// GameplayCue.Hit.X 로 바꿔줬는데, 둘이 1:1 이라 매핑이 전부 중복이었고
	// 타격 종류가 늘 때마다 if 가 같이 늘었다. 별칭을 없애고 큐 태그를 바로 단다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit)

	// 무기 성질별 타격. 맞는 쪽이 그냥 맞았을 때 쓴다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Slash)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Blunt)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Pierce)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Default)

	// 맞는 쪽의 결과가 무기 성질보다 우선한다.
	// 막았으면 칼이든 둔기든 "깡" 소리가 나야지 살점 소리가 나면 안 된다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Block)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Parry)

	// 발사체가 피해를 안 받는 대상(벽 · 사물 · 시체 · 같은 편)에 맞았을 때. 피 없는 큐다.
	//
	// 왜 재질로 안 가르나: 던전 벽과 플레이어가 둘 다 재질 없음(Default)이라, 피를 재질로 막으면
	// 플레이어 피격 피까지 사라진다. 그래서 "누가 쏘나"로 가른다 — 맞은 쪽이 쏘면 몸, 발사체가 쏘면 벽.
	// 쏘는 곳: AEnemyProjectile · AHBPlayerArrow 의 벽 적중. 애셋(BP_GCN_Hit_World)이 없으면 아무것도 안 난다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_World)

	// 적 공격 전용. 적에게 맞는 소리와 플레이어가 때리는 소리가 같아서
	// 누가 맞았는지 귀로 구분이 안 된다는 피드백으로 추가했다.
	// 대상 재질을 보지 않고 GCN 의 디폴트 슬롯 하나로 고정한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Heavy)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Claw)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Bite)

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Magic)

	// 함정 전용. 트랩이 HitCueTag 칸으로 자기 ASC 에 단다 (AHBTrapBase).
	// 적 공격 전용 큐와 같은 이유로 따로 둔다 — 함정에 맞은 건지 귀로 구분되게.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Spike)

	// ================ 스킬 발동 연출 ================
	// 타격이 아니라 시전자 본인에게서 터지는 것. 맞은 대상도 재질도 없다.
	// 위의 Hit 계열과 달리 ResolveCueTag 를 거치지 않는다.
	// UHBPlayerActiveGABase 의 ActivationCueTag 에 애셋이 직접 지정한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Skill_WarCry)
}