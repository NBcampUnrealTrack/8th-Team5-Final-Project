#pragma once
#include "NativeGameplayTags.h"

namespace CueTag
{
	// 무기 성질별 타격. 맞는 쪽이 그냥 맞았을 때 쓴다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Slash)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Blunt)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Pierce)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Default)

	// 맞는 쪽의 결과가 무기 성질보다 우선한다.
	// 막았으면 칼이든 둔기든 "깡" 소리가 나야지 살점 소리가 나면 안 된다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Block)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GameplayCue_Hit_Parry)

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Impact_Slash)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Impact_Blunt)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Impact_Pierce)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Impact_Magic)
}