#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonPortal.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Misc/PackageName.h"

bool HBDungeonRuntimeValidation::ValidateLayoutForLoading(const TArray<FHBPrototypeRoom>& Rooms, FString& Error)
{
    Error.Reset();
    if (Rooms.IsEmpty()) { Error = TEXT("Layout has no rooms."); return false; }
    TArray<TSet<FName>> PortalEndpoints;
    PortalEndpoints.SetNum(Rooms.Num());
    for (int32 Index = 0; Index < Rooms.Num(); ++Index)
    {
        const auto& R = Rooms[Index];
        if (R.Id != Index || R.ModuleId.IsNone() || R.ModuleMap.IsNull() ||
            !FPackageName::IsValidLongPackageName(R.ModuleMap.ToSoftObjectPath().GetLongPackageName()) ||
            !R.Transform.IsValid() || !R.Transform.GetScale3D().Equals(FVector::OneVector, 0.0001) ||
            (Index == 0 ? R.ParentId != INDEX_NONE : R.ParentId < 0 || R.ParentId >= Index))
        { Error = TEXT("Layout needs contiguous room IDs, valid modules/transforms and preceding parent IDs."); return false; }
        if (R.Connection != EHBDungeonConnection::Door && R.Connection != EHBDungeonConnection::Portal)
        { Error = TEXT("Unknown room connection type."); return false; }
        if (R.Connection == EHBDungeonConnection::Portal)
        {
            if (Index == 0 || R.ParentPortalId.IsNone() || R.EntryPortalId.IsNone())
            { Error = TEXT("Portal connections require a preceding parent and stable portal IDs."); return false; }
            if (PortalEndpoints[R.ParentId].Contains(R.ParentPortalId) || PortalEndpoints[Index].Contains(R.EntryPortalId))
            { Error = TEXT("A portal endpoint cannot be reused by another connection in the same room instance."); return false; }
            PortalEndpoints[R.ParentId].Add(R.ParentPortalId);
            PortalEndpoints[Index].Add(R.EntryPortalId);
        }
        TSet<FName> Seen;
        for (FName Id : R.UsedConnectorIds)
        {
            if (Id.IsNone() || Seen.Contains(Id)) { Error = TEXT("Invalid or duplicate used connector ID."); return false; }
            Seen.Add(Id);
        }
        for (FName Id : R.UnusedConnectorIds)
        {
            if (Id.IsNone() || Seen.Contains(Id)) { Error = TEXT("Invalid, duplicate or conflicting unused connector ID."); return false; }
            Seen.Add(Id);
        }
    }
    return true;
}

bool HBDungeonRuntimeValidation::ValidateLocalPortalInstances(const TArray<FHBPrototypeRoom>& Rooms,
    const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams, FString& Error)
{
    if (!ValidateLayoutForLoading(Rooms, Error)) return false;
    if (Streams.Num() != Rooms.Num()) { Error = TEXT("Missing local room streams."); return false; }
    for (const auto& Stream : Streams)
        if (!IsValid(Stream) || !Stream->IsLevelLoaded() || !Stream->IsLevelVisible())
        { Error = TEXT("Local room is not loaded and visible."); return false; }
    for (const auto& R : Rooms)
    {
        if (R.ParentId == INDEX_NONE || R.Connection != EHBDungeonConnection::Portal) continue;
        if (!AHBDungeonPortal::FindInLevel(Streams[R.ParentId]->GetLoadedLevel(), R.ParentPortalId, Error) ||
            !AHBDungeonPortal::FindInLevel(Streams[R.Id]->GetLoadedLevel(), R.EntryPortalId, Error)) return false;
    }
    return true;
}
bool HBDungeonRuntimeValidation::LinkPortals(const TArray<FHBPrototypeRoom>& Rooms,
    const TArray<FHBModuleCatalogEntry>& ModuleCatalog, const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams,
    TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error)
{
	if (!ValidateLocalPortalInstances(Rooms, Streams, Error)) return false;
	if (Streams.Num() != Rooms.Num()) { Error = TEXT("Missing streamed room instances for portal binding."); return false; }
	auto FindPortal = [&](int32 RoomId, FName PortalId) -> AHBDungeonPortal*
	{
		ULevel* Level = Streams[RoomId]->GetLoadedLevel();
		AHBDungeonPortal* Found = AHBDungeonPortal::FindInLevel(Level, PortalId, Error);
		if (!Found) return nullptr;
		const auto* M = ModuleCatalog.FindByPredicate([&](const auto& C) { return C.ModuleId == Rooms[RoomId].ModuleId; });
		const auto* Definition = M ? M->Portals.FindByPredicate([&](const auto& P) { return P.PortalId == PortalId; }) : nullptr;
		if (!Definition || !Found->Trigger->GetComponentTransform().Equals(Definition->TriggerTransform * Rooms[RoomId].Transform, 0.1)
			|| !Found->Trigger->GetUnscaledBoxExtent().Equals(Definition->TriggerExtent, 0.1)
			|| !Found->Arrival->GetComponentTransform().Equals(Definition->ArrivalTransform * Rooms[RoomId].Transform, 0.1)) return nullptr;
		return Found;
	};
	for (const auto& R : Rooms)
	{
		if (R.Connection != EHBDungeonConnection::Portal) continue;
		AHBDungeonPortal* Source = FindPortal(R.ParentId, R.ParentPortalId);
		AHBDungeonPortal* Target = FindPortal(R.Id, R.EntryPortalId);
		if (!Source || !Target || !Source->ConnectTo(Target))
		{
			Error = FString::Printf(TEXT("Portal binding failed: R%d/%s <-> R%d/%s. Check unique placed actors and unchanged trigger/arrival transforms."),
				R.ParentId, *R.ParentPortalId.ToString(), R.Id, *R.EntryPortalId.ToString());
			return false;
		}
		LinkedPortals.Add(Source); LinkedPortals.Add(Target);
	}
	return ValidatePortalLinks(Rooms, Streams, LinkedPortals, Error);
}

bool HBDungeonRuntimeValidation::ValidatePortalLinks(const TArray<FHBPrototypeRoom>& Rooms,
    const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams,
    const TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error)
{
	int32 Pair = 0;
	for (const auto& R : Rooms)
	{
		if (R.Connection != EHBDungeonConnection::Portal) continue;
		if (!LinkedPortals.IsValidIndex(Pair + 1) || !Streams.IsValidIndex(R.Id) || !Streams.IsValidIndex(R.ParentId))
		{ Error = TEXT("Missing runtime portal pair."); return false; }
		const auto* A = LinkedPortals[Pair].Get(); const auto* B = LinkedPortals[Pair + 1].Get();
		if (!IsValid(A) || !IsValid(B) || A->TargetPortal != B || B->TargetPortal != A
			|| A->PortalId != R.ParentPortalId || B->PortalId != R.EntryPortalId
			|| A->GetLevel() != Streams[R.ParentId]->GetLoadedLevel() || B->GetLevel() != Streams[R.Id]->GetLoadedLevel())
		{ Error = TEXT("Runtime portals must be reciprocally linked in the planned room instances."); return false; }
		Pair += 2;
	}
	if (Pair != LinkedPortals.Num()) { Error = TEXT("Unexpected runtime portal pair."); return false; }
	return true;
}

