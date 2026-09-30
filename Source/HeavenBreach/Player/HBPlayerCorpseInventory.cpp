#include "Player/HBPlayerCorpseInventory.h"

#include "Interaction/InteractionComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// 끝없이 늘어나지 않게 막는 상한. 가방 30칸 + 장비 6개로는 닿을 수 없는 크기다.
	constexpr int32 MaxCorpseRows = 32;
}

UHBPlayerCorpseInventory::UHBPlayerCorpseInventory()
{
	SetGridSize(CorpseColumns, CorpseRows);
}

void UHBPlayerCorpseInventory::FillFrom(const TArray<FItemInstance>& EquippedItems, const TArray<FInventoryEntry>& BagEntries)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	while (!TryFillAll(EquippedItems, BagEntries) && CorpseRows < MaxCorpseRows)
	{
		++CorpseRows;
	}

	// 칸 수가 바뀌었을 수 있으니 서버 캐시도 새 크기로 다시 맞춘다.
	SetEntries(GetEntries());
}

bool UHBPlayerCorpseInventory::TryFillAll(const TArray<FItemInstance>& EquippedItems, const TArray<FInventoryEntry>& BagEntries)
{
	// InitializeGrid 는 칸 수를 바꾸고 기존 항목을 비운다.
	InitializeGrid(CorpseColumns, CorpseRows);

	auto AddOne = [this](FItemInstance Item)
	{
		// 원래 가방에서 보이던 물건이다. 서칭 연출 없이 바로 보여준다.
		Item.bRevealed = true;
		int32 Remainder = 0;
		return TryAddItem(Item, Remainder) && Remainder == 0;
	};

	for (const FItemInstance& Item : EquippedItems)
	{
		if (!AddOne(Item))
		{
			return false;
		}
	}

	for (const FInventoryEntry& Entry : BagEntries)
	{
		if (!AddOne(Entry.Item))
		{
			return false;
		}
	}

	return true;
}

void UHBPlayerCorpseInventory::HandleInteract(AActor* Interactor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Interactor)
	{
		return;
	}

	if (UInteractionComponent* InteractionComp = Interactor->FindComponentByClass<UInteractionComponent>())
	{
		InteractionComp->Client_OpenContainer(this);
	}
}

void UHBPlayerCorpseInventory::OnRep_CorpseRows()
{
	SetGridSize(CorpseColumns, CorpseRows);
	// 항목이 칸 수보다 먼저 도착했을 수 있다. 새 크기로 캐시와 화면을 다시 맞춘다.
	OnRep_Entries();
}

void UHBPlayerCorpseInventory::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHBPlayerCorpseInventory, CorpseRows);
}
