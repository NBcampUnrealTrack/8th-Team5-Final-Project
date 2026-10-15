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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HB|Input")
	EHBWidgetInputMode InputMode = EHBWidgetInputMode::Default;

	/**
	 * 활성화된 Menu 화면에서 키보드 포커스가 통째로 사라지면 이 화면이 다시 가져온다.
	 *
	 * 왜 필요한가: 포커스를 쥐고 있던 위젯이 사라지면(클릭한 카드가 닫히며 지워지거나, 드래그 앤 드랍으로
	 * 아이템 칸이 다시 그려질 때) 아무도 포커스를 갖지 않게 된다. 그 상태에서는 ESC 로 화면이 닫히지 않는다.
	 * 다른 위젯이 포커스를 갖고 있을 때는 건드리지 않는다. 아무도 없을 때만 되찾는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HB|Input")
	bool bRecoverLostFocus = true;

private:
	float FocusCheckElapsed = 0.0f;
};
