#include "InventoryComponent.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"
#include "EquipmentComponent.h"
#include "Item/ItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UInventoryComponent::Server_MoveItemIn_Implementation(UInventoryComponent* InTarget, FGuid InInstanceId, FIntPoint InNewPosition, bool InRotated)
{
	if (InTarget == nullptr)
	{
		return;
	}

	InTarget->MoveItem(InInstanceId, InNewPosition, InRotated);
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeGrid(Width, Height);
}

void UInventoryComponent::OnRep_Entries()
{
	RebuildCache();
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::InitializeGrid(int32 InWidth, int32 InHeight)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	Width = FMath::Max(1, InWidth);
	Height = FMath::Max(1, InHeight);

	Entries.Reset();
	RebuildCache();

	OnInventoryChanged.Broadcast();
}

bool UInventoryComponent::IsValidCoord(FIntPoint InCoord) const
{
	return InCoord.X >= 0 && InCoord.X < Width
		&& InCoord.Y >= 0 && InCoord.Y < Height;
}

int32 UInventoryComponent::GetEntryIndexAt(FIntPoint InCoord) const
{
	const int32 Index = CoordToIndex(InCoord);
	if (!CellCache.IsValidIndex(Index))
	{
		return INDEX_NONE;
	}
	return CellCache[Index];
}

int32 UInventoryComponent::FindEntryIndexById(FGuid InInstanceId) const
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].Item.InstanceId == InInstanceId)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 UInventoryComponent::GetTotalGold(FName InGoldRowName) const
{
	int32 Total = 0;

	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.Item.ItemRow.RowName == InGoldRowName)
		{
			Total += Entry.Item.StackCount;
		}
	}

	return Total;
}

bool UInventoryComponent::ConsumeGold(const FDataTableRowHandle& InGoldRow, int32 InAmount)
{
	if (GetOwnerRole() != ROLE_Authority || InAmount <= 0)
	{
		return false;
	}

	if (GetTotalGold(InGoldRow.RowName) < InAmount)
	{
		return false;
	}

	int32 Remaining = InAmount;

	for (int32 Index = Entries.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		if (Entries[Index].Item.ItemRow.RowName != InGoldRow.RowName)
		{
			continue;
		}

		const int32 Consumed = FMath::Min(Entries[Index].Item.StackCount, Remaining);
		Entries[Index].Item.StackCount -= Consumed;
		Remaining -= Consumed;

		if (Entries[Index].Item.StackCount <= 0)
		{
			Entries.RemoveAt(Index);
		}
	}

	RebuildCache();
	OnInventoryChanged.Broadcast();

	return true;
}

bool UInventoryComponent::AddGold(const FDataTableRowHandle& InGoldRow, int32 InAmount)
{
	if (GetOwnerRole() != ROLE_Authority || InAmount <= 0)
	{
		return false;
	}

	FItemInstance GoldItem;
	GoldItem.ItemRow = InGoldRow;
	GoldItem.InstanceId = FGuid::NewGuid();
	GoldItem.StackCount = InAmount;

	int32 Remainder = 0;
	return TryAddItem(GoldItem, Remainder);
}

bool UInventoryComponent::GetEntrySize(const FItemInstance& InItem, FIntPoint& OutSize) const
{
	FItemViewData ViewData;
	if (!ResolveItemViewData(InItem.ItemRow, ViewData))
	{
		return false;
	}

	OutSize = GetOccupiedSize(InItem, ViewData);
	return true;
}

bool UInventoryComponent::HasMergeableSpace(FName InRowName, int32 InMaxStack) const
{
	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.Item.ItemRow.RowName == InRowName && Entry.Item.StackCount < InMaxStack)
		{
			return true;
		}
	}

	return false;
}

bool UInventoryComponent::CanPlaceAt(FIntPoint InSize, FIntPoint InPosition, int32 InIgnoredIndex) const
{
	if (InSize.X <= 0 || InSize.Y <= 0)
	{
		return false;
	}

	if (InPosition.X < 0 || InPosition.Y < 0)
	{
		return false;
	}

	if (CellCache.Num() != Width * Height)
	{
		return false;
	}

	if (InPosition.X + InSize.X > Width || InPosition.Y + InSize.Y > Height)
	{
		return false;
	}

	for (int32 OffsetY = 0; OffsetY < InSize.Y; ++OffsetY)
	{
		for (int32 OffsetX = 0; OffsetX < InSize.X; ++OffsetX)
		{
			const FIntPoint Coord(InPosition.X + OffsetX, InPosition.Y + OffsetY);
			const int32 Occupant = CellCache[CoordToIndex(Coord)];

			if (Occupant != INDEX_NONE && Occupant != InIgnoredIndex)
			{
				return false;
			}
		}
	}

	return true;
}

