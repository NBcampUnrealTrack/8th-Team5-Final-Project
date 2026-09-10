#include "CraftComponent.h"

#include "InventoryComponent.h"
#include "Item.h"
#include "Net/UnrealNetwork.h"

UCraftComponent::UCraftComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Slots.SetNum(NumCraftSlots);
}

int32 UCraftComponent::GetCurrentGold() const
{
	if (TargetStash == nullptr || GoldItemRow.IsNull())
	{
		return 0;
	}

	return TargetStash->GetTotalGold(GoldItemRow.RowName);
}

bool UCraftComponent::GetPreviewResult(FDataTableRowHandle& OutItemRow, int32& OutCount, int32& OutGoldCost, bool& bOutCanAfford) const
{
	FCraftRecipeRow Recipe;

	OutCount = 0;
	OutGoldCost = 0;
	bOutCanAfford = false;

	if (!FindMatchingRecipe(Recipe))
	{
		return false;
	}

	OutItemRow = Recipe.ResultItem;
	OutCount = Recipe.ResultCount;
	OutGoldCost = Recipe.GoldCost;
	bOutCanAfford = GetCurrentGold() >= Recipe.GoldCost;

	return true;
}

void UCraftComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Slots.Num() != NumCraftSlots)
	{
		Slots.SetNum(NumCraftSlots);
	}
}

void UCraftComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCraftComponent, Slots);
	DOREPLIFETIME(UCraftComponent, CraftMode);
}

void UCraftComponent::PlaceFromStash(FGuid InInstanceId, int32 InSlotIndex)
{
	Server_PlaceItem(InInstanceId, InSlotIndex);
}

void UCraftComponent::RemoveFromSlot(int32 InSlotIndex)
{
	Server_RemoveItem(InSlotIndex);
}

void UCraftComponent::ExecuteCraft()
{
	Server_ExecuteCraft();
}

void UCraftComponent::ReturnAllToStash()
{
	Server_ReturnAllToStash();
}

bool UCraftComponent::GetItemViewData(const FDataTableRowHandle& InRow, FItemViewData& OutViewData)
{
	return ResolveItemViewData(InRow, OutViewData);
}

void UCraftComponent::PlaceToFirstEmptySlot(FGuid InInstanceId)
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].IsValid())
		{
			Server_PlaceItem(InInstanceId, Index);
			return;
		}
	}
}

void UCraftComponent::OnRep_Slots()
{
	OnCraftSlotsChanged.Broadcast();
}

void UCraftComponent::SetTargetStash(UInventoryComponent* InStash)
{
	TargetStash = InStash;
}

void UCraftComponent::BuildSlotCountMap(TMap<FName, int32>& OutCounts) const
{
	OutCounts.Reset();

	for (const FItemInstance& Slot : Slots)
	{
		if (!Slot.IsValid() || Slot.ItemRow.IsNull())
		{
			continue;
		}

		OutCounts.FindOrAdd(Slot.ItemRow.RowName) += Slot.StackCount;
	}
}

bool UCraftComponent::FindMatchingRecipe(FCraftRecipeRow& OutRecipe) const
{
	if (RecipeTable == nullptr)
	{
		return false;
	}

	TMap<FName, int32> SlotCounts;
	BuildSlotCountMap(SlotCounts);

	if (SlotCounts.Num() == 0)
	{
		return false;
	}

	static const FString Context(TEXT("FindMatchingRecipe"));

	TArray<FCraftRecipeRow*> Rows;
	RecipeTable->GetAllRows(Context, Rows);

	for (const FCraftRecipeRow* Row : Rows)
	{
		if (Row == nullptr || Row->Ingredients.Num() != SlotCounts.Num())
		{
			continue;
		}

		bool bMatched = true;

		for (const FCraftIngredient& Ingredient : Row->Ingredients)
		{
			const int32* Found = SlotCounts.Find(Ingredient.ItemRow.RowName);

			if (Found == nullptr || *Found < Ingredient.Count)
			{
				bMatched = false;
				break;
			}
		}

		if (bMatched)
		{
			OutRecipe = *Row;
			return true;
		}
	}

	return false;
}

bool UCraftComponent::CanCraft() const
{
	FCraftRecipeRow Recipe;
	return FindMatchingRecipe(Recipe);
}

