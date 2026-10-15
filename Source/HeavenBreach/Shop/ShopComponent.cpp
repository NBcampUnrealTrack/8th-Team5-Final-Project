#include "ShopComponent.h"
#include "UI/Audio/HBUIAudioLibrary.h"

#include "Engine/DataTable.h"
#include "Framework/Application/SlateApplication.h"
#include "InventoryComponent.h"
#include "StashComponent.h"
#include "Inventory/ItemSortRules.h"

UShopComponent::UShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UShopComponent::IsInBasket(FGuid InInstanceId) const
{
	for (const FTradeBasketEntry& Entry : Basket)
	{
		if (Entry.InstanceId == InInstanceId)
		{
			return true;
		}
	}

	return false;
}

void UShopComponent::BeginPlay()
{
	Super::BeginPlay();
}

int32 UShopComponent::CalculateBuyPrice(const FItemViewData& InViewData) const
{
	return FMath::RoundToInt(InViewData.SellValue * BuyPriceMultiplier);
}

bool UShopComponent::IsShiftHeld()
{
	return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
}

int32 UShopComponent::GetMaxBuyQuantity(const FDataTableRowHandle& InItemRow) const
{
	int32 Limit = FMath::Max(1, MaxBuyQuantity);

	if (ShopInventoryTable == nullptr)
	{
		return Limit;
	}

	static const FString ContextString(TEXT("GetMaxBuyQuantity"));

	TArray<FShopItemRow*> Rows;
	ShopInventoryTable->GetAllRows(ContextString, Rows);

	for (const FShopItemRow* Row : Rows)
	{
		if (Row && Row->ItemRow.RowName == InItemRow.RowName && Row->Stock >= 0)
		{
			Limit = FMath::Min(Limit, Row->Stock);
			break;
		}
	}

	return Limit;
}

TArray<FShopEntry> UShopComponent::GetShopEntries() const
{
	TArray<FShopEntry> Entries;

	if (ShopInventoryTable == nullptr)
	{
		return Entries;
	}

	static const FString ContextString(TEXT("GetShopEntries"));

	TArray<FShopItemRow*> Rows;
	ShopInventoryTable->GetAllRows(ContextString, Rows);

	for (const FShopItemRow* Row : Rows)
	{
		if (Row == nullptr)
		{
			continue;
		}

		FItemViewData ViewData;
		if (!ResolveItemViewData(Row->ItemRow, ViewData))
		{
			continue;
		}

		FShopEntry Entry;
		Entry.ItemRow = Row->ItemRow;
		Entry.Stock = Row->Stock;
		Entry.ViewData = ViewData;
		Entry.BuyPrice = CalculateBuyPrice(ViewData);

		Entries.Add(Entry);
	}

	Entries.StableSort([](const FShopEntry& A, const FShopEntry& B)
	{
		const int32 RankA = HBItemSort::GetCategoryRank(A.ItemRow, A.ViewData);
		const int32 RankB = HBItemSort::GetCategoryRank(B.ItemRow, B.ViewData);
		if (RankA != RankB)
		{
			return RankA < RankB;
		}
		if (A.ViewData.Grade != B.ViewData.Grade)
		{
			return A.ViewData.Grade < B.ViewData.Grade;
		}
		return A.BuyPrice < B.BuyPrice;
	});

	return Entries;
}

int32 UShopComponent::GetBasketTotal() const
{
	int32 Total = 0;

	for (const FTradeBasketEntry& Entry : Basket)
	{
		Total += Entry.UnitPrice * Entry.Quantity;
	}

	return Total;
}

void UShopComponent::SetTradeMode(ETradeMode InMode)
{
	if (TradeMode == InMode)
	{
		return;
	}

	TradeMode = InMode;
	ClearBasket();
}

void UShopComponent::SetTargetStash(UInventoryComponent* InStash)
{
	TargetStash = InStash;
}