bool UInventoryComponent::CanFitItem(const FItemInstance& InItem) const
{
	FItemViewData ViewData;

	if (!ResolveItemViewData(InItem.ItemRow, ViewData))
	{
		return false;
	}

	int32 Remaining = InItem.StackCount;

	if (ViewData.IsStackable())
	{
		for (const FInventoryEntry& Entry : Entries)
		{
			if (Remaining <= 0)
			{
				return true;
			}

			if (Entry.Item.ItemRow.RowName != InItem.ItemRow.RowName)
			{
				continue;
			}

			const int32 Space = ViewData.MaxStack - Entry.Item.StackCount;

			if (Space > 0)
			{
				Remaining -= FMath::Min(Space, Remaining);
			}
		}
	}

	if (Remaining <= 0)
	{
		return true;
	}

	if (Remaining > ViewData.MaxStack)
	{
		return false;
	}

	FIntPoint Size;

	if (!GetEntrySize(InItem, Size))
	{
		return false;
	}

	FIntPoint FreePosition;
	return FindFirstFreeSlot(Size, FreePosition);
}

bool UInventoryComponent::FindFirstFreeSlot(FIntPoint InSize, FIntPoint& OutPosition, int32 InIgnoredIndex) const
{
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FIntPoint Candidate(X, Y);
			if (CanPlaceAt(InSize, Candidate, InIgnoredIndex))
			{
				OutPosition = Candidate;
				return true;
			}
		}
	}

	return false;
}

bool UInventoryComponent::TryAddItem(const FItemInstance& InItem, int32& OutRemainder)
{
	OutRemainder = InItem.StackCount;

	FItemViewData ViewData;

	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!ResolveItemViewData(InItem.ItemRow, ViewData))
	{
		return false;
	}

	if (!ViewData.IsStackable() && InItem.StackCount > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("TryAddItem: non-stackable item with StackCount %d"), InItem.StackCount);
		return false;
	}

	if (ViewData.IsStackable())
	{
		for (FInventoryEntry& Entry : Entries)
		{
			if (OutRemainder <= 0)
			{
				break;
			}

			if (Entry.Item.ItemRow.RowName != InItem.ItemRow.RowName)
			{
				continue;
			}

			const int32 Space = ViewData.MaxStack - Entry.Item.StackCount;
			if (Space <= 0)
			{
				continue;
			}

			const int32 Moved = FMath::Min(Space, OutRemainder);
			Entry.Item.StackCount += Moved;
			OutRemainder -= Moved;
		}
	}

	const FIntPoint Size = GetOccupiedSize(InItem, ViewData);

	while (OutRemainder > 0)
	{
		FIntPoint FreePosition;
		if (!FindFirstFreeSlot(Size, FreePosition))
		{
			break;
		}

		FInventoryEntry NewEntry;
		NewEntry.Item = InItem;
		NewEntry.Item.StackCount = FMath::Min(ViewData.MaxStack, OutRemainder);
		NewEntry.Position = FreePosition;

		if (NewEntry.Item.StackCount < OutRemainder)
		{
			NewEntry.Item.InstanceId = FGuid::NewGuid();
		}

		Entries.Add(NewEntry);
		OutRemainder -= NewEntry.Item.StackCount;

		RebuildCache();
	}

	OnInventoryChanged.Broadcast();

	return OutRemainder == 0;
}

bool UInventoryComponent::TryAddItemAt(const FItemInstance& InItem, FIntPoint InPosition)
{
	FIntPoint Size;

	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!GetEntrySize(InItem, Size))
	{
		return false;
	}

	if (!CanPlaceAt(Size, InPosition))
	{
		return false;
	}

	FInventoryEntry NewEntry;
	NewEntry.Item = InItem;
	NewEntry.Position = InPosition;

	Entries.Add(NewEntry);
	RebuildCache();

	OnInventoryChanged.Broadcast();

	return true;
}

