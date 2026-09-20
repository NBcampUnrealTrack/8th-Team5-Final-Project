#include "PCGPrototype/HBDungeonLayout.h"

namespace
{
	const FIntPoint Directions[] = { FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1) };
	bool FreeNeighbour(FRandomStream& Random, const FIntPoint& From, const TSet<FIntPoint>& Used, FIntPoint& Out)
	{
		TArray<FIntPoint> Candidates;
		for (const FIntPoint& D : Directions)
		{
			const FIntPoint P = From + D;
			if (P.X >= 0 && P.X < 4 && P.Y >= 0 && P.Y < 4 && !Used.Contains(P)) Candidates.Add(P);
		}
		if (Candidates.IsEmpty()) return false;
		Out = Candidates[Random.RandRange(0, Candidates.Num()-1)];
		return true;
	}
}

const TCHAR* HBDungeonLayout::RoleName(EHBPrototypeRoomRole Role)
{
	switch (Role)
	{
	case EHBPrototypeRoomRole::Recovery: return TEXT("RECOVERY");
	case EHBPrototypeRoomRole::Upgrade: return TEXT("UPGRADE");
	case EHBPrototypeRoomRole::Boss: return TEXT("BOSS");
	default: return TEXT("ADVENTURE");
	}
}

FColor HBDungeonLayout::RoleColor(EHBPrototypeRoomRole Role)
{
	switch (Role)
	{
	case EHBPrototypeRoomRole::Recovery: return FColor(70, 255, 140);
	case EHBPrototypeRoomRole::Upgrade: return FColor(255, 200, 50);
	case EHBPrototypeRoomRole::Boss: return FColor(255, 75, 90);
	default: return FColor(65, 175, 255);
	}
}

bool HBDungeonLayout::Generate(const FSettings& S, TArray<FHBPrototypeRoom>& Out, FString& Error)
{
	Out.Reset();
	if (!FMath::IsFinite(S.CellSize) || S.CellSize <= 0 || !FMath::IsFinite(S.FloorHeight) || S.FloorHeight < 0 ||
		!S.AdventureBounds.IsValid || !S.BossBounds.IsValid || S.MaxAttempts < 1)
	{
		Error = TEXT("Invalid spacing, module bounds, or attempt limit."); return false;
	}
	FRandomStream Random(S.Seed);
	for (int32 Attempt = 0; Attempt < S.MaxAttempts; ++Attempt)
	{
		TArray<FHBPrototypeRoom> Rooms;
		int32 FloorExit = INDEX_NONE;
		bool bPlacementFailed = false;
		for (int32 Floor = 1; Floor <= 2 && !bPlacementFailed; ++Floor)
		{
			TSet<FIntPoint> Used;
			TMap<int32, FIntPoint> Cells;
			TArray<int32> Main;
			auto Add = [&](FIntPoint Cell, EHBPrototypeRoomRole Role, int32 Parent, bool bBranch)
			{
				FHBPrototypeRoom& R = Rooms.AddDefaulted_GetRef();
				R.Id = Rooms.Num()-1; R.ParentId = Parent; R.Role = Role; R.Floor = Floor; R.bBranch = bBranch;
				// Separate floor panels for inspection; the inter-floor edge is an annotation only.
				const FVector Position((Cell.X + (Floor-1)*6)*S.CellSize, Cell.Y*S.CellSize, (Floor-1)*S.FloorHeight);
				R.Transform = FTransform(FRotator(0, Random.RandRange(0,3)*90.0, 0), Position);
				R.Bounds = (Role == EHBPrototypeRoomRole::Boss ? S.BossBounds : S.AdventureBounds).TransformBy(R.Transform);
				Used.Add(Cell); Cells.Add(R.Id, Cell);
				return R.Id;
			};
			const int32 MainLength = Random.RandRange(4,5);
			FIntPoint Cell(Random.RandRange(0,3), Random.RandRange(0,3));
			for (int32 Index = 0; Index < MainLength; ++Index)
			{
				if (Index && !FreeNeighbour(Random, Cell, Used, Cell)) { bPlacementFailed = true; break; }
				EHBPrototypeRoomRole Role = EHBPrototypeRoomRole::Adventure;
				if (Index == (Floor == 1 ? 2 : 1)) Role = EHBPrototypeRoomRole::Recovery;
				if (Floor == 2 && Index == MainLength-1) Role = EHBPrototypeRoomRole::Boss;
				Main.Add(Add(Cell, Role, Index ? Main.Last() : FloorExit, false));
			}
			if (bPlacementFailed) break;
			if (Floor == 1) { FloorExit = Main.Last(); Rooms[FloorExit].bFloorExit = true; }
			// An upgrade is always optional and never adjacent to the starting room.
			const int32 BranchParent = Main[1];
			FIntPoint BranchCell;
			if (!FreeNeighbour(Random, Cells[BranchParent], Used, BranchCell)) { bPlacementFailed = true; break; }
			const int32 Upgrade = Add(BranchCell, EHBPrototypeRoomRole::Upgrade, BranchParent, true);
			if (Floor == 2)
			{
				// Floor 2 has a second optional branch off its recovery room.
				if (!FreeNeighbour(Random, Cells[BranchParent], Used, BranchCell)) { bPlacementFailed = true; break; }
				Add(BranchCell, EHBPrototypeRoomRole::Adventure, BranchParent, true);
			}
			(void)Upgrade;
		}
		if (!bPlacementFailed && Validate(Rooms, Error)) { Out = MoveTemp(Rooms); Error.Reset(); return true; }
	}
	Error = FString::Printf(TEXT("No valid layout after %d attempts. %s"), S.MaxAttempts, *Error);
	return false;
}

