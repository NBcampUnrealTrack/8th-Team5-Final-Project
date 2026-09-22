#include "PCGModule/HBConnectorLayout.h"
#include "HAL/PlatformTime.h"

namespace
{
	bool Unit(const FTransform& T)
	{
		return T.IsValid() && T.GetScale3D().Equals(FVector::OneVector, 0.0001);
	}
	bool Upright(const FTransform& T)
	{
		return Unit(T) && T.GetRotation().GetUpVector().Equals(FVector::UpVector, 0.0001);
	}
	bool Positive(const FVector& V)
	{
		return !V.ContainsNaN() && V.X > 0 && V.Y > 0 && V.Z > 0;
	}
	struct FOrientedBox
	{
		FVector Center;
		FVector Axis[3];
		FVector Extent;
	};
	FOrientedBox WorldBox(const FHBDungeonOccupancyDefinition& Box, const FTransform& Room)
	{
		const FTransform T = Box.LocalTransform * Room;
		return { T.GetLocation(), { T.GetUnitAxis(EAxis::X), T.GetUnitAxis(EAxis::Y), T.GetUnitAxis(EAxis::Z) }, Box.Extent };
	}
	bool BoxesOverlap(const FOrientedBox& A, const FOrientedBox& B)
	{
		const FVector Delta = B.Center - A.Center;
		auto Separated = [&](FVector Axis)
		{
			const double Length = Axis.Size();
			if (Length < 1.e-8) { return false; }
			Axis /= Length;
			double Radius = 0;
			for (int32 I = 0; I < 3; ++I)
				Radius += A.Extent[I] * FMath::Abs(FVector::DotProduct(A.Axis[I], Axis))
					+ B.Extent[I] * FMath::Abs(FVector::DotProduct(B.Axis[I], Axis));
			return FMath::Abs(FVector::DotProduct(Delta, Axis)) >= Radius - HBConnectorLayout::ContactTolerance;
		};
		for (int32 I = 0; I < 3; ++I)
		{
			if (Separated(A.Axis[I]) || Separated(B.Axis[I])) { return false; }
			for (int32 J = 0; J < 3; ++J)
				if (Separated(FVector::CrossProduct(A.Axis[I], B.Axis[J]))) { return false; }
		}
		return true;
	}
	bool CheckCatalog(const TArray<FHBModuleCatalogEntry>& Catalog, TArray<int32>& Modules, FString& Error)
	{
		TSet<FName> ModuleIds;
		for (int32 Index = 0; Index < Catalog.Num(); ++Index)
		{
			const auto& M = Catalog[Index];
			if (M.Kind != EHBDungeonModuleKind::Room) { continue; }
			auto Fail = [&](const FString& Why) { Error = M.ModuleId.ToString() + TEXT(": ") + Why; return false; };
			if (M.ModuleId.IsNone() || ModuleIds.Contains(M.ModuleId) || M.Map.IsNull() || static_cast<uint8>(M.Role) > 3)
				return Fail(TEXT("Invalid/duplicate module ID, map or fixed role."));
			ModuleIds.Add(M.ModuleId);
			if (!M.Bounds.IsValid || M.Bounds.Min.ContainsNaN() || M.Bounds.Max.ContainsNaN() || M.InspectionEntry.ContainsNaN())
				return Fail(TEXT("Invalid bounds or inspection entry."));
			if (M.Connectors.IsEmpty() || M.Occupancy.IsEmpty())
				return Fail(TEXT("At least one connector and occupancy box are required."));
			TSet<FName> ConnectorIds;
			for (const auto& C : M.Connectors)
			{
				if (C.ConnectorId.IsNone() || ConnectorIds.Contains(C.ConnectorId))
					return Fail(TEXT("Invalid/duplicate connector ID."));
				ConnectorIds.Add(C.ConnectorId);
				if (!Upright(C.LocalTransform))
					return Fail(TEXT("Connector ") + C.ConnectorId.ToString() + TEXT(" must point horizontally with +Z up and unit scale. Move/rotate it in yaw; do not tilt it."));
			}
			FBox OccupiedBounds(ForceInit);
			for (const auto& B : M.Occupancy)
			{
				if (!Unit(B.LocalTransform) || !Positive(B.Extent))
					return Fail(TEXT("Invalid extracted occupancy. Refresh the catalog after editing."));
				OccupiedBounds += FBox(-B.Extent, B.Extent).TransformBy(B.LocalTransform);
			}
			if (!M.Bounds.Equals(OccupiedBounds, HBConnectorLayout::SnapTolerance))
				return Fail(TEXT("Cached bounds do not match occupancy boxes. Refresh the catalog."));
			Modules.Add(Index);
		}
		if (Modules.IsEmpty()) { Error = TEXT("No room modules available for connector tests."); return false; }
		return true;
	}
	const FHBDungeonConnectorDefinition* FindConnector(const FHBModuleCatalogEntry& M, FName Id)
	{
		return M.Connectors.FindByPredicate([Id](const auto& C) { return C.ConnectorId == Id; });
	}