bool UInventoryComponent::MoveItem(FGuid InInstanceId, FIntPoint InNewPosition, bool bInRotated)
{
	const int32 EntryIndex = FindEntryIndexById(InInstanceId);

	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	// 회전 후 크기로 목표 위치에 들어갈 수 있는지까지 한 번에 검증한다.
	FItemInstance RotatedItem = Entries[EntryIndex].Item;
	RotatedItem.bRotated = bInRotated;

	FIntPoint Size;
	if (!GetEntrySize(RotatedItem, Size))
	{
		return false;
	}

	if (!CanPlaceAt(Size, InNewPosition, EntryIndex))
	{
		return false;
	}

	Entries[EntryIndex].Position = InNewPosition;
	Entries[EntryIndex].Item.bRotated = bInRotated;
	RebuildCache();

	OnInventoryChanged.Broadcast();

	return true;
}

bool UInventoryComponent::RemoveItem(FGuid InInstanceId, FItemInstance& OutRemoved)
{
	const int32 EntryIndex = FindEntryIndexById(InInstanceId);

	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	OutRemoved = Entries[EntryIndex].Item;

	Entries.RemoveAt(EntryIndex);
	RebuildCache();

	OnInventoryChanged.Broadcast();

	return true;
}

bool UInventoryComponent::RemoveStackCount(FGuid InInstanceId, int32 InCount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InCount <= 0)
	{
		return false;
	}

	const int32 EntryIndex = FindEntryIndexById(InInstanceId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	FItemInstance& Item = Entries[EntryIndex].Item;
	if (Item.StackCount < InCount)
	{
		return false;
	}

	Item.StackCount -= InCount;

	if (Item.StackCount <= 0)
	{
		Entries.RemoveAt(EntryIndex);
		RebuildCache();
	}

	OnInventoryChanged.Broadcast();

	return true;
}

bool UInventoryComponent::UseItem(FGuid InInstanceId)
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (bHasUsedItem && CurrentTime - LastItemUseTime < ItemUseCooldown)
	{
		return false;
	}
	const int32 EntryIndex = FindEntryIndexById(InInstanceId);
	if (EntryIndex == INDEX_NONE) return false;
	const FItemInstance Item = Entries[EntryIndex].Item;
	FItemViewData ViewData;
	if (!ResolveItemViewData(Item.ItemRow, ViewData)) return false;
	if (!ViewData.IsConsumable()) return false;

	// 최대 체력 상태에서는 체력 회복 아이템을 사용할 수 없다 (개발 기획 18장 남은 기능).
	if (ViewData.HealAmount > 0.f)
	{
		if (const AHB_PlayerState* OwnerPlayerState = Cast<AHB_PlayerState>(GetOwner()))
		{
			if (const UHB_PlayerAttributeSet* AttribSet = OwnerPlayerState->GetPlayerAttributeSet())
			{
				if (AttribSet->GetPlayer_HP() >= AttribSet->GetPlayer_MaxHP())
				{
					return false;
				}
			}
		}
	}

	bHasUsedItem = true;
	LastItemUseTime = CurrentTime;
	OnItemUsed.Broadcast(Item.ItemRow.RowName, ViewData.HealAmount);
	RemoveStackCount(InInstanceId, 1);
	return true;
}

