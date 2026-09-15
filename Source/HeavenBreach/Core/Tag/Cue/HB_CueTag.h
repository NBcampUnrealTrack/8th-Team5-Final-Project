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

	// 적 공격 전용. 적에게 맞는 소리와 플레이어가 때리는 소리가 같아서
	// 누가 맞았는지 귀로 구분이 안 된다는 피드백으로 추가했다.
	// 대상 재질을 보지 않고 GCN 의 디폴트 슬롯 하나로 고정한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Heavy)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Claw)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Bite)

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Magic)
}