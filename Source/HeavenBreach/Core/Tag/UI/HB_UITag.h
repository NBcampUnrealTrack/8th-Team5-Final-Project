#pragma once
#include "NativeGameplayTags.h"

namespace UITag
{
	// UI 레이어 — UHB_UILayout의 Activatable Widget Stack 하나와 1:1 대응
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Layer_Game);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Layer_GameMenu);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Layer_Menu);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_UI_Layer_Modal);
}