	struct FSearch
	{
		const TArray<FHBModuleCatalogEntry>& Catalog;
		const HBConnectorLayout::FSettings& Settings;
		TArray<int32> Modules;
		FRandomStream Random;
		TArray<FHBPrototypeRoom> Rooms;
		TArray<int32> Chosen;
		TArray<TArray<bool>> Used;
		double Start = FPlatformTime::Seconds();
		int32 Steps = 0;
		int32 BestCount = 0;
		int32 OverlapRejects = 0;
		bool bBudgetExceeded = false;
		FString LastOverlap;

		FSearch(const TArray<FHBModuleCatalogEntry>& InCatalog, const HBConnectorLayout::FSettings& InSettings)
			: Catalog(InCatalog), Settings(InSettings), Random(InSettings.Seed) {}

		template<class T> void Shuffle(TArray<T>& Items)
		{
			for (int32 I = Items.Num()-1; I > 0; --I) { Items.Swap(I, Random.RandRange(0,I)); }
		}
		bool Budget()
		{
			bBudgetExceeded |= Steps >= Settings.MaxSearchSteps || FPlatformTime::Seconds()-Start >= Settings.MaxSearchSeconds;
			return !bBudgetExceeded;
		}
		void Push(int32 Module, int32 Parent, const FTransform& Transform, FName ParentConnector = NAME_None, FName Entry = NAME_None)
		{
			const auto& M = Catalog[Module];
			FHBPrototypeRoom R;
			R.Id = Rooms.Num(); R.ParentId = Parent; R.Role = M.Role;
			R.ModuleId = M.ModuleId; R.ModuleMap = M.Map;
			R.Transform = Transform; R.LocalBounds = M.Bounds; R.Bounds = M.Bounds.TransformBy(Transform);
			R.InspectionEntry = M.InspectionEntry;
			R.ParentConnectorId = ParentConnector; R.EntryConnectorId = Entry;
			// LD test groups are not gameplay floors; actual height comes exclusively from connectors.
			Rooms.Add(R); Chosen.Add(Module);
			Used.AddDefaulted_GetRef().Init(false, M.Connectors.Num());
			BestCount = FMath::Max(BestCount, Rooms.Num());
		}
		void Pop() { Rooms.Pop(); Chosen.Pop(); Used.Pop(); }

