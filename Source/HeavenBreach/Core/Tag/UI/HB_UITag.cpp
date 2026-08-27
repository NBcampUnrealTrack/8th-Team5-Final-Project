#include "HB_UITag.h"

namespace UITag
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer_Game, "UI.Layer.Game", "HUD 등 인게임 상시 UI. 항상 맨 아래")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer_GameMenu, "UI.Layer.GameMenu", "게임 진행 중 조작하는 UI (채팅 등)")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer_Menu, "UI.Layer.Menu", "인벤토리 / 상점 / 창고 / 일시정지")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_UI_Layer_Modal, "UI.Layer.Modal", "확인 팝업 등 응답이 필요한 UI. 최상위")
}