bool UInventoryComponent::ToggleFavorite(FGuid InInstanceId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}
	const int32 EntryIndex = FindEntryIndexById(InInstanceId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}
	Entries[EntryIndex].Item.bIsFavorite = !Entries[EntryIndex].Item.bIsFavorite;
	OnInventoryChanged.Broadcast();
	return true;
}
bool UInventoryComponent::TransferItemFrom(UInventoryComponent* InSource, FGuid InInstanceId, FIntPoint InTargetPosition, bool bInRotated)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InSource == nullptr || InSource == this)
	{
		return false;
	}

	const int32 SourceIndex = InSource->FindEntryIndexById(InInstanceId);
	if (SourceIndex == INDEX_NONE)
	{
		return false;
	}

	FItemInstance SourceItem = InSource->GetEntries()[SourceIndex].Item;
	SourceItem.bRotated = bInRotated;

	FIntPoint Size;
	if (!GetEntrySize(SourceItem, Size))
	{
		return false;
	}

	if (!CanPlaceAt(Size, InTargetPosition))
	{
		return false;
	}

	FItemInstance RemovedItem;
	if (!InSource->RemoveItem(InInstanceId, RemovedItem))
	{
		return false;
	}
	RemovedItem.bRotated = bInRotated;

	if (!TryAddItemAt(RemovedItem, InTargetPosition))
	{
		int32 Remainder = 0;
		InSource->TryAddItem(RemovedItem, Remainder);
		return false;
	}

	return true;
}
bool UInventoryComponent::TransferItemFromAuto(UInventoryComponent* InSource, FGuid InInstanceId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InSource == nullptr || InSource == this)
	{
		return false;
	}

	const int32 SourceIndex = InSource->FindEntryIndexById(InInstanceId);
	if (SourceIndex == INDEX_NONE)
	{
		return false;
	}

	const FItemInstance SourceItem = InSource->GetEntries()[SourceIndex].Item;

	FItemViewData ViewData;
	if (!ResolveItemViewData(SourceItem.ItemRow, ViewData))
	{
		return false;
	}

	const FIntPoint Size = GetOccupiedSize(SourceItem, ViewData);

	FIntPoint FreePosition;
	const bool bHasFreeSlot = FindFirstFreeSlot(Size, FreePosition);
	const bool bCanMerge = ViewData.IsStackable() && HasMergeableSpace(SourceItem.ItemRow.RowName, ViewData.MaxStack);

	if (!bHasFreeSlot && !bCanMerge)
	{
		return false;
	}

	FItemInstance RemovedItem;
	if (!InSource->RemoveItem(InInstanceId, RemovedItem))
	{
		return false;
	}

	int32 Remainder = 0;
	if (!TryAddItem(RemovedItem, Remainder))
	{
		if (Remainder > 0)
		{
			FItemInstance Leftover = RemovedItem;
			Leftover.StackCount = Remainder;
			Leftover.InstanceId = FGuid::NewGuid();

			int32 SourceRemainder = 0;
			InSource->TryAddItem(Leftover, SourceRemainder);
		}

		return false;
	}

	return true;
}
bool UInventoryComponent::MergeStack(FGuid InSourceId, FGuid InTargetId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InSourceId == InTargetId)
	{
		return false;
	}

	const int32 SourceIndex = FindEntryIndexById(InSourceId);
	const int32 TargetIndex = FindEntryIndexById(InTargetId);

	if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE)
	{
		return false;
	}

	FItemInstance& SourceItem = Entries[SourceIndex].Item;
	FItemInstance& TargetItem = Entries[TargetIndex].Item;

	if (SourceItem.ItemRow.RowName != TargetItem.ItemRow.RowName
		|| SourceItem.ItemRow.DataTable != TargetItem.ItemRow.DataTable)
	{
		return false;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(SourceItem.ItemRow, ViewData) || !ViewData.IsStackable())
	{
		return false;
	}

	const int32 Space = ViewData.MaxStack - TargetItem.StackCount;
	if (Space <= 0)
	{
		return false;
	}

	const int32 Moved = FMath::Min(Space, SourceItem.StackCount);
	TargetItem.StackCount += Moved;
	SourceItem.StackCount -= Moved;

	if (SourceItem.StackCount <= 0)
	{
		Entries.RemoveAt(SourceIndex);
	}

	RebuildCache();
	OnInventoryChanged.Broadcast();

	return true;
}

