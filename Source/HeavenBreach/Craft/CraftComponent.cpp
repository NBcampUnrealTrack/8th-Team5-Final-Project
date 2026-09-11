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

void UCraftComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCraftComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCraftComponent, Slots);
	DOREPLIFETIME(UCraftComponent, CraftMode);
	DOREPLIFETIME(UCraftComponent, SelectedRecipeRowName);
}

void UCraftComponent::SetTargetStash(UInventoryComponent* InStash)
{
	TargetStash = InStash;
}

int32 UCraftComponent::GetCurrentGold() const
{
	if (!TargetStash || GoldItemRow.IsNull())
	{
		return 0;
	}
	int32 Total = 0;
	for (const FInventoryEntry& Entry : TargetStash->GetEntries())
	{
		if (Entry.Item.ItemRow == GoldItemRow)
		{
			Total += Entry.Item.StackCount;
		}
	}
	return Total;
}

bool UCraftComponent::GetSelectedRecipe(FCraftRecipeRow& OutRecipe) const
{
	if (!RecipeTable || SelectedRecipeRowName == NAME_None)
	{
		return false;
	}
	const FCraftRecipeRow* Row = RecipeTable->FindRow<FCraftRecipeRow>(SelectedRecipeRowName, TEXT("GetSelectedRecipe"));
	if (!Row)
	{
		return false;
	}
	OutRecipe = *Row;
	return true;
}

bool UCraftComponent::GetPreviewResult(FDataTableRowHandle& OutItemRow, int32& OutCount, int32& OutGoldCost, bool& bOutCanAfford) const
{
	FCraftRecipeRow Recipe;
	if (!GetSelectedRecipe(Recipe))
	{
		return false;
	}
	OutItemRow = Recipe.ResultItem;
	OutCount = Recipe.ResultCount;
	OutGoldCost = Recipe.GoldCost;
	bOutCanAfford = GetCurrentGold() >= Recipe.GoldCost;
	return true;
}

bool UCraftComponent::CanCraft() const
{
	FCraftRecipeRow Recipe;
	if (!GetSelectedRecipe(Recipe))
	{
		return false;
	}
	if (!IsRecipeUnlocked(Recipe))
	{
		return false;
	}
	if (GetCurrentGold() < Recipe.GoldCost)
	{
		return false;
	}
	for (int32 i = 0; i < Recipe.Ingredients.Num(); ++i)
	{
		if (!Slots[i].IsValid())
		{
			return false;
		}
		if (Slots[i].ItemRow != Recipe.Ingredients[i].ItemRow)
		{
			return false;
		}
		if (Slots[i].StackCount < Recipe.Ingredients[i].Count)
		{
			return false;
		}
	}
	return true;
}

bool UCraftComponent::IsRecipeUnlocked(const FCraftRecipeRow& InRecipe) const
{
	if (!TargetStash || InRecipe.RecipeItem.IsNull())
	{
		return false;
	}
	for (const FInventoryEntry& Entry : TargetStash->GetEntries())
	{
		if (Entry.Item.ItemRow == InRecipe.RecipeItem)
		{
			return true;
		}
	}
	return false;
}

void UCraftComponent::PlaceToFirstMatchingSlot(FGuid InInstanceId)
{
	FCraftRecipeRow Recipe;
	if (!GetSelectedRecipe(Recipe))
	{
		return;
	}
	for (int32 i = 0; i < Recipe.Ingredients.Num(); ++i)
	{
		if (Slots[i].IsValid())
		{
			continue;
		}
		const int32 EntryIndex = TargetStash ? TargetStash->FindEntryIndexById(InInstanceId) : INDEX_NONE;
		if (EntryIndex == INDEX_NONE)
		{
			return;
		}
		const FItemInstance& Item = TargetStash->GetEntries()[EntryIndex].Item;
		if (Item.ItemRow.RowName == Recipe.Ingredients[i].ItemRow.RowName)
		{
			Server_PlaceItem(InInstanceId, i);
			return;
		}
	}
}

bool UCraftComponent::GetItemViewData(const FDataTableRowHandle& InRow, FItemViewData& OutViewData)
{
	return ResolveItemViewData(InRow, OutViewData);
}

void UCraftComponent::SelectRecipe(FName InRowName)
{
	Server_SelectRecipe(InRowName);
}