		bool Extend()
		{
			if (Rooms.Num() == Settings.RoomCount) { return true; }
			if (!Budget()) { return false; }
			TArray<FIntPoint> Ports;
			for (int32 I = 0; I < Rooms.Num(); ++I)
				for (int32 C = 0; C < Used[I].Num(); ++C)
					if (!Used[I][C]) { Ports.Emplace(I,C); }
			Shuffle(Ports);
			for (const FIntPoint Port : Ports)
			{
				const auto& ParentConnector = Catalog[Chosen[Port.X]].Connectors[Port.Y];
				const FTransform ParentWorld = ParentConnector.LocalTransform * Rooms[Port.X].Transform;
				TArray<int32> Candidates = Modules; Shuffle(Candidates);
				for (int32 Module : Candidates)
				{
					if (!Settings.bAllowModuleReuse && Chosen.Contains(Module)) { continue; }
					const auto& M = Catalog[Module];
					TArray<int32> Entries;
					for (int32 I = 0; I < M.Connectors.Num(); ++I) { Entries.Add(I); }
					Shuffle(Entries);
					for (int32 Entry : Entries)
					{
						if (!Budget()) { return false; }
						++Steps;
						const auto& Connector = M.Connectors[Entry];
						const FTransform Transform = HBConnectorLayout::Align(ParentWorld, Connector.LocalTransform);
						if (!Upright(Transform)) { continue; }
						bool bOverlap = false;
						for (int32 I = 0; I < Rooms.Num(); ++I)
						{
							if (HBConnectorLayout::OccupancyOverlaps(M, Transform, Catalog[Chosen[I]], Rooms[I].Transform))
							{
								bOverlap = true; ++OverlapRejects;
								LastOverlap = FString::Printf(TEXT("%s/%s against R%d %s"), *M.ModuleId.ToString(), *Connector.ConnectorId.ToString(), I, *Rooms[I].ModuleId.ToString());
								break;
							}
						}
						if (bOverlap) { continue; }
						Used[Port.X][Port.Y] = true;
						Push(Module, Port.X, Transform, ParentConnector.ConnectorId, Connector.ConnectorId);
						Used.Last()[Entry] = true;
						if (Extend()) { return true; }
						Pop(); Used[Port.X][Port.Y] = false;
						if (bBudgetExceeded) { return false; }
						// Nothing references TArray elements across recursive growth.
					}
				}
			}
			return false;
		}
	};
}

FTransform HBConnectorLayout::Align(const FTransform& ParentConnectorWorld, const FTransform& ChildConnectorLocal)
{
	return ChildConnectorLocal.Inverse() * FTransform(FQuat(FVector::UpVector, PI)) * ParentConnectorWorld;
}

bool HBConnectorLayout::OccupancyOverlaps(const FHBModuleCatalogEntry& A, const FTransform& TransformA,
	const FHBModuleCatalogEntry& B, const FTransform& TransformB)
{
	if (!A.Bounds.TransformBy(TransformA).Intersect(B.Bounds.TransformBy(TransformB))) { return false; }
	for (const auto& BoxA : A.Occupancy)
		for (const auto& BoxB : B.Occupancy)
			if (BoxesOverlap(WorldBox(BoxA,TransformA), WorldBox(BoxB,TransformB))) { return true; }
	return false;
}

bool HBConnectorLayout::Generate(const TArray<FHBModuleCatalogEntry>& Catalog, const FSettings& Settings,
	TArray<FHBPrototypeRoom>& OutRooms, FString& OutError)
{
	OutRooms.Reset(); OutError.Reset();
	if (Settings.RoomCount < 1 || Settings.RoomCount > 64 || Settings.MaxSearchSteps < 1 ||
		!FMath::IsFinite(Settings.MaxSearchSeconds) || Settings.MaxSearchSeconds <= 0 || !Upright(Settings.Origin))
	{
		OutError = TEXT("Invalid connector-test settings. Use 1-64 rooms, positive search limits, and a generator with unit scale and +Z up.");
		return false;
	}
	FSearch Search(Catalog, Settings);
	if (!CheckCatalog(Catalog, Search.Modules, OutError)) { return false; }
	if (!Settings.bAllowModuleReuse && Settings.RoomCount > Search.Modules.Num())
	{
		OutError = TEXT("Requested more rooms than registered modules while Allow Module Reuse is off."); return false;
	}
	Search.Shuffle(Search.Modules);
	for (int32 Root : Search.Modules)
	{
		if (!Search.Budget()) { break; }
		++Search.Steps;
		Search.Push(Root, INDEX_NONE, Settings.Origin);
		if (Search.Extend())
		{
			for (int32 I = 0; I < Search.Rooms.Num(); ++I)
				for (int32 C = 0; C < Search.Used[I].Num(); ++C)
					(Search.Used[I][C] ? Search.Rooms[I].UsedConnectorIds : Search.Rooms[I].UnusedConnectorIds)
						.Add(Catalog[Search.Chosen[I]].Connectors[C].ConnectorId);
			if (!Validate(Catalog, Search.Rooms, OutError)) { return false; }
			OutRooms = MoveTemp(Search.Rooms); return true;
		}
		Search.Pop();
	}
	OutError = FString::Printf(TEXT("%s: placed at most %d/%d rooms; %d attempts, %d occupancy overlaps. Check available doors, outward arrows, and occupancy at connection planes. Last overlap: %s"),
		Search.bBudgetExceeded ? TEXT("Search limit reached") : TEXT("No fitting connector layout"),
		Search.BestCount, Settings.RoomCount, Search.Steps, Search.OverlapRejects, *Search.LastOverlap);
	return false;
}