bool UInventoryComponent::MergeStackFrom(UInventoryComponent* InSource, FGuid InSourceId, FGuid InTargetId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InSource == nullptr)
	{
		return false;
	}

	if (InSource == this)
	{
		return MergeStack(InSourceId, InTargetId);
	}

	const int32 SourceIndex = InSource->FindEntryIndexById(InSourceId);
	const int32 TargetIndex = FindEntryIndexById(InTargetId);

	if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE)
	{
		return false;
	}

	const FItemInstance SourceItem = InSource->GetEntries()[SourceIndex].Item;
	FItemInstance& TargetItem = Entries[TargetIndex].Item;

	if (SourceItem.ItemRow.RowName != TargetItem.ItemRow.RowName
		|| SourceItem.ItemRow.DataTable != TargetItem.ItemRow.DataTable)
	{
		return false;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(SourceItem.ItemRow, ViewData) || !ViewData.IsStackable())
	{
		return false;
	}

	const int32 Space = ViewData.MaxStack - TargetItem.StackCount;
	if (Space <= 0)
	{
		return false;
	}

	const int32 Moved = FMath::Min(Space, SourceItem.StackCount);
	TargetItem.StackCount += Moved;

	if (Moved >= SourceItem.StackCount)
	{
		FItemInstance Removed;
		InSource->RemoveItem(InSourceId, Removed);
	}
	else
	{
		InSource->RemoveStackCount(InSourceId, Moved);
	}

	RebuildCache();
	OnInventoryChanged.Broadcast();

	return true;
}
bool UInventoryComponent::DropItem(FGuid InInstanceId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	const int32 EntryIndex = FindEntryIndexById(InInstanceId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	const FDataTableRowHandle ItemRow = Entries[EntryIndex].Item.ItemRow;
	const int32 DroppedStackCount = Entries[EntryIndex].Item.StackCount;
	if (ItemRow.IsNull() || ItemRow.DataTable == nullptr)
	{
		return false;
	}

	const UScriptStruct* RowStruct = ItemRow.DataTable->GetRowStruct();
	if (RowStruct == nullptr)
	{
		return false;
	}

	EItemSourceTable SourceTable = EItemSourceTable::Item;
	if (RowStruct == FWeaponData::StaticStruct())
	{
		SourceTable = EItemSourceTable::Weapon;
	}
	else if (RowStruct == FShieldData::StaticStruct())
	{
		SourceTable = EItemSourceTable::Shield;
	}
	else if (RowStruct != FItemData::StaticStruct())
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (OwnerActor == nullptr || World == nullptr)
	{
		return false;
	}
	// Inventory 컴포넌트는 PlayerState에 붙어있는데, PlayerState는 실제로 필드를 돌아다니는
	// 액터가 아니라서 GetOwner()의 위치는 항상 고정된 값이 나옴. 실제 캐릭터(Pawn) 위치를
	// 써야 플레이어가 서 있는 곳에 정확히 떨어짐.
	AActor* SpawnBasisActor = OwnerActor;
	if (const APlayerState* OwnerPlayerState = Cast<APlayerState>(OwnerActor))
	{
		if (APawn* OwnerPawn = OwnerPlayerState->GetPawn())
		{
			SpawnBasisActor = OwnerPawn;
		}
	}
	TSubclassOf<AItemBase> ClassToSpawn = AItemBase::StaticClass();
	if (DropItemClass != nullptr)
	{
		ClassToSpawn = DropItemClass;
	}

	const FVector ForwardVector = SpawnBasisActor->GetActorForwardVector();
	const FVector SpawnLocation = SpawnBasisActor->GetActorLocation() + ForwardVector * 60.f + FVector(0.f, 0.f, 60.f);
	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	AItemBase* NewItemActor = World->SpawnActorDeferred<AItemBase>(ClassToSpawn, SpawnTransform);
	if (NewItemActor == nullptr)
	{
		return false;
	}

	NewItemActor->ItemID = ItemRow.RowName;
	NewItemActor->SourceTable = SourceTable;

	UDataTable* MutableDataTable = const_cast<UDataTable*>(ItemRow.DataTable.Get());

	switch (SourceTable)
	{
	case EItemSourceTable::Weapon:
		NewItemActor->WeaponDataTable = MutableDataTable;
		break;
	case EItemSourceTable::Shield:
		NewItemActor->ShieldDataTable = MutableDataTable;
		break;
	case EItemSourceTable::Item:
	default:
		NewItemActor->ItemDataTable = MutableDataTable;
		break;
	}

	NewItemActor->StackCount = FMath::Max(1, DroppedStackCount);
	NewItemActor->FinishSpawning(SpawnTransform);

	// 던진 사람과는 한동안 충돌 무시해서 스폰 직후 캐릭터에 끼어서 튕기는 걸 방지.
	if (NewItemActor->MeshComponent)
	{
		NewItemActor->MeshComponent->IgnoreActorWhenMoving(SpawnBasisActor, true);
		NewItemActor->MeshComponent->SetPhysicsLinearVelocity(ForwardVector * 250.f + FVector(0.f, 0.f, 150.f));
		NewItemActor->MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector(0.f, 90.f, 0.f));
	}

	FItemInstance RemovedItem;
	RemoveItem(InInstanceId, RemovedItem);

	return true;
}

