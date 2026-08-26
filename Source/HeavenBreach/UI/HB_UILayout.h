#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "HB_UILayout.generated.h"

class UCommonActivatableWidget;

/**
 * 화면 전체를 담는 UI 루트. 뷰포트에 붙는 위젯은 이것 하나뿐이다.
 *
 * 레이어(= Activatable Widget Stack) 4개를 들고 있고, 다른 시스템은
 * PushWidgetToLayer로 원하는 레이어에 화면을 밀어 넣기만 하면 된다.
 * 스택이 활성/비활성 전환, 입력 모드 재계산, Z순서를 알아서 처리한다.
 *
 * 레이어 태그는 Core/Tag/UI/HB_UITag.h 참고.
 */
UCLASS(Abstract, Blueprintable)
class HEAVENBREACH_API UHB_UILayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 태그에 해당하는 레이어 스택을 반환. 없으면 nullptr */
	UFUNCTION(BlueprintCallable, Category = "HB|UI")
	UCommonActivatableWidgetStack* GetLayer(FGameplayTag LayerTag) const;

	/** 레이어에 위젯을 push. 생성/풀링은 스택이 처리한다 */
	template <typename WidgetT = UCommonActivatableWidget>
	WidgetT* PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
	{
		if (UCommonActivatableWidgetStack* Layer = GetLayer(LayerTag))
		{
			return Layer->AddWidget<WidgetT>(WidgetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, Category = "HB|UI", meta = (DisplayName = "Push Widget To Layer"))
	UCommonActivatableWidget* BP_PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

protected:
	// WBP_UILayout에 배치한 스택과 이름으로 바인딩된다. 이름이 다르면 컴파일 에러
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Stack_Game;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Stack_GameMenu;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Stack_Menu;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> Stack_Modal;
};