bool UShopComponent::AddToBuyBasket(const FDataTableRowHandle& InItemRow, int32 InQuantity)
{
	if (TradeMode != ETradeMode::Buy || InQuantity <= 0)
	{
		return false;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(InItemRow, ViewData))
	{
		return false;
	}

	const int32 MaxQuantity = GetMaxBuyQuantity(InItemRow);
	if (MaxQuantity <= 0)
	{
		UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Error);
		return false;
	}

	for (FTradeBasketEntry& Entry : Basket)
	{
		if (Entry.ItemRow.RowName == InItemRow.RowName)
		{
			const int32 NewQuantity = FMath::Min(Entry.Quantity + InQuantity, MaxQuantity);
			if (NewQuantity == Entry.Quantity)
			{
				UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Error);
				return false;
			}

			Entry.Quantity = NewQuantity;
			OnBasketChanged.Broadcast();
			UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::ItemMove);
			return true;
		}
	}

	FTradeBasketEntry NewEntry;
	NewEntry.ItemRow = InItemRow;
	NewEntry.Quantity = FMath::Min(InQuantity, MaxQuantity);
	NewEntry.UnitPrice = CalculateBuyPrice(ViewData);
	NewEntry.ViewData = ViewData;

	Basket.Add(NewEntry);

	OnBasketChanged.Broadcast();
	UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::ItemMove);

	return true;
}

bool UShopComponent::AddToSellBasket(FGuid InInstanceId)
{
	if (TradeMode != ETradeMode::Sell || TargetStash == nullptr)
	{
		return false;
	}

	for (const FTradeBasketEntry& Entry : Basket)
	{
		if (Entry.InstanceId == InInstanceId)
		{
			return false;
		}
	}

	const int32 EntryIndex = TargetStash->FindEntryIndexById(InInstanceId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	const FItemInstance& Item = TargetStash->GetEntries()[EntryIndex].Item;

	if (Item.BagContents.Num() > 0 || Item.bLocked)
	{
		UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Error);
		return false;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(Item.ItemRow, ViewData))
	{
		return false;
	}

	FTradeBasketEntry NewEntry;
	NewEntry.ItemRow = Item.ItemRow;
	NewEntry.InstanceId = InInstanceId;
	NewEntry.Quantity = Item.StackCount;
	NewEntry.UnitPrice = ViewData.SellValue;
	NewEntry.ViewData = ViewData;

	Basket.Add(NewEntry);

	OnBasketChanged.Broadcast();
	UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::ItemMove);

	return true;
}

void UShopComponent::RemoveFromBasket(int32 InIndex)
{
	if (!Basket.IsValidIndex(InIndex))
	{
		return;
	}

	if (TradeMode == ETradeMode::Buy && Basket[InIndex].Quantity > 1)
	{
		Basket[InIndex].Quantity -= 1;
	}
	else
	{
		Basket.RemoveAt(InIndex);
	}

	OnBasketChanged.Broadcast();
	UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Cancel);
}

bool UShopComponent::SetBasketQuantity(int32 InIndex, int32 InQuantity)
{
	if (TradeMode != ETradeMode::Buy || !Basket.IsValidIndex(InIndex))
	{
		return false;
	}

	const int32 NewQuantity = FMath::Min(InQuantity, GetMaxBuyQuantity(Basket[InIndex].ItemRow));
	if (NewQuantity <= 0)
	{
		RemoveBasketEntry(InIndex);
		return true;
	}

	if (NewQuantity == Basket[InIndex].Quantity)
	{
		OnBasketChanged.Broadcast();
		return false;
	}

	const bool bIncreased = NewQuantity > Basket[InIndex].Quantity;
	Basket[InIndex].Quantity = NewQuantity;

	OnBasketChanged.Broadcast();
	UHBUIAudioLibrary::PlayForOwner(this, bIncreased ? EHBUIAudioEvent::ItemMove : EHBUIAudioEvent::Cancel);

	return true;
}

bool UShopComponent::AddBasketQuantity(int32 InIndex, int32 InDelta)
{
	if (!Basket.IsValidIndex(InIndex) || InDelta == 0)
	{
		return false;
	}

	return SetBasketQuantity(InIndex, Basket[InIndex].Quantity + InDelta);
}

void UShopComponent::RemoveBasketEntry(int32 InIndex)
{
	if (!Basket.IsValidIndex(InIndex))
	{
		return;
	}

	Basket.RemoveAt(InIndex);

	OnBasketChanged.Broadcast();
	UHBUIAudioLibrary::PlayForOwner(this, EHBUIAudioEvent::Cancel);
}

