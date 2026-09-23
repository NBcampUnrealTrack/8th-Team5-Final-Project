#include "UI/HB_ContainerScreen.h"

#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryWidget.h"
#include "Player/HBPlayerCharacter.h"

UHB_ContainerScreen::UHB_ContainerScreen()
{
	// 게임 입력을 막고 커서를 띄운다. 드래그&드롭 때문에 마우스는 캡처하지 않는다
	InputMode = EHBWidgetInputMode::Menu;

	// ESC 로 화면 전체를 닫는다. 안쪽 그리드가 아니라 이 화면이 Back 을 처리해야 한다
	bIsBackHandler = true;
}

void UHB_ContainerScreen::SetContainer(UInventoryComponent* InContainer)
{
	Container = InContainer;
}

void UHB_ContainerScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	BindPanels();
}

void UHB_ContainerScreen::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	if (Container == nullptr)
	{
		return;
	}

	// 상자 컴포넌트는 내 소유가 아니므로 항상 내 인벤토리를 거쳐 RPC를 보낸다(BindPanels 참고).
	AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetOwningPlayerPawn());
	if (PlayerCharacter == nullptr)
	{
		return;
	}

	if (UInventoryComponent* PlayerInventory = PlayerCharacter->GetInventory())
	{
		PlayerInventory->Server_PauseContainerReveal(Container);
	}
}

void UHB_ContainerScreen::BindPanels()
{
	if (Container == nullptr || PlayerPanel == nullptr || BoxPanel == nullptr)
	{
		return;
	}

	AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetOwningPlayerPawn());
	if (PlayerCharacter == nullptr)
	{
		return;
	}

	UInventoryComponent* PlayerInventory = PlayerCharacter->GetInventory();
	if (PlayerInventory == nullptr)
	{
		return;
	}

	// 왼쪽 — 플레이어 인벤토리를 그린다.
	// LinkedContainer 가 상자이므로 우클릭하면 상자로 보낸다.
	PlayerPanel->BindToInventory(PlayerInventory);
	PlayerPanel->BindToEquipment(PlayerCharacter->GetEquipment());
	PlayerPanel->SetLinkedEquipment(PlayerCharacter->GetEquipment());
	PlayerPanel->SetLinkedContainer(Container);
	PlayerPanel->SetParentContainerWidget(this);

	// 오른쪽 — 상자 인벤토리를 그린다.
	// 서버 RPC 는 플레이어 컴포넌트가 보내야 하므로 OwningInventory 를 반드시 지정한다.
	// (지정하지 않으면 상자 컴포넌트가 요청 주체가 되어 전송이 실패한다)
	BoxPanel->BindToInventory(Container);
	BoxPanel->SetOwningInventory(PlayerInventory);
	BoxPanel->SetLinkedContainer(PlayerInventory);
	BoxPanel->SetParentContainerWidget(this);
	BoxPanel->SetEquipmentPanelVisible(false);
}

FReply UHB_ContainerScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		DeactivateWidget();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

UWidget* UHB_ContainerScreen::NativeGetDesiredFocusTarget() const
{
	// 게임패드로 열었을 때 플레이어 인벤토리 쪽에 포커스를 준다
	return PlayerPanel;
}
