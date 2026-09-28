#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonPortal.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/LevelStreamingDynamic.h"
bool HBDungeonRuntimeValidation::LinkPortals(const TArray<FHBPrototypeRoom>& Rooms,
    const TArray<FHBModuleCatalogEntry>& ModuleCatalog, const TArray<TObjectPtr<ULevelStreamingDynamic>>& Streams,
    TArray<TObjectPtr<AHBDungeonPortal>>& LinkedPortals, FString& Error)
{
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