void UInventoryComponent::ClearAll()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	Entries.Reset();
	RebuildCache();

	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::RebuildCache()
{
	CellCache.Init(INDEX_NONE, Width * Height);

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FIntPoint Size;
		if (!GetEntrySize(Entries[Index].Item, Size))
		{
			continue;
		}

		MarkCells(Index, Entries[Index].Position, Size);
	}
}

void UInventoryComponent::MarkCells(int32 InEntryIndex, FIntPoint InPosition, FIntPoint InSize)
{
	for (int32 OffsetY = 0; OffsetY < InSize.Y; ++OffsetY)
	{
		for (int32 OffsetX = 0; OffsetX < InSize.X; ++OffsetX)
		{
			const FIntPoint Coord(InPosition.X + OffsetX, InPosition.Y + OffsetY);
			if (IsValidCoord(Coord))
			{
				CellCache[CoordToIndex(Coord)] = InEntryIndex;
			}
		}
	}
}

void UInventoryComponent::DebugPrintGrid() const
{
	UE_LOG(LogTemp, Log, TEXT("Inventory %dx%d, %d entries"), Width, Height, Entries.Num());

	for (int32 Y = 0; Y < Height; ++Y)
	{
		FString Row;
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 Occupant = CellCache[CoordToIndex(FIntPoint(X, Y))];
			Row += (Occupant == INDEX_NONE) ? TEXT(" . ") : FString::Printf(TEXT("%2d "), Occupant);
		}

		UE_LOG(LogTemp, Log, TEXT("%s"), *Row);
	}
}

void UInventoryComponent::DebugAddItem(FName InRowName, int32 InCount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (DebugItemTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugAddItem: DebugItemTable is not set"));
		return;
	}

	FItemInstance NewItem;
	NewItem.ItemRow.DataTable = DebugItemTable;
	NewItem.ItemRow.RowName = InRowName;
	NewItem.InstanceId = FGuid::NewGuid();
	NewItem.StackCount = FMath::Max(1, InCount);

	int32 Remainder = 0;
	const bool bAdded = TryAddItem(NewItem, Remainder);

	UE_LOG(LogTemp, Log, TEXT("DebugAddItem %s x%d -> %s, remainder %d"),
		*InRowName.ToString(), InCount, bAdded ? TEXT("OK") : TEXT("FAIL"), Remainder);

	DebugPrintGrid();
}

void UInventoryComponent::DebugMoveFirstItem(FIntPoint InNewPosition)
{
	if (Entries.Num() == 0)
	{
		return;
	}

	const bool bMoved = MoveItem(Entries[0].Item.InstanceId, InNewPosition, Entries[0].Item.bRotated);
	UE_LOG(LogTemp, Log, TEXT("DebugMove to (%d,%d) -> %s"),
		InNewPosition.X, InNewPosition.Y, bMoved ? TEXT("OK") : TEXT("FAIL"));

	DebugPrintGrid();
}

void UInventoryComponent::DebugRemoveFirstItem()
{
	if (Entries.Num() == 0)
	{
		return;
	}

	FItemInstance Removed;
	const bool bRemoved = RemoveItem(Entries[0].Item.InstanceId, Removed);
	UE_LOG(LogTemp, Log, TEXT("DebugRemove -> %s"), bRemoved ? TEXT("OK") : TEXT("FAIL"));

	DebugPrintGrid();
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInventoryComponent, Entries);
}

void UInventoryComponent::Server_MoveItem_Implementation(FGuid InInstanceId, FIntPoint InNewPosition, bool InRotated)
{
	MoveItem(InInstanceId, InNewPosition, InRotated);
}

void UInventoryComponent::Server_TransferItem_Implementation(UInventoryComponent* InTarget, FGuid InInstanceId, FIntPoint InTargetPosition, bool InRotated)
{
	if (InTarget == nullptr)
	{
		return;
	}

	InTarget->TransferItemFrom(this, InInstanceId, InTargetPosition, InRotated);
}

void UInventoryComponent::Server_TransferItemAuto_Implementation(UInventoryComponent* InTarget, FGuid InInstanceId)
{
	if (InTarget == nullptr)
	{
		return;
	}

	InTarget->TransferItemFromAuto(this, InInstanceId);
}

