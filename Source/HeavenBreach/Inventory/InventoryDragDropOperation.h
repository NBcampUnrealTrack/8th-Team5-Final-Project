#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryDragDropOperation.generated.h"

class UInventoryComponent;

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
};