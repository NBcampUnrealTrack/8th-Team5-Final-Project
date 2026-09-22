#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Styling/SlateBrush.h"
#include "InventoryTypes.h"
#include "InventoryDragDropOperation.generated.h"

class UInventoryComponent;
class UEquipmentComponent;
class UTexture2D;
class UUserWidget;
class SWidget;
class SBox;
class SImage;
class USizeBox;
class UInventoryItemWidget;

UCLASS()
class HEAVENBREACH_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGuid InstanceId;

	UPROPERTY()
	FIntPoint GrabOffset = FIntPoint::ZeroValue;

	UPROPERTY()
	bool bSplitRequested = false;

	// T키로 드래그 중 회전 여부를 토글한 상태. 실제 아이템 데이터(FItemInstance.bRotated)는
	// 드롭이 확정될 때 서버에서 반영된다 ? 드래그 도중에는 이 값이 미리보기용 진실이다.
	UPROPERTY()
	bool bRotated = false;

	// 드래그 중 T키 회전 시 아이콘/박스를 실시간으로 갱신하기 위해 호출한다.
	void SetRotated(bool bInRotated);

	// 인벤토리 그리드에서 드래그를 시작한 경우(UInventoryItemWidget::NativeOnDragDetected)에는
	// CreateVisual()의 로우레벨 Slate 비주얼이 아니라 UMG DefaultDragVisual(이 SizeBox +
	// InventoryItemWidget 복제본)이 실제로 화면에 보이는 드래그 미리보기다. 그런데 SetRotated()는
	// DragBoxWidget/DragImageWidget(=EquipmentSlotWidget 쪽 CreateVisual 경로)만 갱신하고 있어서,
	// 그리드에서 드래그하다 T키로 회전을 토글해도 미리보기의 테두리/아이콘 모양이 그대로 남아
	// 실제로 놓일 모양과 어긋나 보이는 버그가 있었다. 이 함수로 그 SizeBox/위젯을 등록해두면
	// SetRotated()가 이 경로도 함께 갱신한다.
	void SetDefaultVisual(USizeBox* InVisualBox, UInventoryItemWidget* InVisualItemWidget, const FVector2D& InBaseSize);

	UPROPERTY()
	FVector2D GrabPixelOffset = FVector2D::ZeroVector;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> SourceInventory = nullptr;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> SourceEquipment = nullptr;

	UPROPERTY()
	EEquipSlotType SourceSlot = EEquipSlotType::None;

	// 드래그 아이콘을 뷰포트에 직접 붙여서 이 Operation이 스스로 소유/관리한다.
	// 특정 UInventoryWidget 인스턴스가 아니라 Operation이 소유하므로,
	// 인벤토리 칸/패널 밖으로 나가도, 패널을 넘나들어도 계속 잘 따라다니고
	// 드롭/취소 시 정확히 한 번 정리된다.
	void CreateVisual(UUserWidget* InSourceWidget, UTexture2D* IconTexture, const FVector2D& VisualSize);

private:
	UFUNCTION()
	void HandleDragged(UDragDropOperation* Operation);
	UFUNCTION()
	void HandleDrop(UDragDropOperation* Operation);
	UFUNCTION()
	void HandleDragCancelled(UDragDropOperation* Operation);

	void UpdateVisualPosition();
	void DestroyVisual();

	TWeakObjectPtr<UUserWidget> SourceWidgetContext;
	TSharedPtr<SWidget> DragVisualRoot;
	TSharedPtr<SBox> DragBoxWidget;
	TSharedPtr<SImage> DragImageWidget;
	FVector2D BaseVisualSize = FVector2D::ZeroVector;

	// SetDefaultVisual()로 등록된, 인벤토리 그리드 드래그의 UMG 비주얼(SetRotated()에서 같이 갱신됨).
	UPROPERTY()
	TObjectPtr<USizeBox> DefaultVisualBox;
	UPROPERTY()
	TObjectPtr<UInventoryItemWidget> DefaultVisualItemWidget;

	UPROPERTY()
	FSlateBrush DragVisualBrush;
};