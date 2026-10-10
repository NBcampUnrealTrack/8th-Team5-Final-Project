#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonPortal.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
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
        if (R.bIsStartRoom != (Index == 0)
            || (R.bIsStartRoom && (R.Role == EHBPrototypeRoomRole::Boss || R.bCorridor || R.bBranch || R.Floor != 1)))
        { Error = TEXT("Exactly the first non-boss room on floor 1 must be marked as the start room."); return false; }
        if (R.Id != Index || R.ModuleId.IsNone() || R.ModuleMap.IsNull() ||
            !FPackageName::IsValidLongPackageName(R.ModuleMap.ToSoftObjectPath().GetLongPackageName()) ||
            !R.Transform.IsValid() || !R.Transform.GetScale3D().Equals(FVector::OneVector, 0.0001) ||
            (Index == 0 ? R.ParentId != INDEX_NONE : R.ParentId < 0 || R.ParentId >= Index))
        { Error = TEXT("Layout needs contiguous room IDs, valid modules/transforms and preceding parent IDs."); return false; }
        if (R.Floor < 1 || R.Floor > 2)
        { Error = TEXT("Room floor must be 1 or 2."); return false; }
        if (R.Connection != EHBDungeonConnection::Door && R.Connection != EHBDungeonConnection::Portal
            && R.Connection != EHBDungeonConnection::FloorTransition)
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
        if (R.Connection == EHBDungeonConnection::FloorTransition)
        {
            if (Index == 0 || Rooms[R.ParentId].Floor != 1 || R.Floor != 2 || !Rooms[R.ParentId].bFloorExit
                || R.bBranch || R.bCorridor || !R.ParentPortalId.IsNone() || !R.EntryPortalId.IsNone()
                || !R.ParentConnectorId.IsNone() || !R.EntryConnectorId.IsNone())
            { Error = TEXT("Logical floor transitions require floor 1 to 2 without fixed portal or door IDs."); return false; }
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

bool HBDungeonRuntimeValidation::PreparePlayerStarts(const TArray<FHBPrototypeRoom>& Rooms,
    const TArray<ULevel*>& Levels, TArray<APlayerStart*>& OutStarts, FString& Error)
{
    OutStarts.Reset();
    if (!ValidateLayoutForLoading(Rooms, Error)) return false;
    if (Levels.Num() != Rooms.Num()) { Error = TEXT("Missing room levels for PlayerStart preparation."); return false; }
    UWorld* World = IsValid(Levels[0]) ? Levels[0]->GetWorld() : nullptr;
    if (!World || !World->IsGameWorld()) { Error = TEXT("PlayerStart preparation only operates on runtime room instances."); return false; }
    TSet<ULevel*> Seen;
    TArray<APlayerStart*> Remove;
    for (int32 Index = 0; Index < Levels.Num(); ++Index)
    {
        ULevel* Level = Levels[Index];
        if (!IsValid(Level) || Level->GetWorld() != World || Level == World->PersistentLevel || Seen.Contains(Level))
        { OutStarts.Reset(); Error = TEXT("PlayerStarts require distinct generated levels in the same game world."); return false; }
        Seen.Add(Level);
        for (AActor* Actor : Level->Actors)
            if (auto* Start = Cast<APlayerStart>(Actor); IsValid(Start) && !Start->IsActorBeingDestroyed() && Start->GetLevel() == Level)
            {
                if (Rooms[Index].bIsStartRoom) OutStarts.Add(Start);
                else Remove.Add(Start);
            }
    }
    if (OutStarts.IsEmpty()) { Error = TEXT("The actual start room needs at least one PlayerStart. No tag is required."); return false; }
    // Gather first: Destroy may change the level's actor array. Validate every level before removing anything.
    for (APlayerStart* Start : Remove)
        if (!Start->Destroy())
        { OutStarts.Reset(); Error = TEXT("Could not remove a PlayerStart from a non-start room instance."); return false; }
    OutStarts.Sort([](const APlayerStart& A, const APlayerStart& B) { return A.GetFName().LexicalLess(B.GetFName()); });
    return true;
}

APlayerStart* HBDungeonRuntimeValidation::SelectPlayerStart(const TArray<APlayerStart*>& Starts, int32 DungeonSeed,
    TFunctionRef<bool(const APlayerStart&)> IsUsable, FString& Error)
{
    Error.Reset();
    if (Starts.IsEmpty()) { Error = TEXT("The actual start room has no PlayerStart."); return nullptr; }
    UWorld* World = IsValid(Starts[0]) ? Starts[0]->GetWorld() : nullptr;
    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
    { Error = TEXT("PlayerStart selection requires the server game world."); return nullptr; }
    TArray<APlayerStart*> Candidates = Starts;
    for (const APlayerStart* Start : Candidates)
        if (!IsValid(Start) || Start->IsActorBeingDestroyed() || Start->GetWorld() != World || Start->GetLevel() != Starts[0]->GetLevel())
        { Error = TEXT("PlayerStart choices must belong to the same actual start room."); return nullptr; }
    Candidates.Sort([](const APlayerStart& A, const APlayerStart& B) { return A.GetFName().LexicalLess(B.GetFName()); });
    FRandomStream Random(static_cast<int32>(HashCombineFast(GetTypeHash(DungeonSeed), 0x48425354u)));
    for (int32 Index = Candidates.Num() - 1; Index > 0; --Index) Candidates.Swap(Index, Random.RandRange(0, Index));
    for (APlayerStart* Start : Candidates) if (IsUsable(*Start)) return Start;
    Error = TEXT("Every PlayerStart in the actual start room is blocked or has no floor for the gameplay character.");
    return nullptr;
}

bool HBDungeonRuntimeValidation::ValidateLocalPortalInstances(const TArray<FHBPrototypeRoom>& Rooms,
    const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams, FString& Error)
{
    if (!ValidateLayoutForLoading(Rooms, Error)) return false;
    // A logical edge is not a connected portal pair. Do not report a playable layout before materialization.
    if (Rooms.ContainsByPredicate([](const auto& R) { return R.Connection == EHBDungeonConnection::FloorTransition; }))
    { Error = TEXT("Random point portal pairs must be spawned and validated before preparing gameplay."); return false; }
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

