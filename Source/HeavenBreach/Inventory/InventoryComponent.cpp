#include "InventoryComponent.h"
#include "Engine/DataTable.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeGrid(Width, Height);
}

void UInventoryComponent::InitializeGrid(int32 InWidth, int32 InHeight)
{
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
	if (!IsValidCoord(InCoord))
	{
		return INDEX_NONE;
	}

	return CellCache[CoordToIndex(InCoord)];
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

bool UInventoryComponent::MoveItem(FGuid InInstanceId, FIntPoint InNewPosition)
{
	const int32 EntryIndex = FindEntryIndexById(InInstanceId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	FIntPoint Size;
	if (!GetEntrySize(Entries[EntryIndex].Item, Size))
	{
		return false;
	}

	if (!CanPlaceAt(Size, InNewPosition, EntryIndex))
	{
		return false;
	}

	Entries[EntryIndex].Position = InNewPosition;
	RebuildCache();

	OnInventoryChanged.Broadcast();

	return true;
}

bool UInventoryComponent::RemoveItem(FGuid InInstanceId, FItemInstance& OutRemoved)
{
	const int32 EntryIndex = FindEntryIndexById(InInstanceId);
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

void UInventoryComponent::ClearAll()
{
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
	static const TCHAR* TablePath = TEXT("/Script/Engine.DataTable'/Game/Inventory/Data/DT_MockItem.DT_MockItem'");
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TablePath);
	if (Table == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugAddItem: table not found"));
		return;
	}

	FItemInstance NewItem;
	NewItem.ItemRow.DataTable = Table;
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

	const bool bMoved = MoveItem(Entries[0].Item.InstanceId, InNewPosition);
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
