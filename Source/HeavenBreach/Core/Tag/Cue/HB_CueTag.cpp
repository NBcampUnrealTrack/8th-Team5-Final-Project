#include "HB_CueTag.h"

namespace CueTag
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Slash, "GameplayCue.Hit.Slash", "베기 타격")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Blunt, "GameplayCue.Hit.Blunt", "둔타 타격")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Pierce, "GameplayCue.Hit.Pierce", "관통 타격")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_GameplayCue_Hit_Default, "GameplayCue.Hit.Default", "성질 미지정 폴백")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Impact_Slash, "Impact.Slash", "참격류");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Impact_Blunt, "Impact.Blunt", "타격류");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Impact_Pierce, "Impact.Pierce", "화살류");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Impact_Magic, "Impact.Magic", "마법?");
}
