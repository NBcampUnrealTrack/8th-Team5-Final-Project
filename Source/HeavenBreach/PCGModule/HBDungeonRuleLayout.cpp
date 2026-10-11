#include "PCGModule/HBDungeonRuleLayout.h"
#include "HAL/PlatformTime.h"
DEFINE_LOG_CATEGORY_STATIC(LogHBDungeonLayout, Log, All);

namespace
{
	bool MatchesProgression(EHBDungeonConnectorRole Parent, EHBDungeonConnectorRole Entry, bool bCorridor, bool bBoss)
	{
		using R = EHBDungeonConnectorRole;
		if (bCorridor) return Parent == R::None && Entry == R::DungeonSide;
		if (bBoss) return Parent == R::BossSide && Entry == R::BossEntrance;
		return HBConnectorRolesMatch(Parent, Entry);
	}
	bool Allows(const FHBModuleCatalogEntry& M, int32 Floor)
	{
		return M.AllowedFloor == EHBAllowedFloor::Any || static_cast<int32>(M.AllowedFloor) == Floor;
	}
	bool IsFloorTransition(EHBDungeonConnection Connection)
	{
		return Connection == EHBDungeonConnection::Portal || Connection == EHBDungeonConnection::FloorTransition;
	}
	int32 ConsecutiveLimit(EHBPrototypeRoomRole Role, const FHBDungeonGenerationRules& Rules)
	{
		switch (Role)
		{
		case EHBPrototypeRoomRole::Adventure: return Rules.MaxConsecutiveAdventureRooms;
		case EHBPrototypeRoomRole::Recovery: return Rules.MaxConsecutiveRecoveryRooms;
		case EHBPrototypeRoomRole::Upgrade: return Rules.MaxConsecutiveEnhancementRooms;
		default: return 0;
		}
	}
	int32 SpecialRoomLimit(EHBPrototypeRoomRole Role, const FHBDungeonGenerationRules& Rules)
	{
		return Role == EHBPrototypeRoomRole::Recovery ? Rules.MaxRecoveryRoomsPerFloor : Rules.MaxEnhancementRoomsPerFloor;
	}
	int32 ConsecutiveRoomCount(const TArray<FHBPrototypeRoom>& Rooms, int32 Parent, int32 Floor, EHBPrototypeRoomRole Role)
	{
		int32 Streak = 1;
		for (int32 J = Parent; J != INDEX_NONE; J = Rooms[J].ParentId)
		{
			if (Rooms[J].Floor != Floor) break;
			if (Rooms[J].bCorridor) continue;
			if (Rooms[J].Role != Role) break;
			++Streak;
		}
		return Streak;
	}
	const TCHAR* RoomRoleName(EHBPrototypeRoomRole Role)
	{
		switch (Role)
		{
		case EHBPrototypeRoomRole::Adventure: return TEXT("Adventure");
		case EHBPrototypeRoomRole::Recovery: return TEXT("Recovery");
		case EHBPrototypeRoomRole::Upgrade: return TEXT("Upgrade");
		case EHBPrototypeRoomRole::Boss: return TEXT("Boss");
		default: return TEXT("Unassigned");
		}
	}
	const TCHAR* ConnectorRoleName(EHBDungeonConnectorRole Role)
	{
		switch (Role)
		{
		case EHBDungeonConnectorRole::None: return TEXT("None");
		case EHBDungeonConnectorRole::BossEntrance: return TEXT("BossEntrance");
		case EHBDungeonConnectorRole::BossSide: return TEXT("BossSide");
		case EHBDungeonConnectorRole::DungeonSide: return TEXT("DungeonSide");
		default: return TEXT("Unknown");
		}
	}
	void LogFailureSettings(const HBConnectorLayout::FSettings& S, const FHBDungeonGenerationRules& R,
		const TCHAR* Phase, const FString& Error)
	{
		UE_LOG(LogHBDungeonLayout, Warning, TEXT("[Failure] Phase=%s Seed=%d Reason=%s"), Phase, S.Seed, *Error);
		UE_LOG(LogHBDungeonLayout, Warning, TEXT("[Settings] MainRooms=%d/%d BranchRooms=%d/%d Reuse=%s MaxAttempts=%d MaxSeconds=%.6f Origin=%s Floor2Offset=%s"),
			R.Floor1MainRooms, R.Floor2MainRooms, R.Floor1BranchRooms, R.Floor2BranchRooms,
			S.bAllowModuleReuse ? TEXT("true") : TEXT("false"), S.MaxSearchSteps, S.MaxSearchSeconds,
			*S.Origin.ToString(), *R.Floor2Offset.ToString());
		UE_LOG(LogHBDungeonLayout, Warning, TEXT("[RoleLimits] ConsecutiveAdventure=%d ConsecutiveRecovery=%d ConsecutiveEnhancement=%d RecoveryPerFloor=%d EnhancementPerFloor=%d"),
			R.MaxConsecutiveAdventureRooms, R.MaxConsecutiveRecoveryRooms, R.MaxConsecutiveEnhancementRooms,
			R.MaxRecoveryRoomsPerFloor, R.MaxEnhancementRoomsPerFloor);
		UE_LOG(LogHBDungeonLayout, Warning, TEXT("[PortalPlacement] Mode=%d PairCount=%d"),
			static_cast<int32>(R.PortalPlacementMode), R.PortalPairCount);
		UE_LOG(LogHBDungeonLayout, Warning, TEXT("[Maps] Floor1Exit=%s Portal=%s Floor2Entry=%s Portal=%s BossCorridor=%s"),
			*R.Floor1ExitMap.ToString(), *R.Floor1ExitPortalId.ToString(), *R.Floor2EntryMap.ToString(),
			*R.Floor2EntryPortalId.ToString(), *R.BossCorridorMap.ToString());
	}
	enum class ERejection : uint8
	{
		Floor, Reuse, ForcedModule, Kind, BossRole, ReservedTransition, StartCandidate, BeforeCorridor,
		SpecialQuota, AdjacentToStart, AdventureStreak, RecoveryStreak, EnhancementStreak,
		NoParents, NoCandidates, NoParentDoors, ConnectorRole, Occupancy, PortalQuota, FinalRules, Count
	};
	constexpr int32 FilterReasonCount = static_cast<int32>(ERejection::NoParents);
	constexpr int32 RejectionCount = static_cast<int32>(ERejection::Count);
	const TCHAR* RejectionName(ERejection Reason)
	{
		switch (Reason)
		{
		case ERejection::Floor: return TEXT("FloorNotAllowed");
		case ERejection::Reuse: return TEXT("ModuleReuseDisabled");
		case ERejection::ForcedModule: return TEXT("ForcedModuleMismatch");
		case ERejection::Kind: return TEXT("ModuleKindMismatch");
		case ERejection::BossRole: return TEXT("BossRoleMismatch");
		case ERejection::ReservedTransition: return TEXT("ReservedTransitionModule");
		case ERejection::StartCandidate: return TEXT("NotStartCandidate");
		case ERejection::BeforeCorridor: return TEXT("AdventureRequiredBeforeCorridor");
		case ERejection::SpecialQuota: return TEXT("SpecialRoomQuota");
		case ERejection::AdjacentToStart: return TEXT("SpecialRoomAdjacentToStart");
		case ERejection::AdventureStreak: return TEXT("AdventureStreakLimit");
		case ERejection::RecoveryStreak: return TEXT("RecoveryStreakLimit");
		case ERejection::EnhancementStreak: return TEXT("EnhancementStreakLimit");
		case ERejection::NoParents: return TEXT("NoEligibleParentRooms");
		case ERejection::NoCandidates: return TEXT("NoEligibleModules");
		case ERejection::NoParentDoors: return TEXT("NoAvailableParentDoors");
		case ERejection::ConnectorRole: return TEXT("ConnectorRoleMismatch");
		case ERejection::Occupancy: return TEXT("OccupancyOverlap");
		case ERejection::PortalQuota: return TEXT("PortalPointQuota");
		case ERejection::FinalRules: return TEXT("FinalRuleValidation");
		default: return TEXT("Unknown");
		}
	}
	struct FRejectionStats
	{
		int64 Count = 0;
		FString Example; // One bounded representative per reason, formatted only on first occurrence.
	};
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
		if (R.MaxConsecutiveAdventureRooms < 1 || R.MaxConsecutiveRecoveryRooms < 1 || R.MaxConsecutiveEnhancementRooms < 1)
			return Fail(TEXT("MaxConsecutiveAdventureRooms, MaxConsecutiveRecoveryRooms and MaxConsecutiveEnhancementRooms must each be at least 1."));
		if (R.MaxRecoveryRoomsPerFloor < 0 || R.MaxEnhancementRoomsPerFloor < 0)
			return Fail(TEXT("MaxRecoveryRoomsPerFloor and MaxEnhancementRoomsPerFloor must each be at least 0."));
		if (R.Floor2Offset.ContainsNaN()) return Fail(TEXT("Floor2Offset must be finite."));
		if (R.PortalPlacementMode != EHBDungeonPortalPlacementMode::FixedRoomPortals
			&& R.PortalPlacementMode != EHBDungeonPortalPlacementMode::RandomPoints)
			return Fail(TEXT("Unknown portal placement mode."));
		if (R.PortalPlacementMode == EHBDungeonPortalPlacementMode::RandomPoints && R.PortalPairCount < 1)
			return Fail(TEXT("PortalPairCount must be at least 1 in random point mode."));
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
		using CR = EHBDungeonConnectorRole;
		auto HasRole = [](const FHBModuleCatalogEntry& M, CR Role)
		{ return M.Connectors.ContainsByPredicate([Role](const auto& C) { return C.Role == Role; }); };
		if (!HasRole(Catalog[Corridor], CR::DungeonSide) || !HasRole(Catalog[Corridor], CR::BossSide))
		{
			Error = FString::Printf(TEXT("Boss corridor %s requires Dungeon Side and Boss Side connectors."), *Catalog[Corridor].ModuleId.ToString());
			return false;
		}
		for (const auto& M : Catalog)
		{
			if (M.bIsStartCandidate && (M.Kind != EHBDungeonModuleKind::Room || M.Role == EHBPrototypeRoomRole::Boss))
				return Fail(TEXT("Only non-boss Room modules can be start candidates."));
			if (M.Role == EHBPrototypeRoomRole::Boss && !HasRole(M, CR::BossEntrance))
			{
				Error = FString::Printf(TEXT("Boss module %s requires a Boss Entrance connector."), *M.ModuleId.ToString());
				return false;
			}
			for (const auto& C : M.Connectors)
			{
				if ((C.Role == CR::BossEntrance && (M.Kind != EHBDungeonModuleKind::Room || M.Role != EHBPrototypeRoomRole::Boss))
					|| ((C.Role == CR::BossSide || C.Role == CR::DungeonSide) && M.Kind != EHBDungeonModuleKind::Corridor))
				{
					Error = FString::Printf(TEXT("Connector role is invalid for module %s, connector %s. Boss Entrance belongs to a Boss room; Boss Side/Dungeon Side belong to a corridor."), *M.ModuleId.ToString(), *C.ConnectorId.ToString());
					return false;
				}
			}
		}
		if (R.PortalPlacementMode == EHBDungeonPortalPlacementMode::FixedRoomPortals && (R.Floor1ExitPortalId.IsNone() || R.Floor2EntryPortalId.IsNone()
			|| !Catalog[Exit].Portals.ContainsByPredicate([&](const auto& P) { return P.PortalId == R.Floor1ExitPortalId; })
			|| !Catalog[Entry].Portals.ContainsByPredicate([&](const auto& P) { return P.PortalId == R.Floor2EntryPortalId; })))
			return Fail(TEXT("Both selected transition rooms must contain a manually placed Portal Actor with the selected PortalId."));
		if (!S.bAllowModuleReuse && Exit == Entry) return Fail(TEXT("The same transition map on both floors requires Allow Module Reuse."));
		if (!S.bAllowModuleReuse && R.Floor1MainRooms + R.Floor2MainRooms + R.Floor1BranchRooms + R.Floor2BranchRooms
			> Catalog.FilterByPredicate([](const auto& M) { return M.Kind == EHBDungeonModuleKind::Room; }).Num())
			return Fail(TEXT("Not enough distinct room modules for the selected room counts while Allow Module Reuse is off."));
		if (!Catalog.ContainsByPredicate([](const auto& M) { return M.Kind == EHBDungeonModuleKind::Room && M.Role == EHBPrototypeRoomRole::Boss && Allows(M, 2); }))
			return Fail(TEXT("Register at least one Boss module allowed on floor 2."));
		if (!Catalog.ContainsByPredicate([](const auto& M)
			{ return M.Kind == EHBDungeonModuleKind::Room && M.bIsStartCandidate && Allows(M, 1); }))
			return Fail(TEXT("Register at least one non-boss start candidate allowed on floor 1."));
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
			if (I > 0 && R.Connection == EHBDungeonConnection::Door)
			{
				const auto& Parent = Rooms[R.ParentId];
				const auto* PM = Catalog.FindByPredicate([&](const auto& C) { return C.ModuleId == Parent.ModuleId; });
				const auto* PC = PM ? PM->Connectors.FindByPredicate([&](const auto& C) { return C.ConnectorId == R.ParentConnectorId; }) : nullptr;
				const auto* EC = M->Connectors.FindByPredicate([&](const auto& C) { return C.ConnectorId == R.EntryConnectorId; });
				if (!PC || !EC || !MatchesProgression(PC->Role, EC->Role, R.bCorridor, R.Role == EHBPrototypeRoomRole::Boss))
					return Fail(TEXT("Invalid connector progression: None -> Dungeon Side / Boss Side -> Boss Entrance required."));
			}
			if (!S.bAllowModuleReuse && UsedModules.Contains(R.ModuleId)) return Fail(TEXT("Module reused while reuse is disabled."));
			UsedModules.Add(R.ModuleId);
			if (R.bIsStartRoom != (I == 0)) return Fail(TEXT("Exactly the first room instance must be marked as the start room."));
			if (I == 0 && (R.Floor != 1 || !M->bIsStartCandidate || R.Role == EHBPrototypeRoomRole::Boss || R.bBranch || R.bFloorExit || R.bCorridor))
				return Fail(TEXT("Start must be a non-boss start candidate on floor 1."));
			if (R.bBranch && (Children[I] || R.bFloorExit || R.bCorridor || IsFloorTransition(R.Connection)))
				return Fail(TEXT("Optional rooms must be terminal branches, not transition or corridor rooms."));
			if (!R.bBranch && MainChildren[I] != (R.Role == EHBPrototypeRoomRole::Boss ? 0 : 1))
				return Fail(TEXT("Main progression must lead to the sole terminal boss."));
			if (R.bCorridor)
			{
				++Corridors;
				if (R.ModuleMap != Rules.BossCorridorMap || R.Floor != 2 || R.bBranch || R.bFloorExit || I == 0 || Children[I] != 1
					|| R.Connection != EHBDungeonConnection::Door || Rooms[R.ParentId].Role != EHBPrototypeRoomRole::Adventure
					|| IsFloorTransition(Rooms[R.ParentId].Connection) || Rooms[R.ParentId].bFloorExit
					|| !Rooms.ContainsByPredicate([&](const auto& C) { return C.ParentId == I && C.Role == EHBPrototypeRoomRole::Boss; }))
					return Fail(TEXT("Required progression: Adventure -> selected boss corridor -> Boss, after the floor-2 entry room."));
			}
			else
			{
				const int32 Role = static_cast<int32>(R.Role);
				if (Role > 3) return Fail(TEXT("Invalid room role."));
				++Counts[Role];
				++(R.bBranch ? Branch[R.Floor] : Main[R.Floor]);
				if ((R.Role == EHBPrototypeRoomRole::Recovery && ++Heal[R.Floor] > Rules.MaxRecoveryRoomsPerFloor)
					|| (R.Role == EHBPrototypeRoomRole::Upgrade && ++Upgrade[R.Floor] > Rules.MaxEnhancementRoomsPerFloor))
				{
					Error = FString::Printf(TEXT("%s room count exceeds the per-floor limit of %d on floor %d."),
						RoomRoleName(R.Role), SpecialRoomLimit(R.Role, Rules), R.Floor);
					return false;
				}
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
				if (IsFloorTransition(R.Connection))
				{
					++Portals;
					if (P.Floor != 1 || R.Floor != 2 || !P.bFloorExit || R.bBranch || R.ModuleMap != Rules.Floor2EntryMap
						|| !R.Transform.Equals(FTransform(Rules.Floor2Offset) * S.Origin, HBConnectorLayout::SnapTolerance))
						return Fail(TEXT("Invalid configured floor transition."));
					if (Rules.PortalPlacementMode == EHBDungeonPortalPlacementMode::RandomPoints)
					{
						if (R.Connection != EHBDungeonConnection::FloorTransition || !R.ParentPortalId.IsNone() || !R.EntryPortalId.IsNone())
							return Fail(TEXT("Random point mode requires a logical floor transition without a fixed portal pair."));
					}
					else if (R.Connection != EHBDungeonConnection::Portal
						|| R.ParentPortalId != Rules.Floor1ExitPortalId || R.EntryPortalId != Rules.Floor2EntryPortalId)
						return Fail(TEXT("Invalid configured portal pair."));
				}
				else if (P.Floor != R.Floor) return Fail(TEXT("Logical floors may only change through the configured floor transition."));
				if ((R.Role == EHBPrototypeRoomRole::Recovery || R.Role == EHBPrototypeRoomRole::Upgrade)
					&& R.ParentId == 0) return Fail(TEXT("Special room adjacent to start."));
				if (R.Role == EHBPrototypeRoomRole::Boss && (R.Floor != 2 || R.bBranch || !P.bCorridor || Children[I] != 0))
					return Fail(TEXT("Exactly one floor-2 boss must directly follow the selected corridor and end the main path."));
			}
			if ((R.ModuleMap == Rules.Floor1ExitMap || R.ModuleMap == Rules.Floor2EntryMap)
				&& !M->bIsStartCandidate && !R.bFloorExit && !IsFloorTransition(R.Connection))
				return Fail(TEXT("Transition modules are reserved for the configured endpoints."));
			const int32 Limit = ConsecutiveLimit(R.Role, Rules);
			if (Limit > 0 && !R.bCorridor && ConsecutiveRoomCount(Rooms, R.ParentId, R.Floor, R.Role) > Limit)
			{
				Error = FString::Printf(TEXT("More than %d consecutive %s rooms on the same floor (room %d, floor %d)."),
					Limit, RoomRoleName(R.Role), I, R.Floor);
				return false;
			}
		}
		if (Counts[3] != 1 || Portals != 1 || Exits != 1 || Corridors != 1) return Fail(TEXT("Expected one boss, one boss corridor and one portal transition."));
		if (Counts[0] <= FMath::Max3(Counts[1], Counts[2], Counts[3])) return Fail(TEXT("Adventure must be the most common role."));
		if (Main[1] != Rules.Floor1MainRooms || Main[2] != Rules.Floor2MainRooms
			|| Branch[1] != Rules.Floor1BranchRooms || Branch[2] != Rules.Floor2BranchRooms)
			return Fail(TEXT("Generated per-floor room counts do not match Dungeon Rules."));
		return Rules.PortalPlacementMode != EHBDungeonPortalPlacementMode::RandomPoints
			|| HBDungeonRuleLayout::ValidatePortalQuota(Catalog, Rooms, Rules.PortalPairCount, Error);
	}

	struct FSlot
	{
		int32 Floor = 1, Parent = INDEX_NONE, Forced = INDEX_NONE;
		bool bExit = false, bPortal = false, bCorridor = false, bBoss = false, bBranch = false;
	};
	struct FDungeonRuleSearch
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
		int64 PortalCounts[3] = {};
		int32 Steps = 0, Best = 0;
		double Start = FPlatformTime::Seconds();
		bool bLimited = false;
		bool bStepLimitReached = false, bTimeLimitReached = false;
		FRejectionStats AllStats[RejectionCount], DeepestStats[RejectionCount];
		TArray<FHBPrototypeRoom> BestRooms;
		TArray<int32> BestChosen;
		TArray<TArray<bool>> BestUsed;
		FDungeonRuleSearch(const TArray<FHBModuleCatalogEntry>& C, const HBConnectorLayout::FSettings& S, const FHBDungeonGenerationRules& R)
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
			// Preserve the original short-circuit order of the two budget checks.
			const bool bSteps = Steps >= Settings.MaxSearchSteps;
			const bool bSeconds = !bSteps && FPlatformTime::Seconds() - Start >= Settings.MaxSearchSeconds;
			bStepLimitReached |= bSteps; bTimeLimitReached |= bSeconds;
			bLimited |= bSteps || bSeconds;
			return !bLimited;
		}
		void Record(ERejection Reason, TFunctionRef<FString()> MakeExample)
		{
			auto& All = AllStats[static_cast<int32>(Reason)];
			++All.Count;
			const bool bAtBest = Rooms.Num() == Best;
			auto& Deep = DeepestStats[static_cast<int32>(Reason)];
			if (bAtBest) ++Deep.Count;
			if (All.Example.IsEmpty() || (bAtBest && Deep.Example.IsEmpty()))
			{
				const FString Example = MakeExample();
				if (All.Example.IsEmpty()) All.Example = Example;
				if (bAtBest && Deep.Example.IsEmpty()) Deep.Example = Example;
			}
		}
		FString Context(int32 Module, const FSlot& Slot, int32 Parent) const
		{
			return FString::Printf(TEXT("Depth=%d Floor=%d Candidate=%s Map=%s Role=%s ParentRoom=%d ParentModule=%s"),
				Rooms.Num(), Slot.Floor, *Catalog[Module].ModuleId.ToString(), *Catalog[Module].Map.ToString(),
				RoomRoleName(Catalog[Module].Role), Parent,
				Parent == INDEX_NONE ? TEXT("None") : *Rooms[Parent].ModuleId.ToString());
		}
		bool CanUse(int32 Module, const FSlot& Slot, int32 Parent)
		{
			const auto& M = Catalog[Module];
			auto Reject = [&](ERejection Reason)
			{
				Record(Reason, [&] { return Context(Module, Slot, Parent) + FString::Printf(
					TEXT(" AllowedFloor=%d StartCandidate=%s ForcedModule=%s RoleCountOnFloor=%d"),
					static_cast<int32>(M.AllowedFloor), M.bIsStartCandidate ? TEXT("true") : TEXT("false"),
					Slot.Forced == INDEX_NONE ? TEXT("None") : *Catalog[Slot.Forced].ModuleId.ToString(),
					M.Kind == EHBDungeonModuleKind::Room ? RoleCounts[Slot.Floor][static_cast<int32>(M.Role)] : 0); });
				return false;
			};
			if (!Allows(M, Slot.Floor)) return Reject(ERejection::Floor);
			if (!Settings.bAllowModuleReuse && Chosen.Contains(Module)) return Reject(ERejection::Reuse);
			if (Slot.Forced != INDEX_NONE && Module != Slot.Forced) return Reject(ERejection::ForcedModule);
			if (Slot.bCorridor) return M.Kind == EHBDungeonModuleKind::Corridor || Reject(ERejection::Kind);
			if (M.Kind != EHBDungeonModuleKind::Room) return Reject(ERejection::Kind);
			if ((M.Role == EHBPrototypeRoomRole::Boss) != Slot.bBoss) return Reject(ERejection::BossRole);
			if (Slot.Forced == INDEX_NONE && !M.bIsStartCandidate && (M.Map == Rules.Floor1ExitMap || M.Map == Rules.Floor2EntryMap)) return Reject(ERejection::ReservedTransition);
			const bool bBeforeCorridor = Rooms.Num() + 1 < Slots.Num() && Slots[Rooms.Num() + 1].bCorridor;
			if (Parent == INDEX_NONE && !M.bIsStartCandidate) return Reject(ERejection::StartCandidate);
			if (bBeforeCorridor && M.Role != EHBPrototypeRoomRole::Adventure) return Reject(ERejection::BeforeCorridor);
			if (M.Role == EHBPrototypeRoomRole::Recovery || M.Role == EHBPrototypeRoomRole::Upgrade)
			{
				if (RoleCounts[Slot.Floor][static_cast<int32>(M.Role)] >= SpecialRoomLimit(M.Role, Rules)) return Reject(ERejection::SpecialQuota);
				if (Parent == 0) return Reject(ERejection::AdjacentToStart);
			}
			const int32 Limit = ConsecutiveLimit(M.Role, Rules);
			if (Limit > 0 && ConsecutiveRoomCount(Rooms, Parent, Slot.Floor, M.Role) > Limit)
			{
				return Reject(M.Role == EHBPrototypeRoomRole::Adventure ? ERejection::AdventureStreak
					: M.Role == EHBPrototypeRoomRole::Recovery ? ERejection::RecoveryStreak : ERejection::EnhancementStreak);
			}
			return true;
		}
		bool CanSatisfyPortalQuota()
		{
			if (Rules.PortalPlacementMode != EHBDungeonPortalPlacementMode::RandomPoints) return true;
			int64 Maximum[3] = {0, PortalCounts[1], PortalCounts[2]};
			// An upper bound only: future role quotas, geometry and future reuse conflicts may reduce it.
			// Never reject a feasible layout; merely avoid searching branches that cannot supply enough points.
			for (int32 Index = Rooms.Num(); Index < Slots.Num(); ++Index)
			{
				const auto& Slot = Slots[Index];
				int32 BestCount = 0;
				for (int32 Module = 0; Module < Catalog.Num(); ++Module)
				{
					const auto& M = Catalog[Module];
					if (!Allows(M, Slot.Floor) || (Slot.Forced != INDEX_NONE && Module != Slot.Forced)
						|| (!Settings.bAllowModuleReuse && Chosen.Contains(Module))
						|| (M.Kind == EHBDungeonModuleKind::Corridor) != Slot.bCorridor) continue;
					if (!Slot.bCorridor)
					{
						if ((M.Role == EHBPrototypeRoomRole::Boss) != Slot.bBoss || (Index == 0 && !M.bIsStartCandidate)
							|| (Index + 1 < Slots.Num() && Slots[Index + 1].bCorridor && M.Role != EHBPrototypeRoomRole::Adventure)
							|| (Slot.Forced == INDEX_NONE && !M.bIsStartCandidate
								&& (M.Map == Rules.Floor1ExitMap || M.Map == Rules.Floor2EntryMap))) continue;
					}
					BestCount = FMath::Max(BestCount, M.PortalPoints.Num());
				}
				Maximum[Slot.Floor] += BestCount;
			}
			if (Maximum[1] >= Rules.PortalPairCount && Maximum[2] >= Rules.PortalPairCount) return true;
			Record(ERejection::PortalQuota, [&]
			{
				return FString::Printf(TEXT("Depth=%d RequestedPairs=%d Floor1=%lld Floor2=%lld MaximumPossibleFloor1=%lld MaximumPossibleFloor2=%lld"),
					Rooms.Num(), Rules.PortalPairCount, PortalCounts[1], PortalCounts[2], Maximum[1], Maximum[2]);
			});
			return false;
		}
		bool Place(int32 Module, const FSlot& Slot, int32 Parent, int32 Port, int32 Entry, const FTransform& Transform)
		{
			if (!Budget()) return false;
			++Steps;
			const auto& M = Catalog[Module];
			for (int32 I = 0; I < Rooms.Num(); ++I)
			{
				HBConnectorLayout::FOccupancyOverlap Overlap;
				if (HBConnectorLayout::OccupancyOverlaps(M, Transform, Catalog[Chosen[I]], Rooms[I].Transform, &Overlap))
				{
					Record(ERejection::Occupancy, [&] { return Context(Module, Slot, Parent) + FString::Printf(
						TEXT(" CandidateTransform=%s ParentDoor=%s EntryDoor=%s OverlapWithRoom=%d Module=%s Map=%s BoxAIndex=%d CenterA=%s RotationA=%s HalfExtentA=%s BoxBIndex=%d CenterB=%s RotationB=%s HalfExtentB=%s"),
						*Transform.ToString(), Port == INDEX_NONE ? TEXT("None") : *Catalog[Chosen[Parent]].Connectors[Port].ConnectorId.ToString(),
						Entry == INDEX_NONE ? TEXT("None") : *M.Connectors[Entry].ConnectorId.ToString(),
						I, *Rooms[I].ModuleId.ToString(), *Rooms[I].ModuleMap.ToString(), Overlap.BoxAIndex,
						*Overlap.BoxAWorld.GetLocation().ToString(), *Overlap.BoxAWorld.Rotator().ToString(), *Overlap.ExtentA.ToString(),
						Overlap.BoxBIndex, *Overlap.BoxBWorld.GetLocation().ToString(), *Overlap.BoxBWorld.Rotator().ToString(), *Overlap.ExtentB.ToString()); });
					return false;
				}
			}
			FHBPrototypeRoom R;
			R.Id = Rooms.Num(); R.ParentId = Parent; R.Floor = Slot.Floor; R.Role = M.Role;
			R.bBranch = Slot.bBranch; R.bFloorExit = Slot.bExit; R.bCorridor = Slot.bCorridor;
			R.bIsStartRoom = Parent == INDEX_NONE;
			R.ModuleId = M.ModuleId; R.ModuleMap = M.Map; R.Transform = Transform;
			R.LocalBounds = M.Bounds; R.Bounds = M.Bounds.TransformBy(Transform); R.InspectionEntry = M.InspectionEntry;
			if (Slot.bPortal)
			{
				R.Connection = Rules.PortalPlacementMode == EHBDungeonPortalPlacementMode::RandomPoints
					? EHBDungeonConnection::FloorTransition : EHBDungeonConnection::Portal;
				if (R.Connection == EHBDungeonConnection::Portal)
				{
					R.ParentPortalId = Rules.Floor1ExitPortalId; R.EntryPortalId = Rules.Floor2EntryPortalId;
				}
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
			PortalCounts[Slot.Floor] += M.PortalPoints.Num();
			if (Rooms.Num() > Best)
			{
				Best = Rooms.Num();
				BestRooms = Rooms; BestChosen = Chosen; BestUsed = Used;
				for (auto& Stat : DeepestStats) Stat = FRejectionStats();
			}
			if (Extend()) return true;
			if (!Slot.bCorridor) --RoleCounts[Slot.Floor][static_cast<int32>(M.Role)];
			PortalCounts[Slot.Floor] -= M.PortalPoints.Num();
			Rooms.Pop(); Chosen.Pop(); Used.Pop();
			if (Port != INDEX_NONE) Used[Parent][Port] = false;
			return false;
		}
		bool Extend()
		{
			if (!CanSatisfyPortalQuota()) return false;
			if (Rooms.Num() == Slots.Num())
			{
				FString Error;
				if (CheckRules(Catalog, Settings, Rules, Rooms, Error)) return true;
				Record(ERejection::FinalRules, [&] { return FString::Printf(TEXT("Depth=%d Reason=%s"), Rooms.Num(), *Error); });
				return false;
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
			if (Parents.IsEmpty()) Record(ERejection::NoParents, [&] { return FString::Printf(TEXT("Depth=%d Floor=%d Branch=%s"),
				Rooms.Num(), Slot.Floor, Slot.bBranch ? TEXT("true") : TEXT("false")); });
			for (int32 Parent : Parents)
			{
				TArray<int32> Candidates;
				for (int32 M = 0; M < Catalog.Num(); ++M) if (CanUse(M, Slot, Parent)) Candidates.Add(M);
				if (Candidates.IsEmpty()) Record(ERejection::NoCandidates, [&] { return FString::Printf(
					TEXT("Depth=%d Floor=%d ParentRoom=%d ParentModule=%s ForcedModule=%s; see candidate-filter counts"),
					Rooms.Num(), Slot.Floor, Parent, Parent == INDEX_NONE ? TEXT("None") : *Rooms[Parent].ModuleId.ToString(),
					Slot.Forced == INDEX_NONE ? TEXT("None") : *Catalog[Slot.Forced].ModuleId.ToString()); });
				if (Parent == INDEX_NONE)
					Candidates.Sort([&](int32 A, int32 B) { return Catalog[A].ModuleId.LexicalLess(Catalog[B].ModuleId); });
				Shuffle(Candidates);
				// Upgrade is preferred on optional branches; retain random order within each priority.
				Candidates.StableSort([&](int32 A, int32 B)
				{
					if (Parent == INDEX_NONE) return false; // Start roles have no priority over one another.
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
						if (Ports.IsEmpty()) Record(ERejection::NoParentDoors, [&] { return Context(Module, Slot, Parent)
							+ FString::Printf(TEXT(" UsedDoors=%d/%d"), Used[Parent].Num(), Catalog[Chosen[Parent]].Connectors.Num()); });
						for (int32 Port : Ports)
							for (int32 Entry : Entries)
							{
								if (!MatchesProgression(Catalog[Chosen[Parent]].Connectors[Port].Role,
									Catalog[Module].Connectors[Entry].Role, Slot.bCorridor, Slot.bBoss))
								{
									Record(ERejection::ConnectorRole, [&] { return Context(Module, Slot, Parent) + FString::Printf(
										TEXT(" ParentDoor=%s ParentRole=%s EntryDoor=%s EntryRole=%s Corridor=%s Boss=%s"),
										*Catalog[Chosen[Parent]].Connectors[Port].ConnectorId.ToString(), ConnectorRoleName(Catalog[Chosen[Parent]].Connectors[Port].Role),
										*Catalog[Module].Connectors[Entry].ConnectorId.ToString(), ConnectorRoleName(Catalog[Module].Connectors[Entry].Role),
										Slot.bCorridor ? TEXT("true") : TEXT("false"), Slot.bBoss ? TEXT("true") : TEXT("false")); });
									continue;
								}
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
		const TCHAR* StopReason() const
		{
			if (bStepLimitReached) return TEXT("AttemptLimit");
			if (bTimeLimitReached) return TEXT("TimeLimit");
			return TEXT("Exhausted");
		}
		FString CountSummary(const FRejectionStats* Stats, int32 Begin, int32 End) const
		{
			FString Summary;
			for (int32 I = Begin; I < End; ++I)
				if (Stats[I].Count > 0)
				{
					if (!Summary.IsEmpty()) Summary += TEXT(", ");
					Summary += FString::Printf(TEXT("%s=%lld"), RejectionName(static_cast<ERejection>(I)), Stats[I].Count);
				}
			return Summary.IsEmpty() ? TEXT("none") : Summary;
		}
		void LogStats(const TCHAR* Scope, const FRejectionStats* Stats) const
		{
			UE_LOG(LogHBDungeonLayout, Warning, TEXT("[CandidateFilters] Scope=%s Counts=%s (normal filtering; not placement failures)"),
				Scope, *CountSummary(Stats, 0, FilterReasonCount));
			UE_LOG(LogHBDungeonLayout, Warning, TEXT("[PlacementRejections] Scope=%s Counts=%s"),
				Scope, *CountSummary(Stats, FilterReasonCount, RejectionCount));
			// At most eight samples per scope, prioritizing placement failures over normal filters.
			int32 Shown = 0;
			for (int32 Pass = 0; Pass < 2; ++Pass)
				for (int32 I = Pass == 0 ? FilterReasonCount : 0;
					I < (Pass == 0 ? RejectionCount : FilterReasonCount) && Shown < 8; ++I)
					if (!Stats[I].Example.IsEmpty())
					{
						UE_LOG(LogHBDungeonLayout, Warning, TEXT("[Example] Scope=%s Rejection=%s %s"),
							Scope, RejectionName(static_cast<ERejection>(I)), *Stats[I].Example);
						++Shown;
					}
		}
		void LogParentSnapshot(int32 Parent) const
		{
			if (Parent == INDEX_NONE)
			{
				UE_LOG(LogHBDungeonLayout, Warning, TEXT("[ParentSnapshot] Start uses Origin; no parent door required."));
				return;
			}
			const auto& M = Catalog[BestChosen[Parent]];
			FString Doors;
			int32 Available = 0;
			for (int32 I = 0; I < BestUsed[Parent].Num(); ++I)
				if (!BestUsed[Parent][I])
				{
					if (++Available > 8) continue;
					const auto& Door = M.Connectors[I];
					if (!Doors.IsEmpty()) Doors += TEXT("; ");
					Doors += FString::Printf(TEXT("%s(%s)@%s"), *Door.ConnectorId.ToString(), ConnectorRoleName(Door.Role),
						*(Door.LocalTransform * BestRooms[Parent].Transform).GetLocation().ToString());
				}
			UE_LOG(LogHBDungeonLayout, Warning, TEXT("[ParentSnapshot] Room=%d Module=%s Map=%s AvailableDoors=%d/%d FirstEight=%s"),
				Parent, *M.ModuleId.ToString(), *M.Map.ToString(), Available, M.Connectors.Num(), Doors.IsEmpty() ? TEXT("none") : *Doors);
		}
		void LogFailure(const FString& Error) const
		{
			LogFailureSettings(Settings, Rules, TEXT("Search"), Error);
			UE_LOG(LogHBDungeonLayout, Warning, TEXT("[Search] Stop=%s Attempts=%d/%d ElapsedSeconds=%.6f Best=%d/%d CatalogModules=%d"),
				StopReason(), Steps, Settings.MaxSearchSteps, FPlatformTime::Seconds() - Start, Best, Slots.Num(), Catalog.Num());
			UE_LOG(LogHBDungeonLayout, Warning, TEXT("[Deepest] Snapshot=FirstContextAtBest; Scope=Deepest aggregates all contexts at Depth=%d. Examples may belong to different branches."), Best);
			if (Best < Slots.Num())
			{
				const auto& Slot = Slots[Best];
				const TCHAR* Kind = Slot.bBranch ? TEXT("Branch") : Slot.bBoss ? TEXT("Boss") : Slot.bCorridor ? TEXT("Corridor")
					: Slot.bPortal ? TEXT("PortalEntry") : Slot.bExit ? TEXT("FloorExit") : Best == 0 ? TEXT("Start") : TEXT("Room");
				UE_LOG(LogHBDungeonLayout, Warning, TEXT("[NextSlot] Room=%d Floor=%d Kind=%s ParentRoom=%d ForcedModule=%s"),
					Best, Slot.Floor, Kind, Slot.Parent, Slot.Forced == INDEX_NONE ? TEXT("None") : *Catalog[Slot.Forced].ModuleId.ToString());
				if (Slot.bBranch)
				{
					int32 Parents = 0;
					for (int32 I = 0; I < BestRooms.Num(); ++I)
						if (BestRooms[I].Floor == Slot.Floor && !BestRooms[I].bBranch && !BestRooms[I].bCorridor && BestRooms[I].Role != EHBPrototypeRoomRole::Boss)
							if (++Parents <= 6) LogParentSnapshot(I);
					UE_LOG(LogHBDungeonLayout, Warning, TEXT("[ParentSnapshot] EligibleBranchParents=%d (showing up to six)."), Parents);
				}
				else LogParentSnapshot(Slot.Parent);
			}
			else UE_LOG(LogHBDungeonLayout, Warning, TEXT("[NextSlot] All slots filled; final rule validation rejected the layouts."));
			int32 Counts[3][4] = {};
			for (const auto& Room : BestRooms)
				if (!Room.bCorridor) ++Counts[Room.Floor][static_cast<int32>(Room.Role)];
			for (int32 Floor = 1; Floor <= 2; ++Floor)
				UE_LOG(LogHBDungeonLayout, Warning, TEXT("[SnapshotRoles] Floor=%d Adventure=%d Recovery=%d Upgrade=%d Boss=%d"),
					Floor, Counts[Floor][0], Counts[Floor][1], Counts[Floor][2], Counts[Floor][3]);
			if (Best > 6) UE_LOG(LogHBDungeonLayout, Warning, TEXT("[RoomSnapshot] %d earlier rooms omitted; showing last six."), Best - 6);
			for (int32 I = FMath::Max(0, Best - 6); I < Best; ++I)
				UE_LOG(LogHBDungeonLayout, Warning, TEXT("[RoomSnapshot] Room=%d Module=%s Map=%s Floor=%d Role=%s ParentRoom=%d ParentDoor=%s EntryDoor=%s Position=%s"),
					I, *BestRooms[I].ModuleId.ToString(), *BestRooms[I].ModuleMap.ToString(), BestRooms[I].Floor, RoomRoleName(BestRooms[I].Role),
					BestRooms[I].ParentId, *BestRooms[I].ParentConnectorId.ToString(), *BestRooms[I].EntryConnectorId.ToString(), *BestRooms[I].Transform.GetLocation().ToString());
			LogStats(TEXT("All"), AllStats);
			LogStats(TEXT("Deepest"), DeepestStats);
		}
	};
}

bool HBDungeonRuleLayout::ResolvePortalIds(const TArray<FHBModuleCatalogEntry>& Catalog,
    FHBDungeonGenerationRules& Rules, FString& OutError)
{
    OutError.Reset();
    if (Rules.PortalPlacementMode == EHBDungeonPortalPlacementMode::RandomPoints) return true;
    if (Rules.PortalPlacementMode != EHBDungeonPortalPlacementMode::FixedRoomPortals)
    { OutError = TEXT("Unknown portal placement mode."); return false; }
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
	if (!ResolvePortalIds(Catalog, Rules, OutError))
	{
		LogFailureSettings(Settings, Rules, TEXT("PortalSelection"), OutError);
		return false;
	}
	if (!CheckSettings(Catalog, Settings, Rules, OutError))
	{
		LogFailureSettings(Settings, Rules, TEXT("Settings"), OutError);
		return false;
	}
	FDungeonRuleSearch Search(Catalog, Settings, Rules);
	if (!Search.Extend())
	{
		OutError = FString::Printf(TEXT("%s (%s): placed at most %d/%d modules in %d attempts. Deepest placement rejections: %s. See LogHBDungeonLayout for settings, candidate filters, parent doors and representative failures."),
			Search.bLimited ? TEXT("Search limit reached") : TEXT("No rule-compliant layout"), Search.StopReason(),
			Search.Best, Search.Slots.Num(), Search.Steps, *Search.CountSummary(Search.DeepestStats, FilterReasonCount, RejectionCount));
		const auto& Quota = Search.AllStats[static_cast<int32>(ERejection::PortalQuota)];
		if (Quota.Count > 0) OutError += TEXT(" PortalPointQuota: ") + Quota.Example;
		Search.LogFailure(OutError);
		return false;
	}
	for (int32 I = 0; I < Search.Rooms.Num(); ++I)
		for (int32 C = 0; C < Search.Used[I].Num(); ++C)
			(Search.Used[I][C] ? Search.Rooms[I].UsedConnectorIds : Search.Rooms[I].UnusedConnectorIds).Add(Catalog[Search.Chosen[I]].Connectors[C].ConnectorId);
	if (!Validate(Catalog, Settings, Rules, Search.Rooms, OutError))
	{
		LogFailureSettings(Settings, Rules, TEXT("FinalValidation"), OutError);
		return false;
	}
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

bool HBDungeonRuleLayout::CollectPortalCandidates(const TArray<FHBModuleCatalogEntry>& Catalog,
	const TArray<FHBPrototypeRoom>& Rooms, TArray<HBPortalPlacement::FCandidate>& OutCandidates, FString& OutError)
{
	OutCandidates.Reset(); OutError.Reset();
	if (!HBConnectorLayout::ValidateCatalog(Catalog, OutError)) return false;
	if (Rooms.IsEmpty()) { OutError = TEXT("No selected room instances for portal candidate collection."); return false; }
	TArray<HBPortalPlacement::FCandidate> Pending;
	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		const auto& Room = Rooms[Index];
		const auto* Module = Catalog.FindByPredicate([&](const auto& M) { return M.ModuleId == Room.ModuleId; });
		if (!Module || Module->Map != Room.ModuleMap || Room.Id != Index || Room.Floor < 1 || Room.Floor > 2
			|| !Allows(*Module, Room.Floor) || !Room.Transform.IsValid()
			|| !Room.Transform.GetScale3D().Equals(FVector::OneVector, 0.0001))
		{
			OutError = FString::Printf(TEXT("Invalid selected room metadata for portal candidates: Room=%d Module=%s Floor=%d."),
				Room.Id, *Room.ModuleId.ToString(), Room.Floor);
			return false;
		}
		for (const auto& Point : Module->PortalPoints)
			Pending.Add({{Room.Id, Point.PointId}, Room.Floor, Point.LocalTransform * Room.Transform});
	}
	Pending.Sort([](const auto& A, const auto& B)
	{
		return A.Key.RoomId != B.Key.RoomId ? A.Key.RoomId < B.Key.RoomId : A.Key.PointId.LexicalLess(B.Key.PointId);
	});
	OutCandidates = MoveTemp(Pending);
	return true;
}

bool HBDungeonRuleLayout::ValidatePortalQuota(const TArray<FHBModuleCatalogEntry>& Catalog,
	const TArray<FHBPrototypeRoom>& Rooms, int32 PairCount, FString& OutError)
{
	OutError.Reset();
	if (PairCount < 1) { OutError = TEXT("PortalPairCount must be at least 1."); return false; }
	TArray<HBPortalPlacement::FCandidate> Candidates;
	if (!CollectPortalCandidates(Catalog, Rooms, Candidates, OutError)) return false;
	int32 Counts[3] = {};
	for (const auto& Candidate : Candidates) ++Counts[Candidate.Floor];
	if (Counts[1] < PairCount || Counts[2] < PairCount)
	{
		OutError = FString::Printf(TEXT("Insufficient portal points in selected room instances: requested %d pairs; Floor1=%d, Floor2=%d (each floor needs at least %d)."),
			PairCount, Counts[1], Counts[2], PairCount);
		return false;
	}
	return true;
}
