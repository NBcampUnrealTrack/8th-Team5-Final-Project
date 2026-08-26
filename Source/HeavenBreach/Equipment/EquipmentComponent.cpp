#include "EquipmentComponent.h"

#include "Net/UnrealNetwork.h"
#include "InventoryComponent.h"
#include "AbilitySystemComponent.h"
#include "Player/HBPlayerCharacter.h"

UEquipmentComponent::UEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Slots.SetNum(static_cast<int32>(EEquipSlotType::MAX));
}

UAbilitySystemComponent* UEquipmentComponent::GetOwnerASC() const
{
	if (AHBPlayerCharacter* OwnerCharacter = Cast<AHBPlayerCharacter>(GetOwner()))
	{
		return OwnerCharacter->GetAbilitySystemComponent();
	}

	return nullptr;
}

void UEquipmentComponent::ApplyRelicEffects(const FItemInstance& InItem)
{
	UE_LOG(LogTemp, Warning, TEXT("[Relic] ApplyRelicEffects CALLED"));

	FItemViewData ViewData;
	if (!ResolveItemViewData(InItem.ItemRow, ViewData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] ResolveItemViewData FAILED"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Relic] bIsRelic = %s"), ViewData.bIsRelic ? TEXT("true") : TEXT("false"));

	if (!ViewData.bIsRelic)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] ASC is NULL"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Relic] BenefitEffect valid: %s, RiskEffect valid: %s"),
		ViewData.BenefitEffect ? TEXT("true") : TEXT("false"),
		ViewData.RiskEffect ? TEXT("true") : TEXT("false"));

	if (ViewData.BenefitEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		EquippedRelicBenefitHandle = ASC->ApplyGameplayEffectToSelf(
			ViewData.BenefitEffect.GetDefaultObject(), 1.f, Context);
	}

	if (ViewData.RiskEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		EquippedRelicRiskHandle = ASC->ApplyGameplayEffectToSelf(
			ViewData.RiskEffect.GetDefaultObject(), 1.f, Context);
	}
}

void UEquipmentComponent::RemoveRelicEffects()
{
	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC == nullptr)
	{
		return;
	}

	if (EquippedRelicBenefitHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(EquippedRelicBenefitHandle);
		EquippedRelicBenefitHandle.Invalidate();
	}

	if (EquippedRelicRiskHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(EquippedRelicRiskHandle);
		EquippedRelicRiskHandle.Invalidate();
	}
}

void UEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Slots.Num() != static_cast<int32>(EEquipSlotType::MAX))
	{
		Slots.SetNum(static_cast<int32>(EEquipSlotType::MAX));
	}
}

void UEquipmentComponent::OnRep_Slots()
{
	OnEquipmentChanged.Broadcast();
}

bool UEquipmentComponent::IsSlotOccupied(EEquipSlotType InSlot) const
{
	if (InSlot == EEquipSlotType::None || InSlot == EEquipSlotType::MAX)
	{
		return false;
	}

	return Slots[SlotToIndex(InSlot)].IsValid();
}

bool UEquipmentComponent::GetItemInSlot(EEquipSlotType InSlot, FItemInstance& OutItem) const
{
	if (!IsSlotOccupied(InSlot))
	{
		return false;
	}

	OutItem = Slots[SlotToIndex(InSlot)];
	return true;
}

bool UEquipmentComponent::FindSlotByInstanceId(FGuid InInstanceId, EEquipSlotType& OutSlot) const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].IsValid() && Slots[Index].InstanceId == InInstanceId)
		{
			OutSlot = static_cast<EEquipSlotType>(Index);
			return true;
		}
	}

	return false;
}

bool UEquipmentComponent::CanEquip(const FItemInstance& InItem, EEquipSlotType InSlot) const
{
	if (InSlot == EEquipSlotType::None || InSlot == EEquipSlotType::MAX)
	{
		return false;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(InItem.ItemRow, ViewData))
	{
		return false;
	}

	const UEnum* SlotEnum = StaticEnum<EEquipSlotType>();

	return ViewData.EquipSlotType == InSlot;
}

