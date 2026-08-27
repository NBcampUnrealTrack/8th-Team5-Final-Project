#include "HB_EventTag.h"

namespace EventTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_MeleeHitCheck,"Event.Melee.HitCheck")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Ranged_Fire, "Event.Ranged.Fire", "몽타주 발사 프레임 신호")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Leap_Launch, "Event.Leap.Launch", "몽타주 루프 다음단계 나아가기 신호")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Block_Release, "Event.Block.Release", "방어 해제 이벤트. 플레이어가 사용")
}