void UCraftComponent::AutoFillFromStash()
{
	Server_AutoFillFromStash();
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

void UCraftComponent::OnRep_Slots()
{
	OnCraftSlotsChanged.Broadcast();
}

void UCraftComponent::OnRep_SelectedRecipeRowName()
{
	OnRecipeSelected.Broadcast();
}

void UCraftComponent::Server_SetCraftMode_Implementation(ECraftMode InMode)
{
	CraftMode = InMode;
}

void UCraftComponent::Server_SelectRecipe_Implementation(FName InRowName)
{
	if (SelectedRecipeRowName == InRowName)
	{
		return;
	}
	Server_ReturnAllToStash_Implementation();
	SelectedRecipeRowName = InRowName;
	OnRecipeSelected.Broadcast();
	OnCraftSlotsChanged.Broadcast();
}

void UCraftComponent::Server_AutoFillFromStash_Implementation()
{
	if (!TargetStash)
	{
		return;
	}
	FCraftRecipeRow Recipe;
	if (!GetSelectedRecipe(Recipe))
	{
		return;
	}
	for (int32 i = 0; i < Recipe.Ingredients.Num(); ++i)
	{
		if (Slots[i].IsValid())
		{
			continue;
		}
		const FCraftIngredient& Ingredient = Recipe.Ingredients[i];
		for (const FInventoryEntry& Entry : TargetStash->GetEntries())
		{
			if (Entry.Item.ItemRow == Ingredient.ItemRow && Entry.Item.StackCount >= Ingredient.Count)
			{
				Server_PlaceItem_Implementation(Entry.Item.InstanceId, i);
				break;
			}
		}
	}
}

void UCraftComponent::Server_PlaceItem_Implementation(FGuid InInstanceId, int32 InSlotIndex)
{
	if (TargetStash == nullptr || !Slots.IsValidIndex(InSlotIndex))
	{
		return;
	}

	FCraftRecipeRow Recipe;
	if (!GetSelectedRecipe(Recipe) || !Recipe.Ingredients.IsValidIndex(InSlotIndex))
	{
		return;
	}

	if (Slots[InSlotIndex].IsValid())
	{
		ReturnSlotToStash(InSlotIndex);
	}

	const int32 EntryIndex = TargetStash->FindEntryIndexById(InInstanceId);
	if (EntryIndex == INDEX_NONE)
	{
		return;
	}

	const FItemInstance Placed = TargetStash->GetEntries()[EntryIndex].Item;
	const FCraftIngredient& Required = Recipe.Ingredients[InSlotIndex];

	if (Placed.ItemRow.RowName != Required.ItemRow.RowName)
	{
		return;
	}
	if (Placed.StackCount < Required.Count)
	{
		return;
	}

	if (!TargetStash->RemoveStackCount(InInstanceId, Placed.StackCount))
	{
		return;
	}

	Slots[InSlotIndex] = Placed;
	OnRep_Slots();
}

void UCraftComponent::Server_RemoveItem_Implementation(int32 InSlotIndex)
{
	if (!Slots.IsValidIndex(InSlotIndex) || !Slots[InSlotIndex].IsValid())
	{
		return;
	}
	ReturnSlotToStash(InSlotIndex);
	OnCraftSlotsChanged.Broadcast();
}

void UCraftComponent::Server_ExecuteCraft_Implementation()
{
	if (TargetStash == nullptr)
	{
		OnCraftCompleted.Broadcast(false);
		return;
	}

	FCraftRecipeRow Recipe;
	if (!GetSelectedRecipe(Recipe))
	{
		OnCraftCompleted.Broadcast(false);
		return;
	}

	if (!CanCraft())
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

	for (int32 i = 0; i < Recipe.Ingredients.Num(); ++i)
	{
		Slots[i] = FItemInstance();
	}

	int32 Remainder = 0;
	TargetStash->TryAddItem(Result, Remainder);

	if (Recipe.IsLimitedUse())
	{
		ConsumeRecipeItemIfLimited(Recipe);
	}

	OnRep_Slots();
	OnCraftCompleted.Broadcast(true);
}

void UCraftComponent::Server_ReturnAllToStash_Implementation()
{
	bool bAnyReturned = false;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].IsValid())
		{
			ReturnSlotToStash(i);
			bAnyReturned = true;
		}
	}
	if (bAnyReturned)
	{
		OnCraftSlotsChanged.Broadcast();
	}
}

bool UCraftComponent::ReturnSlotToStash(int32 InSlotIndex)
{
	if (!TargetStash || !Slots.IsValidIndex(InSlotIndex) || !Slots[InSlotIndex].IsValid())
	{
		return false;
	}
	int32 Remainder = 0;
	TargetStash->TryAddItem(Slots[InSlotIndex], Remainder);
	Slots[InSlotIndex] = FItemInstance();
	return true;
}

bool UCraftComponent::ConsumeRecipeItemIfLimited(const FCraftRecipeRow& InRecipe)
{
	if (!TargetStash || InRecipe.RecipeItem.IsNull())
	{
		return false;
	}
	for (const FInventoryEntry& Entry : TargetStash->GetEntries())
	{
		if (Entry.Item.ItemRow == InRecipe.RecipeItem)
		{
			return TargetStash->RemoveStackCount(Entry.Item.InstanceId, 1);
		}
	}
	return false;
}