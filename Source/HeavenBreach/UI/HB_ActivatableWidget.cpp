#include "UI/HB_ActivatableWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Components/Widget.h"

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

void UHB_ActivatableWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bRecoverLostFocus || InputMode != EHBWidgetInputMode::Menu || !IsActivated() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	// 매 프레임 확인할 필요는 없다.
	FocusCheckElapsed += InDeltaTime;
	if (FocusCheckElapsed < 0.1f)
	{
		return;
	}
	FocusCheckElapsed = 0.0f;

	APlayerController* PC = GetOwningPlayer();
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	// 누군가 포커스를 갖고 있으면 그대로 둔다 (다른 창이나 팝업이 쓰는 중일 수 있다).
	if (FSlateApplication::Get().GetUserFocusedWidget(LocalPlayer->GetControllerId()).IsValid())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UI | 포커스가 없어 되찾는다: %s"), *GetName());

	if (UWidget* Target = GetDesiredFocusTarget())
	{
		Target->SetFocus();
	}
	else
	{
		SetUserFocus(PC);
	}
}
