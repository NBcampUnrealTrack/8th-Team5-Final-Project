#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryTypes.h"
#include "InventoryDragDropOperation.generated.h"

class UInventoryComponent;
class UEquipmentComponent;

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
	TObjectPtr<UInventoryComponent> SourceInventory = nullptr;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> SourceEquipment = nullptr;

	UPROPERTY()
	EEquipSlotType SourceSlot = EEquipSlotType::None;

};