#include "PCGModule/HBDungeonRuleLayout.h"
#include "HAL/PlatformTime.h"

namespace
{
	bool Allows(const FHBModuleCatalogEntry& M, int32 Floor)
	{
		return M.AllowedFloor == EHBAllowedFloor::Any || static_cast<int32>(M.AllowedFloor) == Floor;
	}
	int32 FindMap(const TArray<FHBModuleCatalogEntry>& Catalog, const TSoftObjectPtr<UWorld>& Map)
	{
		return Catalog.IndexOfByPredicate([&](const auto& M) { return M.Map == Map; });
	}
	bool CheckSettings(const TArray<FHBModuleCatalogEntry>& Catalog, const HBConnectorLayout::FSettings& S,
		const FHBDungeonGenerationRules& R, FString& Error)
	{
		auto Fail = [&](const TCHAR* Why) { Error = Why; return false; };
		if (!HBConnectorLayout::ValidateCatalog(Catalog, Error)) return false;
		if (!S.Origin.IsValid() || !S.Origin.GetScale3D().Equals(FVector::OneVector, 0.0001)
			|| !S.Origin.GetRotation().GetUpVector().Equals(FVector::UpVector, 0.0001)
			|| S.MaxSearchSteps < 1 || !FMath::IsFinite(S.MaxSearchSeconds) || S.MaxSearchSeconds <= 0)
			return Fail(TEXT("Invalid search limits or generator transform."));
		if (R.Floor1MainRooms < 2 || R.Floor1MainRooms > 24 || R.Floor2MainRooms < 3 || R.Floor2MainRooms > 24
			|| R.Floor1BranchRooms < 0 || R.Floor1BranchRooms > 8 || R.Floor2BranchRooms <= R.Floor1BranchRooms
			|| R.Floor2BranchRooms > 8 || R.Floor1MainRooms + R.Floor2MainRooms + R.Floor1BranchRooms + R.Floor2BranchRooms + 1 > 64)
			return Fail(TEXT("Use 2-24 floor-1 main rooms, 3-24 floor-2 main rooms, 0-8 branches; floor 2 needs more branches, at most 64 modules including the corridor."));
		if (R.Floor2Offset.ContainsNaN()) return Fail(TEXT("Floor2Offset must be finite."));
		if (R.Floor1ExitMap.IsNull() || R.Floor2EntryMap.IsNull() || R.BossCorridorMap.IsNull())
			return Fail(TEXT("Dungeon Rules: select Floor1ExitMap, Floor2EntryMap and BossCorridorMap. No map names are assumed."));
		const int32 Exit = FindMap(Catalog, R.Floor1ExitMap), Entry = FindMap(Catalog, R.Floor2EntryMap), Corridor = FindMap(Catalog, R.BossCorridorMap);
		if (Exit == INDEX_NONE || Entry == INDEX_NONE || Corridor == INDEX_NONE)
			return Fail(TEXT("Selected transition/corridor maps are missing from the module catalog."));
		if (Catalog[Exit].Kind != EHBDungeonModuleKind::Room || Catalog[Entry].Kind != EHBDungeonModuleKind::Room
			|| Catalog[Exit].Role == EHBPrototypeRoomRole::Boss || Catalog[Entry].Role == EHBPrototypeRoomRole::Boss
			|| !Allows(Catalog[Exit], 1) || !Allows(Catalog[Entry], 2))
			return Fail(TEXT("Transition endpoints must be non-boss rooms allowed on their assigned floors."));
		if (Catalog[Corridor].Kind != EHBDungeonModuleKind::Corridor || Catalog[Corridor].Role != EHBPrototypeRoomRole::Unassigned
			|| Catalog[Corridor].Connectors.Num() < 2 || !Allows(Catalog[Corridor], 2))
			return Fail(TEXT("BossCorridorMap must be a Corridor module with two or more doors, allowed on floor 2."));
		if (R.Floor1ExitPortalId.IsNone() || R.Floor2EntryPortalId.IsNone()
			|| !Catalog[Exit].Portals.ContainsByPredicate([&](const auto& P) { return P.PortalId == R.Floor1ExitPortalId; })
			|| !Catalog[Entry].Portals.ContainsByPredicate([&](const auto& P) { return P.PortalId == R.Floor2EntryPortalId; }))
			return Fail(TEXT("Both selected transition rooms must contain a manually placed Portal Actor with the selected PortalId."));
		if (!S.bAllowModuleReuse && Exit == Entry) return Fail(TEXT("The same transition map on both floors requires Allow Module Reuse."));
		if (!S.bAllowModuleReuse && R.Floor1MainRooms + R.Floor2MainRooms + R.Floor1BranchRooms + R.Floor2BranchRooms
			> Catalog.FilterByPredicate([](const auto& M) { return M.Kind == EHBDungeonModuleKind::Room; }).Num())
			return Fail(TEXT("Not enough distinct room modules for the selected room counts while Allow Module Reuse is off."));
		if (!Catalog.ContainsByPredicate([](const auto& M) { return M.Kind == EHBDungeonModuleKind::Room && M.Role == EHBPrototypeRoomRole::Boss && Allows(M, 2); }))
			return Fail(TEXT("Register at least one Boss module allowed on floor 2."));
		return true;
	}

