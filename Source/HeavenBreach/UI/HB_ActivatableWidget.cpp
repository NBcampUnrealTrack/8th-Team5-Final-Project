#include "UI/HB_ActivatableWidget.h"

TOptional<FUIInputConfig> UHB_ActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputMode)
	{
	case EHBWidgetInputMode::GameOnly:
		// 기존 FInputModeGameOnly와 동일: 마우스 상시 캡처 + 커서 숨김
		return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently);

	case EHBWidgetInputMode::Menu:
		// 기존 FInputModeGameAndUI(+커서 표시)를 대체. 드래그&드롭 때문에 캡처하지 않는다
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);

	case EHBWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown);

	case EHBWidgetInputMode::Default:
	default:
		// 빈 TOptional = "나는 입력 모드에 관심 없다". 스택 아래 위젯의 설정이 유지된다
		return TOptional<FUIInputConfig>();
	}
}
