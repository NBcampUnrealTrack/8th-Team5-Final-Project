// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LootComponent.h"

#include "Inventory/InventoryTypes.h"
#include "Interaction/InteractionComponent.h"
#include "Item/Item.h"
#include "Engine/DataTable.h"
#include "AI/Enum/EAlertState.h" 

void ULootComponent::GenerateLoot()
{
	// 서버에서만 동작해야함
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 드랍테이블과 아이템 테이블이 연결 확인
	if (!LootTable || !ItemDataTable)
	{
		return;
	}

	// 테이블의 데이터들을 모두 가져온다.
	TArray<FBoxLootData*> AllRows;
	LootTable->GetAllRows<FBoxLootData>(TEXT("ULootComponent::GenerateLoot"), AllRows);

	// 데이터 배열속에서 LootID와 BoxID가 일치하는 행만 추출한다.
	TArray<FBoxLootData*> Eligible;
	for (FBoxLootData* Row : AllRows)
	{
		if (Row && Row->BoxID == LootID)
		{
			Eligible.Add(Row);
		}
	}

	// 아무것도 찾지 못햇다면 종료
	if (Eligible.Num() == 0)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Loot] LootID '%s' 드랍 행 없음: %s"),*LootID, *GetNameSafe(GetOwner()));
		return;
	}

	// 테이블의 아이템을 추가한다.
	int32 AddedCount = 0;
	for (const FBoxLootData* Row : Eligible)
	{
		// 드랍찬스. 0~1사이
		if (FMath::FRand() <= Row->DropChance)
		{
			// 드랍되는 양은 설정된 1~아이템 최대 카운트까지 랜덤
			const int32 Count = FMath::RandRange(1, FMath::Max(1, Row->MaxItemCount));
			for (int32 i = 0; i < Count; ++i)
			{
				// 테이블의 아이템을 추가 후 성공적으로 옳겨진 아이템인지 판단(로그용)
				if (AddLootItem(*Row))
				{
					++AddedCount;
				}
			}
		}
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Loot] %s 드랍 %d개"), *GetNameSafe(GetOwner()), AddedCount);
}

void ULootComponent::HandleInteract(AActor* Interactor)
{
	// 서버에서만 동작해야함
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Interactor)
	{
		return;
	}

	// 플레이어의 인벤토리 UI열기위해 상호작용 컴포넌트에 보낸다.
	if (UInteractionComponent* InteractionComp = Interactor->FindComponentByClass<UInteractionComponent>())
	{
		InteractionComp->Client_OpenContainer(this);
	}
}

bool ULootComponent::AddLootItem(const FBoxLootData& Row)
{
	FItemInstance Item;
	Item.ItemRow.DataTable = ItemDataTable;
	Item.ItemRow.RowName = FName(*Row.ItemID);
	Item.InstanceId = FGuid::NewGuid();
	Item.StackCount = 1;

	int32 Remainder = 0;
	TryAddItem(Item, Remainder);

	// Remainder가 0일시 전부 들어갔다
	// 1이상이면 꽉차서 안들어갔다.
	return Remainder == 0;
}
