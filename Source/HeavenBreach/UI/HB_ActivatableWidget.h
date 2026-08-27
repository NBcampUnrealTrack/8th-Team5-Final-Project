#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "HB_ActivatableWidget.generated.h"

/**
 * 이 위젯이 활성화됐을 때 요구할 입력 모드.
 *
 * CommonUI에서는 위젯이 "나는 이런 입력 모드가 필요하다"고 선언만 하고,
 * 실제 적용/해제 시점은 Activatable Widget Stack이 결정한다.
 * → PlayerController에서 SetInputMode를 직접 부르지 않아도 된다.
 */
UENUM(BlueprintType)
enum class EHBWidgetInputMode : uint8
{
	// 입력 모드에 관여하지 않음. 스택 아래쪽 위젯의 설정을 그대로 따른다
	Default,

	// 게임에만 입력 전달. HUD처럼 항상 떠 있는 UI용
	GameOnly,

	// UI에만 입력 전달 + 커서 표시. 인벤토리 / 상점 / 창고용
	Menu,

	// 게임과 UI 양쪽에 입력 전달. 채팅처럼 플레이 중 조작하는 UI용
	GameAndMenu
};

/**
 * 프로젝트 공통 Activatable Widget 베이스.
 *
 * 화면 단위 UI(HUD, 인벤토리, 상점 등)는 전부 이 클래스를 상속한다.
 * GetDesiredInputConfig를 C++에서 직접 오버라이드하는 대신 InputMode enum으로
 * 노출해서, 블루프린트 작업자가 위젯 기본값에서 고를 수 있게 한다.
 */
UCLASS(Abstract, Blueprintable)
class HEAVENBREACH_API UHB_ActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HB|Input")
	EHBWidgetInputMode InputMode = EHBWidgetInputMode::Default;
};
