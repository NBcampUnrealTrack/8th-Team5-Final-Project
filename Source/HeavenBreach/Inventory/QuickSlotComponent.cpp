#include "Inventory/QuickSlotComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Net/UnrealNetwork.h"
#include "Player/PlayerState/HB_PlayerState.h"

UQuickSlotComponent::UQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Slots.SetNum(NumQuickSlots);
}

void UQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UInventoryComponent* Inventory = GetOwnerInventory())
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UQuickSlotComponent::HandleInventoryChanged);
	}
}

UInventoryComponent* UQuickSlotComponent::GetOwnerInventory() const
{
	if (AHB_PlayerState* HBPlayerState = Cast<AHB_PlayerState>(GetOwner()))
	{
		return HBPlayerState->GetInventory();
	}

	AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->FindComponentByClass<UInventoryComponent>() : nullptr;
}

bool UQuickSlotComponent::SetSlot(int32 InSlotIndex, FGuid InInstanceId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!Slots.IsValidIndex(InSlotIndex))
	{
		return false;
	}

	UInventoryComponent* Inventory = GetOwnerInventory();
	if (Inventory == nullptr)
	{
		return false;
	}

	if (Inventory->FindEntryIndexById(InInstanceId) == INDEX_NONE)
	{
		return false;
	}

	Slots[InSlotIndex] = InInstanceId;
	OnRep_Slots();

	return true;
}

bool UQuickSlotComponent::UseSlot(int32 InSlotIndex)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!Slots.IsValidIndex(InSlotIndex))
	{
		return false;
	}

	const FGuid InstanceId = Slots[InSlotIndex];
	if (!InstanceId.IsValid())
	{
		return false;
	}

	UInventoryComponent* Inventory = GetOwnerInventory();
	if (Inventory == nullptr)
	{
		return false;
	}

	if (Inventory->FindEntryIndexById(InstanceId) == INDEX_NONE)
	{
		Slots[InSlotIndex].Invalidate();
		OnRep_Slots();
		return false;
	}

	const bool bUsed = Inventory->UseItem(InstanceId);
	return bUsed;
}

FGuid UQuickSlotComponent::GetSlotInstanceId(int32 InSlotIndex) const
{
	if (!Slots.IsValidIndex(InSlotIndex))
	{
		return FGuid();
	}
	return Slots[InSlotIndex];
}

void UQuickSlotComponent::Server_SetSlot_Implementation(int32 InSlotIndex, FGuid InInstanceId)
{
	SetSlot(InSlotIndex, InInstanceId);
}

void UQuickSlotComponent::Server_UseSlot_Implementation(int32 InSlotIndex)
{
	UseSlot(InSlotIndex);
}

void UQuickSlotComponent::OnRep_Slots()
{
	OnQuickSlotChanged.Broadcast();
}

void UQuickSlotComponent::HandleInventoryChanged()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	UInventoryComponent* Inventory = GetOwnerInventory();
	if (Inventory == nullptr)
	{
		return;
	}

	bool bChanged = false;
	for (FGuid& SlotId : Slots)
	{
		if (SlotId.IsValid() && Inventory->FindEntryIndexById(SlotId) == INDEX_NONE)
		{
			SlotId.Invalidate();
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnRep_Slots();
	}
}

void UQuickSlotComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQuickSlotComponent, Slots);
}

void UQuickSlotComponent::ApplyLoadedSlots(const TArray<FGuid>& InSlots)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 Count = FMath::Min(Slots.Num(), InSlots.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Slots[Index] = InSlots[Index];
	}

	OnRep_Slots();
}