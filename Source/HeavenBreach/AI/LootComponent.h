// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryComponent.h"
#include "LootComponent.generated.h"

class UDataTable;
struct FBoxLootData;

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API ULootComponent : public UInventoryComponent
{
	GENERATED_BODY()
	
public:
	// ----- InventoryComponent -----

	// 루팅 가능한 아이템을 채워넣기
	void GenerateLoot();

	// 클라에게 UI열라고 명령
	void HandleInteract(AActor* Interactor);

	FText GetPrompt() const { return InteractPrompt; }

protected:
	// ----- Item/Loot -----

	// 상호작용시 띄울 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FText InteractPrompt = FText::FromString(TEXT("상자 열기 [F]"));

	// 데이터 테이블에 대조할 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FString LootID;

	// 드랍 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDataTable> LootTable;

	// 아이템 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDataTable> ItemDataTable;

private:
	// 테이블에 있는 아이템 무작위로 인벤토리에 넣기
	// true로 반환해야 성공적으로 들어간것이다. (Remainder 가1 이상나옴 = 못들어갓음)
	bool AddLootItem(const FBoxLootData& Row);

};
