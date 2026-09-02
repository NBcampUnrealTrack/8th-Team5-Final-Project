// Copyright RelicsOfTheFallenHeaven. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShopTypes.h"
#include "ShopComponent.generated.h"

class UInventoryComponent;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBasketChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTradeCompleted, bool, bSuccess, const FString&, Message);

UCLASS(ClassGroup = (Shop), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShopComponent();

	void SetShopInventoryTable(UDataTable* InTable) { ShopInventoryTable = InTable; }

	void SetGoldItemRow(const FDataTableRowHandle& InRow) { GoldItemRow = InRow; }

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FOnBasketChanged OnBasketChanged;

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FOnTradeCompleted OnTradeCompleted;

	UFUNCTION(BlueprintPure, Category = "Shop")
	TArray<FShopEntry> GetShopEntries() const;

	UFUNCTION(BlueprintPure, Category = "Shop")
	const TArray<FTradeBasketEntry>& GetBasket() const { return Basket; }

	UFUNCTION(BlueprintPure, Category = "Shop")
	ETradeMode GetTradeMode() const { return TradeMode; }

	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetBasketTotal() const;

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetTradeMode(ETradeMode InMode);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void SetTargetStash(UInventoryComponent* InStash);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool AddToBuyBasket(const FDataTableRowHandle& InItemRow, int32 InQuantity);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool AddToSellBasket(FGuid InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void RemoveFromBasket(int32 InIndex);

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ClearBasket();

	UFUNCTION(BlueprintCallable, Category = "Shop")
	void ConfirmTrade();

	UFUNCTION(Server, Reliable)
	void Server_ExecuteTrade(const TArray<FTradeBasketEntry>& InBasket, ETradeMode InMode, UInventoryComponent* InStash);

	UFUNCTION(Client, Reliable)
	void Client_TradeResult(bool bSuccess, const FString& InMessage);

	UFUNCTION(BlueprintPure, Category = "Shop")
	bool IsInBasket(FGuid InInstanceId) const;


protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TObjectPtr<UDataTable> ShopInventoryTable;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	float BuyPriceMultiplier = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	FDataTableRowHandle GoldItemRow;

private:
	int32 CalculateBuyPrice(const FItemViewData& InViewData) const;

	UPROPERTY()
	TArray<FTradeBasketEntry> Basket;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> TargetStash;

	ETradeMode TradeMode = ETradeMode::Buy;
};