void UShopComponent::ClearBasket()
{
	if (Basket.Num() == 0)
	{
		return;
	}

	Basket.Reset();

	OnBasketChanged.Broadcast();
}

void UShopComponent::ConfirmTrade()
{
	if (Basket.Num() == 0 || TargetStash == nullptr)
	{
		return;
	}

	Server_ExecuteTrade(Basket, TradeMode, TargetStash);
}

void UShopComponent::Client_TradeResult_Implementation(bool bSuccess, const FString& InMessage)
{
	if (bSuccess)
	{
		ClearBasket();
	}

	UHBUIAudioLibrary::PlayForOwner(this, bSuccess ? EHBUIAudioEvent::TradeSuccess : EHBUIAudioEvent::Error);
	OnTradeCompleted.Broadcast(bSuccess, InMessage);
}

void UShopComponent::Server_ExecuteTrade_Implementation(const TArray<FTradeBasketEntry>& InBasket, ETradeMode InMode, UInventoryComponent* InStash)
{
	if (InStash == nullptr || InBasket.Num() == 0)
	{
		Client_TradeResult(false, TEXT("\uAC70\uB798\uD560 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4."));
		return;
	}

	if (GoldItemRow.IsNull())
	{
		Client_TradeResult(false, TEXT("\uACE8\uB4DC \uC124\uC815\uC774 \uC5C6\uC2B5\uB2C8\uB2E4."));
		return;
	}

	TArray<FTradeBasketEntry> Verified;
	Verified.Reserve(InBasket.Num());

	int32 TotalPrice = 0;
	for (const FTradeBasketEntry& Entry : InBasket)
	{
		FTradeBasketEntry Checked = Entry;

		if (InMode == ETradeMode::Buy)
		{
			if (!ResolveItemViewData(Entry.ItemRow, Checked.ViewData)
				|| Entry.Quantity <= 0
				|| Entry.Quantity > GetMaxBuyQuantity(Entry.ItemRow))
			{
				Client_TradeResult(false, TEXT("\uAC70\uB798\uD560 \uC218 \uC5C6\uB294 \uD56D\uBAA9\uC774 \uC788\uC2B5\uB2C8\uB2E4."));
				return;
			}
			Checked.UnitPrice = CalculateBuyPrice(Checked.ViewData);
		}
		else
		{
			const int32 SellIndex = InStash->FindEntryIndexById(Entry.InstanceId);
			if (SellIndex == INDEX_NONE)
			{
				Client_TradeResult(false, TEXT("\uD310\uB9E4\uD560 \uC544\uC774\uD15C\uC744 \uCC3E\uC744 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4."));
				return;
			}

			const FItemInstance& Item = InStash->GetEntries()[SellIndex].Item;
			if (!ResolveItemViewData(Item.ItemRow, Checked.ViewData))
			{
				Client_TradeResult(false, TEXT("\uC544\uC774\uD15C \uC815\uBCF4\uB97C \uCC3E\uC744 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4."));
				return;
			}
			Checked.ItemRow = Item.ItemRow;
			Checked.Quantity = Item.StackCount;
			Checked.UnitPrice = Checked.ViewData.SellValue;
		}

		TotalPrice += Checked.UnitPrice * Checked.Quantity;
		Verified.Add(Checked);
	}

	if (InMode == ETradeMode::Buy)
	{
		if (InStash->GetTotalGold(GoldItemRow.RowName) < TotalPrice)
		{
			Client_TradeResult(false, TEXT("\uACE8\uB4DC\uAC00 \uBD80\uC871\uD569\uB2C8\uB2E4."));
			return;
		}

		for (const FTradeBasketEntry& Entry : Verified)
		{
			FItemViewData ViewData;
			if (!ResolveItemViewData(Entry.ItemRow, ViewData))
			{
				Client_TradeResult(false, TEXT("\uC544\uC774\uD15C \uC815\uBCF4\uB97C \uCC3E\uC744 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4."));
				return;
			}

			FIntPoint FreePosition;
			if (!InStash->FindFirstFreeSlot(ViewData.GridSize, FreePosition))
			{
				Client_TradeResult(false, TEXT("\uCC3D\uACE0\uC5D0 \uACF5\uAC04\uC774 \uBD80\uC871\uD569\uB2C8\uB2E4."));
				return;
			}
		}

		if (!InStash->ConsumeGold(GoldItemRow, TotalPrice))
		{
			Client_TradeResult(false, TEXT("\uAC70\uB798\uC5D0 \uC2E4\uD328\uD588\uC2B5\uB2C8\uB2E4."));
			return;
		}

		int32 FailedCount = 0;
		int32 RefundGold = 0;

		for (const FTradeBasketEntry& Entry : Verified)
		{
			const bool bStackable = Entry.ViewData.IsStackable();
			const int32 AddCount = bStackable ? 1 : Entry.Quantity;

			for (int32 Count = 0; Count < AddCount; ++Count)
			{
				FItemInstance NewItem;
				NewItem.ItemRow = Entry.ItemRow;
				NewItem.InstanceId = FGuid::NewGuid();
				NewItem.StackCount = bStackable ? Entry.Quantity : 1;

				if (Entry.ViewData.IsEquippable())
				{
					NewItem.CurrentDurability = GetMaxDurabilityForGrade(Entry.ViewData.Grade);
				}

				int32 Remainder = 0;
				InStash->TryAddItem(NewItem, Remainder);
				if (Remainder > 0)
				{
					FailedCount += Remainder;
					RefundGold += Entry.UnitPrice * Remainder;
				}
			}
		}

		if (RefundGold > 0)
		{
			InStash->AddGold(GoldItemRow, RefundGold);
		}

		if (UStashComponent* Stash = Cast<UStashComponent>(InStash))
		{
			Stash->SaveStash();
		}

		if (FailedCount > 0)
		{
			Client_TradeResult(true, FString::Printf(TEXT("\uCC3D\uACE0 \uACF5\uAC04\uC774 \uBD80\uC871\uD574 %d\uAC1C\uB294 \uAD6C\uB9E4\uD558\uC9C0 \uBABB\uD588\uC2B5\uB2C8\uB2E4. \uACE8\uB4DC\uB294 \uB3CC\uB824\uBC1B\uC558\uC2B5\uB2C8\uB2E4."), FailedCount));
			return;
		}

		Client_TradeResult(true, TEXT("\uAD6C\uB9E4\uAC00 \uC644\uB8CC\uB418\uC5C8\uC2B5\uB2C8\uB2E4."));
		return;
	}

	for (const FTradeBasketEntry& Entry : Verified)
	{
		const int32 SellIndex = InStash->FindEntryIndexById(Entry.InstanceId);
		if (SellIndex != INDEX_NONE && InStash->GetEntries()[SellIndex].Item.BagContents.Num() > 0)
		{
			Client_TradeResult(false, TEXT("\uAC00\uBC29\uC744 \uBE44\uC6B4 \uB4A4 \uD310\uB9E4\uD558\uC138\uC694."));
			return;
		}
		if (SellIndex == INDEX_NONE)
		{
			Client_TradeResult(false, TEXT("\uD310\uB9E4\uD560 \uC544\uC774\uD15C\uC744 \uCC3E\uC744 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4."));
			return;
		}
		if (InStash->GetEntries()[SellIndex].Item.bLocked)
		{
			Client_TradeResult(false, TEXT("\uC7A0\uAE34 \uC544\uC774\uD15C\uC740 \uD310\uB9E4\uD560 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4."));
			return;
		}
	}

	for (const FTradeBasketEntry& Entry : Verified)
	{
		FItemInstance Removed;
		InStash->RemoveItem(Entry.InstanceId, Removed);
	}

	if (!InStash->AddGold(GoldItemRow, TotalPrice))
	{
		Client_TradeResult(false, TEXT("\uACE8\uB4DC\uB97C \uBC1B\uC744 \uACF5\uAC04\uC774 \uC5C6\uC2B5\uB2C8\uB2E4."));
		return;
	}

	if (UStashComponent* Stash = Cast<UStashComponent>(InStash))
	{
		Stash->SaveStash();
	}

	Client_TradeResult(true, TEXT("\uD310\uB9E4\uAC00 \uC644\uB8CC\uB418\uC5C8\uC2B5\uB2C8\uB2E4."));
}