	// Physical validation establishes indices and the parent-before-child connected tree first.
	bool CheckRules(const TArray<FHBModuleCatalogEntry>& Catalog, const HBConnectorLayout::FSettings& S,
		const FHBDungeonGenerationRules& Rules, const TArray<FHBPrototypeRoom>& Rooms, FString& Error)
	{
		auto Fail = [&](const TCHAR* Why) { Error = Why; return false; };
		if (Rooms.IsEmpty()) return Fail(TEXT("No dungeon rooms."));
		if (!Rooms[0].Transform.Equals(S.Origin, HBConnectorLayout::SnapTolerance)) return Fail(TEXT("Start does not match generator origin."));
		int32 Counts[4] = {}, Heal[3] = {}, Upgrade[3] = {}, Main[3] = {}, Branch[3] = {};
		int32 Portals = 0, Exits = 0, Corridors = 0;
		TSet<FName> UsedModules;
		TArray<int32> Children, MainChildren;
		Children.Init(0, Rooms.Num()); MainChildren.Init(0, Rooms.Num());
		for (int32 I = 1; I < Rooms.Num(); ++I)
		{
			if (Rooms[I].ParentId < 0 || Rooms[I].ParentId >= I) return Fail(TEXT("Invalid progression parent."));
			++Children[Rooms[I].ParentId];
			if (!Rooms[I].bBranch) ++MainChildren[Rooms[I].ParentId];
		}
		for (int32 I = 0; I < Rooms.Num(); ++I)
		{
			const auto& R = Rooms[I];
			const auto* M = Catalog.FindByPredicate([&](const auto& C) { return C.ModuleId == R.ModuleId; });
			if (!M || R.Floor < 1 || R.Floor > 2 || !Allows(*M, R.Floor)) return Fail(TEXT("Module placed on a forbidden floor."));
			if (!S.bAllowModuleReuse && UsedModules.Contains(R.ModuleId)) return Fail(TEXT("Module reused while reuse is disabled."));
			UsedModules.Add(R.ModuleId);
			if (I == 0 && (R.Floor != 1 || R.Role != EHBPrototypeRoomRole::Adventure || R.bBranch || R.bFloorExit || R.bCorridor))
				return Fail(TEXT("Start must be an Adventure room on floor 1."));
			if (R.bBranch && (Children[I] || R.bFloorExit || R.bCorridor || R.Connection == EHBDungeonConnection::Portal))
				return Fail(TEXT("Optional rooms must be terminal branches, not transition or corridor rooms."));
			if (!R.bBranch && MainChildren[I] != (R.Role == EHBPrototypeRoomRole::Boss ? 0 : 1))
				return Fail(TEXT("Main progression must lead to the sole terminal boss."));
			if (R.bCorridor)
			{
				++Corridors;
				if (R.ModuleMap != Rules.BossCorridorMap || R.Floor != 2 || R.bBranch || R.bFloorExit || I == 0 || Children[I] != 1
					|| R.Connection != EHBDungeonConnection::Door || Rooms[R.ParentId].Role != EHBPrototypeRoomRole::Adventure
					|| Rooms[R.ParentId].Connection == EHBDungeonConnection::Portal || Rooms[R.ParentId].bFloorExit
					|| !Rooms.ContainsByPredicate([&](const auto& C) { return C.ParentId == I && C.Role == EHBPrototypeRoomRole::Boss; }))
					return Fail(TEXT("Required progression: Adventure -> selected boss corridor -> Boss, after the floor-2 entry room."));
			}
			else
			{
				const int32 Role = static_cast<int32>(R.Role);
				if (Role > 3) return Fail(TEXT("Invalid room role."));
				++Counts[Role];
				++(R.bBranch ? Branch[R.Floor] : Main[R.Floor]);
				if (R.Role == EHBPrototypeRoomRole::Recovery && ++Heal[R.Floor] > 1) return Fail(TEXT("At most one Recovery room per floor."));
				if (R.Role == EHBPrototypeRoomRole::Upgrade && ++Upgrade[R.Floor] > 1) return Fail(TEXT("At most one Upgrade room per floor."));
			}
			if (R.bFloorExit)
			{
				++Exits;
				if (R.Floor != 1 || R.bBranch || R.ModuleMap != Rules.Floor1ExitMap) return Fail(TEXT("Invalid selected floor-1 exit room."));
			}
			if (I > 0)
			{
				const auto& P = Rooms[R.ParentId];
				if (P.Role == EHBPrototypeRoomRole::Boss) return Fail(TEXT("No rooms may follow the boss."));
				if (R.Connection == EHBDungeonConnection::Portal)
				{
					++Portals;
					if (P.Floor != 1 || R.Floor != 2 || !P.bFloorExit || R.bBranch || R.ModuleMap != Rules.Floor2EntryMap
						|| R.ParentPortalId != Rules.Floor1ExitPortalId || R.EntryPortalId != Rules.Floor2EntryPortalId
						|| !R.Transform.Equals(FTransform(Rules.Floor2Offset) * S.Origin, HBConnectorLayout::SnapTolerance))
						return Fail(TEXT("Invalid configured portal transition."));
				}
				else if (P.Floor != R.Floor) return Fail(TEXT("Logical floors may only change through the configured portal pair."));
				if ((R.Role == EHBPrototypeRoomRole::Recovery || R.Role == EHBPrototypeRoomRole::Upgrade)
					&& (R.ParentId == 0 || R.Role == P.Role)) return Fail(TEXT("Special room adjacent to start or to the same special role."));
				if (R.Role == EHBPrototypeRoomRole::Boss && (R.Floor != 2 || R.bBranch || !P.bCorridor || Children[I] != 0))
					return Fail(TEXT("Exactly one floor-2 boss must directly follow the selected corridor and end the main path."));
			}
			if ((R.ModuleMap == Rules.Floor1ExitMap || R.ModuleMap == Rules.Floor2EntryMap)
				&& !R.bFloorExit && R.Connection != EHBDungeonConnection::Portal)
				return Fail(TEXT("Transition modules are reserved for the configured endpoints."));
			int32 Streak = 0;
			for (int32 J = I; J != INDEX_NONE; J = Rooms[J].ParentId)
			{
				if (Rooms[J].Floor != R.Floor) break;
				if (Rooms[J].bCorridor) continue;
				if (Rooms[J].Role != EHBPrototypeRoomRole::Adventure) break;
				if (++Streak > 3) return Fail(TEXT("More than three consecutive Adventure rooms on the same floor."));
			}
		}
		if (Counts[3] != 1 || Portals != 1 || Exits != 1 || Corridors != 1) return Fail(TEXT("Expected one boss, one boss corridor and one portal transition."));
		if (Counts[0] <= FMath::Max3(Counts[1], Counts[2], Counts[3])) return Fail(TEXT("Adventure must be the most common role."));
		if (Main[1] != Rules.Floor1MainRooms || Main[2] != Rules.Floor2MainRooms
			|| Branch[1] != Rules.Floor1BranchRooms || Branch[2] != Rules.Floor2BranchRooms)
			return Fail(TEXT("Generated per-floor room counts do not match Dungeon Rules."));
		return true;
	}

