#include "UI/HB_UILayout.h"

#include "CommonActivatableWidget.h"
#include "Core/Tag/UI/HB_UITag.h"

UCommonActivatableWidgetStack* UHB_UILayout::GetLayer(FGameplayTag LayerTag) const
{
	if (LayerTag == UITag::TAG_UI_Layer_Game)
	{
		return Stack_Game;
	}
	if (LayerTag == UITag::TAG_UI_Layer_GameMenu)
	{
		return Stack_GameMenu;
	}
	if (LayerTag == UITag::TAG_UI_Layer_Menu)
	{
		return Stack_Menu;
	}
	if (LayerTag == UITag::TAG_UI_Layer_Modal)
	{
		return Stack_Modal;
	}

	return nullptr;
}

UCommonActivatableWidget* UHB_UILayout::BP_PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	return PushWidgetToLayer<UCommonActivatableWidget>(LayerTag, WidgetClass);
}