void UInventoryComponent::Server_EquipItem_Implementation(UEquipmentComponent* InEquipment, FGuid InInstanceId, EEquipSlotType InSlot)
{
	if (InEquipment == nullptr)
	{
		return;
	}

	InEquipment->EquipFromInventory(this, InInstanceId, InSlot);
}

void UInventoryComponent::Server_UnequipItem_Implementation(UEquipmentComponent* InEquipment, EEquipSlotType InSlot, FIntPoint InTargetPosition)
{
	if (InEquipment == nullptr)
	{
		return;
	}

	InEquipment->UnequipToInventoryAt(InSlot, this, InTargetPosition);
}

void UInventoryComponent::Server_TransferItemFrom_Implementation(UInventoryComponent* InSource, FGuid InInstanceId, FIntPoint InTargetPosition, bool InRotated)
{
	if (InSource == nullptr)
	{
		return;
	}

	TransferItemFrom(InSource, InInstanceId, InTargetPosition, InRotated);
}

void UInventoryComponent::Server_TransferItemFromAuto_Implementation(UInventoryComponent* InSource, FGuid InInstanceId)
{
	if (InSource == nullptr)
	{
		return;
	}
	TransferItemFromAuto(InSource, InInstanceId);
}

void UInventoryComponent::Server_UnequipItemTo_Implementation(UEquipmentComponent* InEquipment, EEquipSlotType InSlot, UInventoryComponent* InTarget, FIntPoint InTargetPosition)
{
	if (InEquipment == nullptr || InTarget == nullptr)
	{
		return;
	}
	InEquipment->UnequipToInventoryAt(InSlot, InTarget, InTargetPosition);
}

void UInventoryComponent::Server_UseItem_Implementation(FGuid InInstanceId)
{
	UseItem(InInstanceId);
}
void UInventoryComponent::Server_ToggleFavorite_Implementation(FGuid InInstanceId)
{
	ToggleFavorite(InInstanceId);
}

void UInventoryComponent::Server_SplitStack_Implementation(FGuid InInstanceId, int32 InQuantity, FIntPoint InTargetPosition)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	const int32 SourceIndex = FindEntryIndexById(InInstanceId);
	if (SourceIndex == INDEX_NONE)
	{
		return;
	}

	if (InQuantity <= 0 || InQuantity >= Entries[SourceIndex].Item.StackCount)
	{
		return;
	}

	FItemInstance NewItem = Entries[SourceIndex].Item;
	NewItem.InstanceId = FGuid::NewGuid();
	NewItem.StackCount = InQuantity;

	FIntPoint FinalPosition = InTargetPosition;

	// 유효하지 않은 좌표(예: 우클릭 메뉴에서 온 요청)면 빈 칸을 자동으로 찾음
	if (InTargetPosition.X < 0 || InTargetPosition.Y < 0)
	{
		FIntPoint Size;
		if (!GetEntrySize(NewItem, Size))
		{
			return;
		}

		if (!FindFirstFreeSlot(Size, FinalPosition, SourceIndex))
		{
			return;
		}
	}

	if (!TryAddItemAt(NewItem, FinalPosition))
	{
		return;
	}

	if (!RemoveStackCount(InInstanceId, InQuantity))
	{
		FItemInstance Removed;
		RemoveItem(NewItem.InstanceId, Removed);
	}
}
void UInventoryComponent::Server_MergeStack_Implementation(FGuid InSourceId, FGuid InTargetId)
{
	MergeStack(InSourceId, InTargetId);
}

void UInventoryComponent::Server_MergeStackFrom_Implementation(UInventoryComponent* InSource, FGuid InSourceId, FGuid InTargetId)
{
	if (InSource == nullptr)
	{
		return;
	}
	MergeStackFrom(InSource, InSourceId, InTargetId);
}
void UInventoryComponent::Server_DropItem_Implementation(FGuid InInstanceId)
{
	DropItem(InInstanceId);
}

void UInventoryComponent::SetGridSize(int32 InWidth, int32 InHeight)
{
	Width = FMath::Max(1, InWidth);
	Height = FMath::Max(1, InHeight);
}

void UInventoryComponent::SetEntries(const TArray<FInventoryEntry>& InEntries)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	Entries = InEntries;
	RebuildCache();
	OnInventoryChanged.Broadcast();
}