	struct FSlot
	{
		int32 Floor = 1, Parent = INDEX_NONE, Forced = INDEX_NONE;
		bool bExit = false, bPortal = false, bCorridor = false, bBoss = false, bBranch = false;
	};
	struct FSearch
	{
		const TArray<FHBModuleCatalogEntry>& Catalog;
		const HBConnectorLayout::FSettings& Settings;
		const FHBDungeonGenerationRules& Rules;
		FRandomStream Random;
		TArray<FSlot> Slots;
		TArray<FHBPrototypeRoom> Rooms;
		TArray<int32> Chosen;
		TArray<TArray<bool>> Used;
		int32 RoleCounts[3][4] = {};
		int32 Steps = 0, Best = 0;
		double Start = FPlatformTime::Seconds();
		bool bLimited = false;
		FSearch(const TArray<FHBModuleCatalogEntry>& C, const HBConnectorLayout::FSettings& S, const FHBDungeonGenerationRules& R)
			: Catalog(C), Settings(S), Rules(R), Random(S.Seed)
		{
			for (int32 I = 0; I < R.Floor1MainRooms; ++I)
			{
				FSlot Slot; Slot.Parent = I - 1;
				if (I == R.Floor1MainRooms - 1) { Slot.bExit = true; Slot.Forced = FindMap(C, R.Floor1ExitMap); }
				Slots.Add(Slot);
			}
			for (int32 I = 0; I < R.Floor2MainRooms - 1; ++I)
			{
				FSlot Slot; Slot.Floor = 2; Slot.Parent = Slots.Num() - 1;
				if (I == 0) { Slot.bPortal = true; Slot.Forced = FindMap(C, R.Floor2EntryMap); }
				Slots.Add(Slot);
			}
			FSlot Corridor; Corridor.Floor = 2; Corridor.Parent = Slots.Num() - 1;
			Corridor.bCorridor = true; Corridor.Forced = FindMap(C, R.BossCorridorMap); Slots.Add(Corridor);
			FSlot Boss; Boss.Floor = 2; Boss.Parent = Slots.Num() - 1; Boss.bBoss = true; Slots.Add(Boss);
			for (int32 Floor = 1; Floor <= 2; ++Floor)
				for (int32 I = 0; I < (Floor == 1 ? R.Floor1BranchRooms : R.Floor2BranchRooms); ++I)
				{
					FSlot Slot; Slot.Floor = Floor; Slot.bBranch = true; Slots.Add(Slot);
				}
		}
		template<class T> void Shuffle(TArray<T>& A)
		{
			for (int32 I = A.Num() - 1; I > 0; --I) A.Swap(I, Random.RandRange(0, I));
		}
		bool Budget()
		{
			bLimited |= Steps >= Settings.MaxSearchSteps || FPlatformTime::Seconds() - Start >= Settings.MaxSearchSeconds;
			return !bLimited;
		}
		bool CanUse(int32 Module, const FSlot& Slot, int32 Parent) const
		{
			const auto& M = Catalog[Module];
			if (!Allows(M, Slot.Floor) || (!Settings.bAllowModuleReuse && Chosen.Contains(Module))) return false;
			if (Slot.Forced != INDEX_NONE && Module != Slot.Forced) return false;
			if (Slot.bCorridor) return M.Kind == EHBDungeonModuleKind::Corridor;
			if (M.Kind != EHBDungeonModuleKind::Room || (M.Role == EHBPrototypeRoomRole::Boss) != Slot.bBoss) return false;
			if (Slot.Forced == INDEX_NONE && (M.Map == Rules.Floor1ExitMap || M.Map == Rules.Floor2EntryMap)) return false;
			const bool bBeforeCorridor = Rooms.Num() + 1 < Slots.Num() && Slots[Rooms.Num() + 1].bCorridor;
			if ((Parent == INDEX_NONE || bBeforeCorridor) && M.Role != EHBPrototypeRoomRole::Adventure) return false;
			if (M.Role == EHBPrototypeRoomRole::Recovery || M.Role == EHBPrototypeRoomRole::Upgrade)
			{
				if (RoleCounts[Slot.Floor][static_cast<int32>(M.Role)] >= 1
					|| Parent == 0 || (Parent != INDEX_NONE && Rooms[Parent].Role == M.Role)) return false;
			}
			if (M.Role == EHBPrototypeRoomRole::Adventure)
			{
				int32 Streak = 1;
				for (int32 J = Parent; J != INDEX_NONE; J = Rooms[J].ParentId)
				{
					if (Rooms[J].Floor != Slot.Floor) break;
					if (Rooms[J].bCorridor) continue;
					if (Rooms[J].Role != M.Role) break;
					if (++Streak > 3) return false;
				}
			}
			return true;
		}
		bool Place(int32 Module, const FSlot& Slot, int32 Parent, int32 Port, int32 Entry, const FTransform& Transform)
		{
			if (!Budget()) return false;
			++Steps;
			const auto& M = Catalog[Module];
			for (int32 I = 0; I < Rooms.Num(); ++I)
				if (HBConnectorLayout::OccupancyOverlaps(M, Transform, Catalog[Chosen[I]], Rooms[I].Transform)) return false;
			FHBPrototypeRoom R;
			R.Id = Rooms.Num(); R.ParentId = Parent; R.Floor = Slot.Floor; R.Role = M.Role;
			R.bBranch = Slot.bBranch; R.bFloorExit = Slot.bExit; R.bCorridor = Slot.bCorridor;
			R.ModuleId = M.ModuleId; R.ModuleMap = M.Map; R.Transform = Transform;
			R.LocalBounds = M.Bounds; R.Bounds = M.Bounds.TransformBy(Transform); R.InspectionEntry = M.InspectionEntry;
			if (Slot.bPortal)
			{
				R.Connection = EHBDungeonConnection::Portal;
				R.ParentPortalId = Rules.Floor1ExitPortalId; R.EntryPortalId = Rules.Floor2EntryPortalId;
			}
			else if (Parent != INDEX_NONE)
			{
				R.ParentConnectorId = Catalog[Chosen[Parent]].Connectors[Port].ConnectorId;
				R.EntryConnectorId = M.Connectors[Entry].ConnectorId;
				Used[Parent][Port] = true;
			}
			Rooms.Add(R); Chosen.Add(Module); Used.AddDefaulted_GetRef().Init(false, M.Connectors.Num());
			if (Entry != INDEX_NONE) Used.Last()[Entry] = true;
			if (!Slot.bCorridor) ++RoleCounts[Slot.Floor][static_cast<int32>(M.Role)];
			Best = FMath::Max(Best, Rooms.Num());
			if (Extend()) return true;
			if (!Slot.bCorridor) --RoleCounts[Slot.Floor][static_cast<int32>(M.Role)];
			Rooms.Pop(); Chosen.Pop(); Used.Pop();
			if (Port != INDEX_NONE) Used[Parent][Port] = false;
			return false;
		}
		bool Extend()
		{
			if (Rooms.Num() == Slots.Num())
			{
				FString Error;
				return CheckRules(Catalog, Settings, Rules, Rooms, Error);
			}
			if (!Budget()) return false;
			const FSlot Slot = Slots[Rooms.Num()];
			TArray<int32> Parents;
			if (Slot.bBranch)
			{
				for (int32 I = 0; I < Rooms.Num(); ++I)
					if (Rooms[I].Floor == Slot.Floor && !Rooms[I].bBranch && !Rooms[I].bCorridor && Rooms[I].Role != EHBPrototypeRoomRole::Boss) Parents.Add(I);
				Shuffle(Parents);
			}
			else Parents.Add(Slot.Parent);
			for (int32 Parent : Parents)
			{
				TArray<int32> Candidates;
				for (int32 M = 0; M < Catalog.Num(); ++M) if (CanUse(M, Slot, Parent)) Candidates.Add(M);
				Shuffle(Candidates);
				// Upgrade is preferred on optional branches; retain random order within each priority.
				Candidates.StableSort([&](int32 A, int32 B)
				{
					const bool UA = Catalog[A].Role == EHBPrototypeRoomRole::Upgrade, UB = Catalog[B].Role == EHBPrototypeRoomRole::Upgrade;
					return Slot.bBranch ? UA > UB : UA < UB;
				});
				for (int32 Module : Candidates)
				{
					if (Parent == INDEX_NONE || Slot.bPortal)
					{
						const FTransform T = Slot.bPortal ? FTransform(Rules.Floor2Offset) * Settings.Origin : Settings.Origin;
						if (Place(Module, Slot, Parent, INDEX_NONE, INDEX_NONE, T)) return true;
					}
					else
					{
						TArray<int32> Ports, Entries;
						for (int32 I = 0; I < Used[Parent].Num(); ++I) if (!Used[Parent][I]) Ports.Add(I);
						for (int32 I = 0; I < Catalog[Module].Connectors.Num(); ++I) Entries.Add(I);
						Shuffle(Ports); Shuffle(Entries);
						for (int32 Port : Ports)
							for (int32 Entry : Entries)
							{
								const FTransform PW = Catalog[Chosen[Parent]].Connectors[Port].LocalTransform * Rooms[Parent].Transform;
								if (Place(Module, Slot, Parent, Port, Entry, HBConnectorLayout::Align(PW, Catalog[Module].Connectors[Entry].LocalTransform))) return true;
								if (bLimited) return false;
							}
					}
					if (bLimited) return false;
				}
			}
			return false;
		}
	};
}

