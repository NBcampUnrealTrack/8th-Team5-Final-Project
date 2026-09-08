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

	UPROPERTY()
	FSlateBrush DragVisualBrush;
};