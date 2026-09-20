#include "HB_EventTag.h"

namespace EventTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_MeleeHitCheck,"Event.Melee.HitCheck")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Beam_Fire, "Event.Beam.Fire")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Beam_Stop, "Event.Beam.Stop")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Summon_Mark, "Event.Summon.Mark")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Summon_Spawn, "Event.Summon.Spawn")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Ranged_Fire, "Event.Ranged.Fire", "��Ÿ�� �߻� ������ ��ȣ")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Leap_Launch, "Event.Leap.Launch", "��Ÿ�� ���� �����ܰ� ���ư��� ��ȣ")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Block_Release, "Event.Block.Release", "��� ���� �̺�Ʈ. �÷��̾ ���")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Parry_Success, "Event.Parry.Success", "패링 성립. 패링 GA 가 받아 후딜을 면제한다");
}