bool HBDungeonRuleLayout::ResolvePortalIds(const TArray<FHBModuleCatalogEntry>& Catalog,
    FHBDungeonGenerationRules& Rules, FString& OutError)
{
    auto Resolve = [&](const TSoftObjectPtr<UWorld>& Map, FName& Id)
    {
        const auto* Entry = Catalog.FindByPredicate([&](const auto& E) { return E.Map == Map; });
        if (!Entry) { OutError = TEXT("Transition map must be selected from registered modules: ") + Map.ToString(); return false; }
        if (Id.IsNone() && Entry->Portals.Num() == 1) Id = Entry->Portals[0].PortalId;
        if (!Id.IsNone() && Entry->Portals.ContainsByPredicate([&](const auto& P) { return P.PortalId == Id; })) return true;
        OutError = FString::Printf(TEXT("%s: %s"), *Map.ToString(), Id.IsNone()
            ? (Entry->Portals.IsEmpty() ? TEXT("Place a dungeon portal actor in this map.") : TEXT("Select a portal from the dropdown."))
            : TEXT("Selected portal no longer exists. Select it again; it will not be replaced automatically."));
        return false;
    };
    // Commit only after both endpoints resolve successfully.
    FHBDungeonGenerationRules Pending = Rules;
    if (!Resolve(Pending.Floor1ExitMap, Pending.Floor1ExitPortalId)
        || !Resolve(Pending.Floor2EntryMap, Pending.Floor2EntryPortalId)) return false;
    Rules = Pending;
    OutError.Reset();
    return true;
}

