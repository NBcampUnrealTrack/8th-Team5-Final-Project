#include "EquipmentWidget.h"
#include "EquipmentComponent.h"
#include "EquipmentSlotWidget.h"
#include "InventoryComponent.h"

void UEquipmentWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureSlotMapInitialized();
}

void UEquipmentWidget::EnsureSlotMapInitialized()
{
	if (SlotMap.Num() > 0) return;

	SlotMap.Add(EEquipSlotType::Helmet, Slot_Helmet);
	SlotMap.Add(EEquipSlotType::Chest, Slot_Chest);
	SlotMap.Add(EEquipSlotType::Legs, Slot_Legs);
	SlotMap.Add(EEquipSlotType::PrimaryWeapon, Slot_PrimaryWeapon);
	SlotMap.Add(EEquipSlotType::SecondaryEquip, Slot_SecondaryEquip);
	SlotMap.Add(EEquipSlotType::Accessory, Slot_Accessory);
	if (Slot_Backpack != nullptr)
	{
		SlotMap.Add(EEquipSlotType::Backpack, Slot_Backpack);
	}
}

void UEquipmentWidget::BindToEquipment(UEquipmentComponent* InEquipment, UInventoryComponent* InInventory, UInventoryWidget* InOwningInventoryWidget)
{
	EnsureSlotMapInitialized();

	if (BoundEquipment != nullptr)
	{
		BoundEquipment->OnEquipmentChanged.RemoveDynamic(this, &UEquipmentWidget::HandleEquipmentChanged);
	}

	BoundEquipment = InEquipment;
	BoundInventory = InInventory;
	OwningInventoryWidget = InOwningInventoryWidget;

	if (BoundEquipment != nullptr)
	{
		BoundEquipment->OnEquipmentChanged.AddDynamic(this, &UEquipmentWidget::HandleEquipmentChanged);
	}

	int32 InitializedCount = 0;
	for (auto& Pair : SlotMap)
	{
		if (UEquipmentSlotWidget* SlotWidget = Pair.Value)
		{
			SlotWidget->InitSlot(Pair.Key, BoundEquipment, BoundInventory, OwningInventoryWidget);
			InitializedCount++;
		}
	}

	RefreshSlots();
}

UEquipmentSlotWidget* UEquipmentWidget::GetHoveredSlotWidget() const
{
	for (const auto& Pair : SlotMap)
	{
		if (UEquipmentSlotWidget* SlotWidget = Pair.Value)
		{
			if (SlotWidget->IsHovered())
			{
				return SlotWidget;
			}
		}
	}

	return nullptr;
}

void UEquipmentWidget::NativeDestruct()
{
	if (BoundEquipment != nullptr)
	{
		BoundEquipment->OnEquipmentChanged.RemoveDynamic(this, &UEquipmentWidget::HandleEquipmentChanged);
	}

	Super::NativeDestruct();
}

void UEquipmentWidget::HandleEquipmentChanged()
{
	RefreshSlots();
}

void UEquipmentWidget::RefreshSlots()
{
	if (BoundEquipment == nullptr)
	{
		return;
	}

	const UEnum* SlotEnum = StaticEnum<EEquipSlotType>();

	for (auto& Pair : SlotMap)
	{
		EEquipSlotType SlotType = Pair.Key;
		UEquipmentSlotWidget* SlotWidget = Pair.Value;

		if (SlotWidget == nullptr)
		{
			continue;
		}

		FItemInstance EquippedItem;
		const bool bHasItem = BoundEquipment->GetItemInSlot(SlotType, EquippedItem);

		if (bHasItem)
		{
			FItemViewData ViewData;
			ResolveItemViewData(EquippedItem.ItemRow, ViewData);

			UTexture2D* IconTexture = LoadItemIcon(ViewData.IconPath);
			SlotWidget->UpdateSlot(true, ViewData.DisplayName, IconTexture);
		}
		else
		{
			const FString SlotName = SlotEnum ? SlotEnum->GetDisplayNameTextByValue(static_cast<int64>(SlotType)).ToString() : FString();
			SlotWidget->UpdateSlot(false, SlotName, nullptr);
		}
	}
}