#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Styling/SlateBrush.h"
#include "Layout/Margin.h"
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

	// T키 드래그 중 회전 여부. bRotated(=FItemInstance.bRotated)가 바뀌면 반영된다.
	UPROPERTY()
	bool bRotated = false;

	// 드래그 중 T키 회전 시 위젯/박스를 실시간으로 갱신하기 위해 호출한다.
	void SetRotated(bool bInRotated);

	// UInventoryItemWidget::NativeOnDragDetected 쪽 UMG DefaultDragVisual(SizeBox+위젯)을 등록해두면 SetRotated()가 그쪽도 함께 갱신한다.
	void SetDefaultVisual(USizeBox* InVisualBox, UInventoryItemWidget* InVisualItemWidget, const FVector2D& InBaseSize);

	UPROPERTY()
	FVector2D GrabPixelOffset = FVector2D::ZeroVector;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> SourceInventory = nullptr;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> SourceEquipment = nullptr;

	UPROPERTY()
	EEquipSlotType SourceSlot = EEquipSlotType::None;

	// 드래그 아이콘을 뷰포트에 직접 붙여 Operation이 스스로 소유/관리한다(패널을 넘나들어도 계속 따라다니고, 드롭/취소 시 정확히 한 번 정리됨).
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

	/** 뷰포트 Fill 슬롯 때문에 박스 정렬을 Left/Top으로 고정해야 해서(CreateVisual 주석 참고),
	 * 회전 시 어긋나는 아이콘-박스 중심을 좌상단 Padding으로 보정하는 값을 계산한다. */
	FMargin ComputeCenteringPadding() const;

	TWeakObjectPtr<UUserWidget> SourceWidgetContext;
	TSharedPtr<SWidget> DragVisualRoot;
	TSharedPtr<SBox> DragBoxWidget;
	TSharedPtr<SImage> DragImageWidget;
	FVector2D BaseVisualSize = FVector2D::ZeroVector;

	// SetDefaultVisual()로 등록된 인벤토리 그리드 드래그의 UMG 위젯(SetRotated()에서 함께 갱신됨).
	UPROPERTY()
	TObjectPtr<USizeBox> DefaultVisualBox;
	UPROPERTY()
	TObjectPtr<UInventoryItemWidget> DefaultVisualItemWidget;

	UPROPERTY()
	FSlateBrush DragVisualBrush;
};