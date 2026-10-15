#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_ContainerScreen.generated.h"

class UInventoryComponent;
class UInventoryWidget;
class USoundBase;

/**
 * 상자 / 몬스터 시체 / 플레이어 시체처럼 "외부 인벤토리를 여는" 화면.
 *
 * UI.Layer.Menu 스택에 push 되며, 왼쪽에 플레이어 인벤토리(PlayerPanel),
 * 오른쪽에 대상 인벤토리(BoxPanel)를 나란히 보여준다.
 *
 * 컨테이너는 push 직전 SetContainer() 로 주입된다. 스택의 초기화 함수는
 * 활성화 이전에 실행되므로, NativeOnActivated 시점에는 값이 채워져 있다.
 */
UCLASS()
class HEAVENBREACH_API UHB_ContainerScreen : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UHB_ContainerScreen();

	/** push 하기 전에 호출한다. 열 대상 인벤토리 */
	void SetContainer(UInventoryComponent* InContainer);

protected:
	/**
	 * 이 화면이 키보드 포커스를 받을 수 있게 한다.
	 *
	 * 왜: 드래그 앤 드랍이 끝나면 InventoryWidget::RefocusAfterDrop 이 포커스를 이 화면으로 되돌리는데,
	 * 포커스를 받을 수 없는 위젯이면 되돌리기가 조용히 실패한다. 그러면 아이템을 옮긴 뒤 아무도 포커스를
	 * 갖지 않아 ESC/Tab 이 먹지 않는다.
	 */
	virtual void NativeOnInitialized() override;

	virtual void NativeOnActivated() override;

	/** 화면 닫을 때(ESC/Tab 등 모든 경로 공통) 서칭 타이머를 멈춘다. */
	virtual void NativeOnDeactivated() override;

	virtual bool NativeOnHandleBackAction() override;

	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/**
	 * Tab 으로 화면 전체를 닫는다.
	 *
	 * OnKeyDown(버블링)이 아니라 Preview(터널링)에서 잡는다. 포커스는 PlayerPanel 에
	 * 가 있는데, UInventoryWidget::NativeOnKeyDown 이 Tab 을 자기 것으로 소비해서
	 * 부모까지 올라오지 않기 때문이다.
	 *
	 * IA_UI_Back 에 Tab 을 매핑하면 안 된다. Back 이 화면을 닫는 순간 입력 모드가
	 * Game 으로 돌아가고, 같은 Tab 이 IA_ToggleInventory 로 한 번 더 잡혀서
	 * 인벤토리가 곧바로 열린다.
	 */
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** 양쪽 그리드를 서로 연결한다. 컨테이너와 플레이어 폰이 모두 준비돼야 한다 */
	void BindPanels();

protected:
	/** WBP_Inventory — 플레이어 인벤토리 쪽 그리드 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UInventoryWidget> PlayerPanel;

	/** WBP_BoxGrid — 상자 쪽 그리드 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UInventoryWidget> BoxPanel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> LootOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> ItemRevealedSound;

private:
	UFUNCTION()
	void HandleItemRevealed();

	UPROPERTY()
	TObjectPtr<UInventoryComponent> Container;
};