bool HBConnectorLayout::Validate(const TArray<FHBModuleCatalogEntry>& Catalog,
	const TArray<FHBPrototypeRoom>& Rooms, FString& OutError)
{
	OutError.Reset();
	auto Fail = [&](const FString& Why) { OutError = Why; return false; };
	TArray<int32> Modules;
	if (!CheckCatalog(Catalog, Modules, OutError)) { return false; }
	if (Rooms.IsEmpty()) { return Fail(TEXT("No connected rooms.")); }
	TArray<int32> Chosen;
	TArray<TSet<FName>> Used; Used.SetNum(Rooms.Num());
	for (int32 I = 0; I < Rooms.Num(); ++I)
	{
		const auto& R = Rooms[I];
		const int32 Module = Catalog.IndexOfByPredicate([&](const auto& M) { return M.Kind == EHBDungeonModuleKind::Room && M.ModuleId == R.ModuleId; });
		if (Module == INDEX_NONE || R.Id != I || !Upright(R.Transform)) { return Fail(TEXT("Invalid placed room metadata/transform.")); }
		const auto& M = Catalog[Module];
		Chosen.Add(Module);
		if (R.ModuleMap != M.Map || R.Role != M.Role || !R.Bounds.Equals(M.Bounds.TransformBy(R.Transform), SnapTolerance))
			return Fail(TEXT("Placed room does not match its source module."));
		if (I == 0)
		{
			if (R.ParentId != INDEX_NONE || !R.ParentConnectorId.IsNone() || !R.EntryConnectorId.IsNone())
				return Fail(TEXT("Invalid root room."));
		}
		else
		{
			if (R.ParentId < 0 || R.ParentId >= I) { return Fail(TEXT("Disconnected/cyclic room graph.")); }
			const auto* P = FindConnector(Catalog[Chosen[R.ParentId]], R.ParentConnectorId);
			const auto* C = FindConnector(M, R.EntryConnectorId);
			if (!P || !C || Used[R.ParentId].Contains(R.ParentConnectorId) || Used[I].Contains(R.EntryConnectorId))
				return Fail(TEXT("Missing or reused connector."));
			Used[R.ParentId].Add(R.ParentConnectorId); Used[I].Add(R.EntryConnectorId);
			const FTransform PW = P->LocalTransform * Rooms[R.ParentId].Transform;
			const FTransform CW = C->LocalTransform * R.Transform;
			if (!PW.GetLocation().Equals(CW.GetLocation(), SnapTolerance) ||
				FVector::DotProduct(PW.GetRotation().GetForwardVector(), CW.GetRotation().GetForwardVector()) > -0.9999 ||
				!PW.GetRotation().GetUpVector().Equals(CW.GetRotation().GetUpVector(), 0.0001))
				return Fail(TEXT("Connector position/direction mismatch."));
		}
		for (int32 J = 0; J < I; ++J)
			if (OccupancyOverlaps(M, R.Transform, Catalog[Chosen[J]], Rooms[J].Transform))
				return Fail(FString::Printf(TEXT("Occupancy overlap between R%d and R%d."), J, I));
	}
	for (int32 I = 0; I < Rooms.Num(); ++I)
	{
		TSet<FName> Reported;
		for (FName Id : Rooms[I].UsedConnectorIds)
		{
			if (!Used[I].Contains(Id) || Reported.Contains(Id)) { return Fail(TEXT("Invalid used-connector report.")); }
			Reported.Add(Id);
		}
		if (Reported.Num() != Used[I].Num()) { return Fail(TEXT("Incomplete used-connector report.")); }
		for (FName Id : Rooms[I].UnusedConnectorIds)
		{
			if (!FindConnector(Catalog[Chosen[I]], Id) || Reported.Contains(Id)) { return Fail(TEXT("Invalid unused-connector report.")); }
			Reported.Add(Id);
		}
		if (Reported.Num() != Catalog[Chosen[I]].Connectors.Num()) { return Fail(TEXT("Incomplete connector report.")); }
	}
	return true;
}
