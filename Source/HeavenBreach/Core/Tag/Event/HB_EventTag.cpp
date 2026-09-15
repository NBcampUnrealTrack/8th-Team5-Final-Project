#include "HB_EventTag.h"

namespace EventTag
{
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_MeleeHitCheck,"Event.Melee.HitCheck")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Beam_Fire, "Event.Beam.Fire")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Beam_Stop, "Event.Beam.Stop")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Ranged_Fire, "Event.Ranged.Fire", "��Ÿ�� �߻� ������ ��ȣ")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Leap_Launch, "Event.Leap.Launch", "��Ÿ�� ���� �����ܰ� ���ư��� ��ȣ")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Event_Block_Release, "Event.Block.Release", "��� ���� �̺�Ʈ. �÷��̾ ���")
}