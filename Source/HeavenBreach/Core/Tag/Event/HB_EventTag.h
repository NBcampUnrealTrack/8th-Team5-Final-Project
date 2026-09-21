#pragma once
#include "NativeGameplayTags.h"


namespace EventTag
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_MeleeHitCheck)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Ranged_Fire)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Leap_Launch)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Block_Release)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Beam_Fire)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Beam_Stop)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Parry_Success)

	// 소환 지점을 정해 예고 이펙트를 띄우는 시점
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Summon_Mark)

	// 실제로 적이 튀어나오는 시점.
	// 둘 사이의 간격이 곧 회피 가능 시간이라 코드에 딜레이 상수를 두지 않는다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Summon_Spawn)
}