bool HBDungeonRuleLayout::Generate(const TArray<FHBModuleCatalogEntry>& Catalog, const HBConnectorLayout::FSettings& Settings,
	const FHBDungeonGenerationRules& InputRules, TArray<FHBPrototypeRoom>& OutRooms, FString& OutError)
{
	OutRooms.Reset(); OutError.Reset();
	FHBDungeonGenerationRules Rules = InputRules;
	if (!ResolvePortalIds(Catalog, Rules, OutError)) return false;
	if (!CheckSettings(Catalog, Settings, Rules, OutError)) return false;
	FSearch Search(Catalog, Settings, Rules);
	if (!Search.Extend())
	{
		OutError = FString::Printf(TEXT("%s: placed at most %d/%d modules in %d attempts. Check role/floor availability, portal selections, door capacity, occupancy and Floor2Offset."),
			Search.bLimited ? TEXT("Search limit reached") : TEXT("No rule-compliant layout"), Search.Best, Search.Slots.Num(), Search.Steps);
		return false;
	}
	for (int32 I = 0; I < Search.Rooms.Num(); ++I)
		for (int32 C = 0; C < Search.Used[I].Num(); ++C)
			(Search.Used[I][C] ? Search.Rooms[I].UsedConnectorIds : Search.Rooms[I].UnusedConnectorIds).Add(Catalog[Search.Chosen[I]].Connectors[C].ConnectorId);
	if (!Validate(Catalog, Settings, Rules, Search.Rooms, OutError)) return false;
	OutRooms = MoveTemp(Search.Rooms);
	return true;
}

bool HBDungeonRuleLayout::Validate(const TArray<FHBModuleCatalogEntry>& Catalog, const HBConnectorLayout::FSettings& Settings,
	const FHBDungeonGenerationRules& InputRules, const TArray<FHBPrototypeRoom>& Rooms, FString& OutError)
{
	FHBDungeonGenerationRules Rules = InputRules;
	if (!ResolvePortalIds(Catalog, Rules, OutError)) return false;
	return CheckSettings(Catalog, Settings, Rules, OutError)
		&& HBConnectorLayout::Validate(Catalog, Rooms, OutError) && CheckRules(Catalog, Settings, Rules, Rooms, OutError);
}