bool HBDungeonLayout::Validate(const TArray<FHBPrototypeRoom>& R, FString& Error)
{
	auto Fail = [&](const TCHAR* Reason) { Error = Reason; return false; };
	if (R.IsEmpty() || R[0].Role != EHBPrototypeRoomRole::Adventure || R[0].Floor != 1 || R[0].ParentId != INDEX_NONE)
		return Fail(TEXT("Missing adventure start on floor 1."));
	int32 Heal[2] = {}, Upgrade[2] = {}, Branch[2] = {}, Count[4] = {}, Transitions = 0, Exits = 0;
	for (int32 I = 0; I < R.Num(); ++I)
	{
		const auto& Room = R[I];
		if (Room.Id != I || Room.Floor < 1 || Room.Floor > 2 || !Room.Bounds.IsValid) return Fail(TEXT("Invalid room metadata."));
		if (static_cast<uint8>(Room.Role) > 3) return Fail(TEXT("Invalid role."));
		++Count[static_cast<uint8>(Room.Role)];
		if (Room.Role == EHBPrototypeRoomRole::Recovery && ++Heal[Room.Floor-1] > 1) return Fail(TEXT("Too many recovery rooms."));
		if (Room.Role == EHBPrototypeRoomRole::Upgrade && ++Upgrade[Room.Floor-1] > 1) return Fail(TEXT("Too many upgrade rooms."));
		if (Room.bBranch) ++Branch[Room.Floor-1];
		if (Room.bFloorExit) { ++Exits; if (Room.Floor != 1 || Room.bBranch) return Fail(TEXT("Invalid floor exit.")); }
		if (I > 0)
		{
			// Parent-before-child order proves reachability and excludes cycles/orphans.
			if (Room.ParentId < 0 || Room.ParentId >= I) return Fail(TEXT("Disconnected or cyclic room."));
			const auto& P = R[Room.ParentId];
			if (P.Role == EHBPrototypeRoomRole::Boss) return Fail(TEXT("A room follows the boss."));
			if (Room.Floor != P.Floor)
			{
				if (P.Floor != 1 || Room.Floor != 2 || !P.bFloorExit || Room.bBranch) return Fail(TEXT("Invalid floor transition."));
				++Transitions;
			}
			if ((Room.Role == EHBPrototypeRoomRole::Recovery || Room.Role == EHBPrototypeRoomRole::Upgrade) &&
				(Room.ParentId == 0 || Room.Role == P.Role)) return Fail(TEXT("Invalid special-room adjacency."));
			if (Room.Role == EHBPrototypeRoomRole::Boss && (Room.Floor != 2 || Room.bBranch || P.bFloorExit || P.Role != EHBPrototypeRoomRole::Adventure))
				return Fail(TEXT("Invalid boss predecessor."));
		}
		int32 Consecutive = 0;
		for (int32 J = I; J != INDEX_NONE && R[J].Role == EHBPrototypeRoomRole::Adventure; J = R[J].ParentId)
			if (++Consecutive > 3) return Fail(TEXT("More than three consecutive adventure rooms."));
		for (int32 J = 0; J < I; ++J)
			if (Room.Bounds.Intersect(R[J].Bounds)) return Fail(TEXT("Module bounds overlap."));
	}
	if (Count[3] != 1 || Transitions != 1 || Exits != 1) return Fail(TEXT("Expected one boss and one floor transition."));
	if (Count[0] <= FMath::Max3(Count[1], Count[2], Count[3])) return Fail(TEXT("Adventure must be the most common role."));
	if (Branch[1] <= Branch[0]) return Fail(TEXT("Floor 2 requires more optional rooms."));
	Error.Reset(); return true;
}

FString HBDungeonLayout::Signature(const TArray<FHBPrototypeRoom>& Rooms)
{
	FString Result;
	for (const auto& R : Rooms)
		Result += FString::Printf(TEXT("%d:%d:%d:%d:%s:%g;"), R.Id, R.ParentId, R.Floor, static_cast<int32>(R.Role),
			*R.Transform.GetLocation().ToString(), R.Transform.Rotator().Yaw);
	return Result;
}
