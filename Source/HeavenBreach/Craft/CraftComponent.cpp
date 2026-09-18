#include "CraftComponent.h"
#include "UI/Audio/HBUIAudioLibrary.h"

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

	const int32 MaxSlots = Slots.Num();
	for (int32 i = 0; i < Recipe.Ingredients.Num() && i < MaxSlots; ++i)
	{
		const FCraftIngredient& Required = Recipe.Ingredients[i];

		const int32 CurrentCount = Slots[i].IsValid() ? Slots[i].StackCount : 0;
		if (CurrentCount >= Required.Count)
		{
			continue;
		}

		const int32 EntryIndex = TargetStash ? TargetStash->FindEntryIndexById(InInstanceId) : INDEX_NONE;
		if (EntryIndex == INDEX_NONE)
		{
			return;
		}
		const FItemInstance& Item = TargetStash->GetEntries()[EntryIndex].Item;
		if (Item.ItemRow.RowName == Required.ItemRow.RowName)
		{
			Server_PlaceItem(InInstanceId, i);
			return;
		}
	}
}

bool UCraftComponent::HasAnyFillableIngredient() const
{
	FCraftRecipeRow Recipe;
	if (!GetSelectedRecipe(Recipe))
	{
		return false;
	}

	if (!TargetStash)
	{
		return false;
	}

	const int32 MaxSlots = Slots.Num();
	for (int32 i = 0; i < Recipe.Ingredients.Num() && i < MaxSlots; ++i)
	{
		const FCraftIngredient& Ingredient = Recipe.Ingredients[i];

		if (Ingredient.ItemRow.IsNull())
		{
			continue;
		}

		if (Slots[i].IsValid() && Slots[i].ItemRow.RowName == Ingredient.ItemRow.RowName)
		{
			continue;
		}

		for (const FInventoryEntry& Entry : TargetStash->GetEntries())
		{
			if (!Entry.Item.IsValid())
			{
				continue;
			}

			if (Entry.Item.ItemRow.RowName == Ingredient.ItemRow.RowName && Entry.Item.StackCount > 0)
			{
				return true;
			}
		}
	}

	return false;
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
	const int32 MaxSlots = Slots.Num();
	for (int32 i = 0; i < Recipe.Ingredients.Num() && i < MaxSlots; ++i)
	{
		if (Slots[i].IsValid())
		{
			continue;
		}
		const FCraftIngredient& Ingredient = Recipe.Ingredients[i];

		int32 TotalCount = 0;
		TArray<TPair<FGuid, int32>> MatchingItems;
		for (const FInventoryEntry& Entry : TargetStash->GetEntries())
		{
			if (Entry.Item.ItemRow == Ingredient.ItemRow)
			{
				TotalCount += Entry.Item.StackCount;
				MatchingItems.Add(TPair<FGuid, int32>(Entry.Item.InstanceId, Entry.Item.StackCount));
				if (TotalCount >= Ingredient.Count)
				{
					break;
				}
			}
		}

		if (TotalCount < Ingredient.Count)
		{
			continue;
		}

		int32 Remaining = Ingredient.Count;
		FItemInstance SlotItem;
		bool bSlotInitialized = false;

		for (const TPair<FGuid, int32>& Pair : MatchingItems)
		{
			const int32 TakeCount = FMath::Min(Remaining, Pair.Value);

			if (!bSlotInitialized)
			{
				const int32 EntryIndex = TargetStash->FindEntryIndexById(Pair.Key);
				if (EntryIndex == INDEX_NONE)
				{
					continue;
				}
				SlotItem = TargetStash->GetEntries()[EntryIndex].Item;
				SlotItem.StackCount = 0;
				bSlotInitialized = true;
			}

			TargetStash->RemoveStackCount(Pair.Key, TakeCount);
			SlotItem.StackCount += TakeCount;
			Remaining -= TakeCount;

			if (Remaining <= 0)
			{
				break;
			}
		}

		if (bSlotInitialized)
		{
			Slots[i] = SlotItem;
		}
	}
	OnRep_Slots();
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

	const int32 CurrentCount = Slots[InSlotIndex].IsValid() ? Slots[InSlotIndex].StackCount : 0;
	const int32 Needed = Required.Count - CurrentCount;

	if (Needed <= 0)
	{
		return;
	}

	const int32 TakeCount = FMath::Min(Needed, Placed.StackCount);

	if (!TargetStash->RemoveStackCount(InInstanceId, TakeCount))
	{
		return;
	}

	if (!Slots[InSlotIndex].IsValid())
	{
		Slots[InSlotIndex] = Placed;
		Slots[InSlotIndex].StackCount = TakeCount;
	}
	else
	{
		Slots[InSlotIndex].StackCount += TakeCount;
	}

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

	FCraftRecipeRow SelectedRecipe;
	if (!GetSelectedRecipe(SelectedRecipe))
	{
		return;
	}

	if (TargetStash == nullptr)
	{
		Client_PlayCraftResult(false);
		OnCraftCompleted.Broadcast(false);
		return;
	}

	FCraftRecipeRow Recipe;
	if (!GetSelectedRecipe(Recipe))
	{
		Client_PlayCraftResult(false);
		OnCraftCompleted.Broadcast(false);
		return;
	}

	if (!CanCraft())
	{
		Client_PlayCraftResult(false);
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
		Client_PlayCraftResult(false);
		OnCraftCompleted.Broadcast(false);
		return;
	}

	if (Recipe.GoldCost > 0 && !TargetStash->ConsumeGold(GoldItemRow, Recipe.GoldCost))
	{
		Client_PlayCraftResult(false);
		OnCraftCompleted.Broadcast(false);
		return;
	}

	const int32 MaxSlots = Slots.Num();
	for (int32 i = 0; i < Recipe.Ingredients.Num() && i < MaxSlots; ++i)
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
	Client_PlayCraftResult(true);
	OnCraftCompleted.Broadcast(true);
}

void UCraftComponent::Client_PlayCraftResult_Implementation(bool bSuccess)
{
	UHBUIAudioLibrary::PlayForOwner(this, bSuccess ? EHBUIAudioEvent::CraftSuccess : EHBUIAudioEvent::Error);
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

	FItemViewData ViewData;
	UCraftComponent::GetItemViewData(Slots[InSlotIndex].ItemRow, ViewData);

	if (!ViewData.IsStackable() && Slots[InSlotIndex].StackCount > 1)
	{
		FItemInstance SingleItem = Slots[InSlotIndex];
		SingleItem.StackCount = 1;
		for (int32 j = 0; j < Slots[InSlotIndex].StackCount; ++j)
		{
			SingleItem.InstanceId = FGuid::NewGuid();
			int32 Remainder = 0;
			TargetStash->TryAddItem(SingleItem, Remainder);
		}
	}
	else
	{
		int32 Remainder = 0;
		TargetStash->TryAddItem(Slots[InSlotIndex], Remainder);
	}

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