bool UEquipmentComponent::EquipFromInventory(UInventoryComponent* InSource, FGuid InInstanceId, EEquipSlotType InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InSource == nullptr)
	{
		return false;
	}

	const int32 SourceIndex = InSource->FindEntryIndexById(InInstanceId);
	if (SourceIndex == INDEX_NONE)
	{
		return false;
	}

	const FItemInstance SourceItem = InSource->GetEntries()[SourceIndex].Item;
	const FIntPoint SourcePosition = InSource->GetEntries()[SourceIndex].Position;

	if (!CanEquip(SourceItem, InSlot))
	{
		return false;
	}

	const int32 SlotIndex = SlotToIndex(InSlot);
	const FItemInstance PreviousItem = Slots[SlotIndex];

	FItemInstance RemovedItem;
	if (!InSource->RemoveItem(InInstanceId, RemovedItem))
	{
		return false;
	}

	if (PreviousItem.IsValid())
	{
		if (!InSource->TryAddItemAt(PreviousItem, SourcePosition))
		{
			int32 RollbackRemainder = 0;
			InSource->TryAddItem(RemovedItem, RollbackRemainder);
			return false;
		}
	}

	Slots[SlotIndex] = RemovedItem;

	UE_LOG(LogTemp, Warning, TEXT("[Relic] EquipFromInventory: InSlot = %d (Accessory = %d)"),
		(int32)InSlot, (int32)EEquipSlotType::Accessory);

	if (InSlot == EEquipSlotType::Accessory)
	{
		ApplyRelicEffects(RemovedItem);
	}

	OnEquipmentChanged.Broadcast();

	return true;
}

bool UEquipmentComponent::UnequipToInventory(EEquipSlotType InSlot, UInventoryComponent* InTarget)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InTarget == nullptr || !IsSlotOccupied(InSlot))
	{
		return false;
	}

	const int32 SlotIndex = SlotToIndex(InSlot);
	const FItemInstance EquippedItem = Slots[SlotIndex];

	int32 Remainder = 0;
	if (!InTarget->TryAddItem(EquippedItem, Remainder))
	{
		return false;
	}

	Slots[SlotIndex] = FItemInstance();

	if (InSlot == EEquipSlotType::Accessory)
	{
		RemoveRelicEffects();
	}

	OnEquipmentChanged.Broadcast();

	return true;
}

bool UEquipmentComponent::UnequipToInventoryAt(EEquipSlotType InSlot, UInventoryComponent* InTarget, FIntPoint InTargetPosition)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InTarget == nullptr || !IsSlotOccupied(InSlot))
	{
		return false;
	}

	const int32 SlotIndex = SlotToIndex(InSlot);
	const FItemInstance EquippedItem = Slots[SlotIndex];

	if (!InTarget->TryAddItemAt(EquippedItem, InTargetPosition))
	{
		return false;
	}

	Slots[SlotIndex] = FItemInstance();

	if (InSlot == EEquipSlotType::Accessory)
	{
		RemoveRelicEffects();
	}

	OnEquipmentChanged.Broadcast();

	return true;
}

void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEquipmentComponent, Slots);
}

void UEquipmentComponent::DebugPrintSlots() const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Index == static_cast<int32>(EEquipSlotType::None))
		{
			continue;
		}

		const UEnum* SlotEnum = StaticEnum<EEquipSlotType>();
		const FString SlotName = SlotEnum ? SlotEnum->GetNameStringByValue(Index) : FString::FromInt(Index);

		if (!Slots[Index].IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("%s: empty"), *SlotName);
			continue;
		}

		FItemViewData ViewData;
		ResolveItemViewData(Slots[Index].ItemRow, ViewData);

		UE_LOG(LogTemp, Log, TEXT("%s: %s"), *SlotName, *ViewData.DisplayName);
	}
}