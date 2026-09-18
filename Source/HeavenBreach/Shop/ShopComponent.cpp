#include "ShopComponent.h"
#include "UI/Audio/HBUIAudioLibrary.h"

#include "Engine/DataTable.h"
#include "InventoryComponent.h"
#include "StashComponent.h"

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

	for (FTradeBasketEntry& Entry : Basket)
	{
		if (Entry.ItemRow.RowName == InItemRow.RowName)
		{
			Entry.Quantity += InQuantity;
			OnBasketChanged.Broadcast();
			return true;
		}
	}

	FTradeBasketEntry NewEntry;
	NewEntry.ItemRow = InItemRow;
	NewEntry.Quantity = InQuantity;
	NewEntry.UnitPrice = CalculateBuyPrice(ViewData);
	NewEntry.ViewData = ViewData;

	Basket.Add(NewEntry);

	OnBasketChanged.Broadcast();

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
		Client_TradeResult(false, TEXT("거래할 수 없습니다."));
		return;
	}

	if (GoldItemRow.IsNull())
	{
		Client_TradeResult(false, TEXT("골드 설정이 없습니다."));
		return;
	}

	int32 TotalPrice = 0;
	for (const FTradeBasketEntry& Entry : InBasket)
	{
		TotalPrice += Entry.UnitPrice * Entry.Quantity;
	}

	if (InMode == ETradeMode::Buy)
	{
		if (InStash->GetTotalGold(GoldItemRow.RowName) < TotalPrice)
		{
			Client_TradeResult(false, TEXT("골드가 부족합니다."));
			return;
		}

		for (const FTradeBasketEntry& Entry : InBasket)
		{
			FItemViewData ViewData;
			if (!ResolveItemViewData(Entry.ItemRow, ViewData))
			{
				Client_TradeResult(false, TEXT("아이템 정보를 찾을 수 없습니다."));
				return;
			}

			FIntPoint FreePosition;
			if (!InStash->FindFirstFreeSlot(ViewData.GridSize, FreePosition))
			{
				Client_TradeResult(false, TEXT("창고에 공간이 부족합니다."));
				return;
			}
		}

		if (!InStash->ConsumeGold(GoldItemRow, TotalPrice))
		{
			Client_TradeResult(false, TEXT("거래에 실패했습니다."));
			return;
		}

		for (const FTradeBasketEntry& Entry : InBasket)
		{
			for (int32 Count = 0; Count < Entry.Quantity; ++Count)
			{
				FItemInstance NewItem;
				NewItem.ItemRow = Entry.ItemRow;
				NewItem.InstanceId = FGuid::NewGuid();
				NewItem.StackCount = 1;

				if (Entry.ViewData.IsEquippable())
				{
					NewItem.CurrentDurability = GetMaxDurabilityForGrade(Entry.ViewData.Grade);
				}

				int32 Remainder = 0;
				InStash->TryAddItem(NewItem, Remainder);
			}
		}

		if (UStashComponent* Stash = Cast<UStashComponent>(InStash))
		{
			Stash->SaveStash();
		}

		Client_TradeResult(true, TEXT("구매가 완료되었습니다."));
		return;
	}

	for (const FTradeBasketEntry& Entry : InBasket)
	{
		if (InStash->FindEntryIndexById(Entry.InstanceId) == INDEX_NONE)
		{
			Client_TradeResult(false, TEXT("판매할 아이템을 찾을 수 없습니다."));
			return;
		}
	}

	for (const FTradeBasketEntry& Entry : InBasket)
	{
		FItemInstance Removed;
		InStash->RemoveItem(Entry.InstanceId, Removed);
	}

	if (!InStash->AddGold(GoldItemRow, TotalPrice))
	{
		Client_TradeResult(false, TEXT("골드를 받을 공간이 없습니다."));
		return;
	}

	if (UStashComponent* Stash = Cast<UStashComponent>(InStash))
	{
		Stash->SaveStash();
	}

	Client_TradeResult(true, TEXT("판매가 완료되었습니다."));
}