void UCraftComponent::Server_SetCraftMode_Implementation(ECraftMode InMode)
{
	if (CraftMode == InMode)
	{
		return;
	}

	Server_ReturnAllToStash_Implementation();
	CraftMode = InMode;
}

void UCraftComponent::Server_PlaceItem_Implementation(FGuid InInstanceId, int32 InSlotIndex)
{
	if (TargetStash == nullptr || !Slots.IsValidIndex(InSlotIndex))
	{
		return;
	}

	if (Slots[InSlotIndex].IsValid())
	{
		return;
	}

	const int32 EntryIndex = TargetStash->FindEntryIndexById(InInstanceId);

	if (EntryIndex == INDEX_NONE)
	{
		return;
	}

	const FItemInstance Placed = TargetStash->GetEntries()[EntryIndex].Item;

	if (!TargetStash->RemoveStackCount(InInstanceId, Placed.StackCount))
	{
		return;
	}

	Slots[InSlotIndex] = Placed;

	OnRep_Slots();
}

void UCraftComponent::Server_RemoveItem_Implementation(int32 InSlotIndex)
{
	if (ReturnSlotToStash(InSlotIndex))
	{
		OnRep_Slots();
	}
}

void UCraftComponent::Server_ReturnAllToStash_Implementation()
{
	bool bChanged = false;

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		bChanged |= ReturnSlotToStash(Index);
	}

	if (bChanged)
	{
		OnRep_Slots();
	}
}

bool UCraftComponent::ReturnSlotToStash(int32 InSlotIndex)
{
	if (TargetStash == nullptr || !Slots.IsValidIndex(InSlotIndex))
	{
		return false;
	}

	if (!Slots[InSlotIndex].IsValid())
	{
		return false;
	}

	const int32 Before = Slots[InSlotIndex].StackCount;

	int32 Remainder = 0;
	TargetStash->TryAddItem(Slots[InSlotIndex], Remainder);

	if (Remainder >= Before)
	{
		return false;
	}

	if (Remainder > 0)
	{
		Slots[InSlotIndex].StackCount = Remainder;
	}
	else
	{
		Slots[InSlotIndex] = FItemInstance();
	}

	return true;
}

void UCraftComponent::Server_ExecuteCraft_Implementation()
{
	if (TargetStash == nullptr)
	{
		OnCraftCompleted.Broadcast(false);
		return;
	}

	FCraftRecipeRow Recipe;

	if (!FindMatchingRecipe(Recipe))
	{
		OnCraftCompleted.Broadcast(false);
		return;
	}

	if (Recipe.GoldCost > 0 && GetCurrentGold() < Recipe.GoldCost)
	{
		OnCraftCompleted.Broadcast(false);
		return;
	}

	FItemInstance Result;
	Result.ItemRow = Recipe.ResultItem;
	Result.InstanceId = FGuid::NewGuid();
	Result.StackCount = Recipe.ResultCount;

	FItemViewData ResultViewData;
	if (ResolveItemViewData(Result.ItemRow, ResultViewData) && ResultViewData.IsEquippable())
	{
		Result.CurrentDurability = GetMaxDurabilityForGrade(ResultViewData.Grade);
	}

	if (!TargetStash->CanFitItem(Result))
	{
		OnCraftCompleted.Broadcast(false);
		return;
	}

	if (Recipe.GoldCost > 0 && !TargetStash->ConsumeGold(GoldItemRow, Recipe.GoldCost))
	{
		OnCraftCompleted.Broadcast(false);
		return;
	}

	for (const FCraftIngredient& Ingredient : Recipe.Ingredients)
	{
		int32 Left = Ingredient.Count;

		for (int32 Index = 0; Index < Slots.Num() && Left > 0; ++Index)
		{
			if (!Slots[Index].IsValid() || Slots[Index].ItemRow.RowName != Ingredient.ItemRow.RowName)
			{
				continue;
			}

			const int32 Taken = FMath::Min(Left, Slots[Index].StackCount);
			Slots[Index].StackCount -= Taken;
			Left -= Taken;

			if (Slots[Index].StackCount <= 0)
			{
				Slots[Index] = FItemInstance();
			}
		}
	}

	int32 Remainder = 0;
	TargetStash->TryAddItem(Result, Remainder);

	OnRep_Slots();
	OnCraftCompleted.Broadcast(true);
}