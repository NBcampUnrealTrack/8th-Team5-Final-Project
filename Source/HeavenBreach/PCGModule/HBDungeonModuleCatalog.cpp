#include "PCGModule/HBDungeonModuleCatalog.h"
#include "PCGModule/HBRoomInfoComponent.h"
#include "PCGModule/HBDungeonPortal.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"

bool HBDungeonModuleCatalog::Build(const TArray<TSoftObjectPtr<UWorld>>& Maps,
    TArray<FHBModuleCatalogEntry>& OutEntries, TArray<FString>& OutErrors)
{
    TArray<TSoftObjectPtr<UWorld>> RoomMaps, CorridorMaps;
    for (const auto& Map : Maps)
    {
        UWorld* World = Map.LoadSynchronous();
        bool bCorridor = false;
        if (World && World->PersistentLevel)
            for (AActor* Actor : World->PersistentLevel->Actors)
                if (auto* Definition = Cast<AHBDungeonModuleDefinition>(Actor))
                { bCorridor = Definition->ModuleKind == EHBDungeonModuleKind::Corridor; break; }
        (bCorridor ? CorridorMaps : RoomMaps).Add(Map);
    }
    return Build(RoomMaps, CorridorMaps, OutEntries, OutErrors);
}

bool HBDungeonModuleCatalog::Build(const TArray<TSoftObjectPtr<UWorld>>& RoomMaps,
    const TArray<TSoftObjectPtr<UWorld>>& CorridorMaps, TArray<FHBModuleCatalogEntry>& OutEntries, TArray<FString>& OutErrors)
{
    OutEntries.Reset(); OutErrors.Reset();
    TArray<FHBModuleCatalogEntry> Pending;
    TSet<FSoftObjectPath> Paths;
    TSet<FName> Ids;
    if (RoomMaps.IsEmpty()) { OutErrors.Add(TEXT("Room Modules is empty. Add source room maps.")); }
    auto Read = [&](const TArray<TSoftObjectPtr<UWorld>>& Maps, EHBDungeonModuleKind ExpectedKind)
    {
        for (int32 Index = 0; Index < Maps.Num(); ++Index)
        {
            const auto& Map = Maps[Index];
            const FString Label = FString::Printf(TEXT("%s[%d] %s"),
                ExpectedKind == EHBDungeonModuleKind::Room ? TEXT("Room Modules") : TEXT("Corridor Modules"), Index, *Map.ToString());
            auto Error = [&](const FString& Message) { OutErrors.Add(Label + TEXT(": ") + Message); };
            if (Map.IsNull()) { Error(TEXT("No level assigned.")); continue; }
            if (Paths.Contains(Map.ToSoftObjectPath())) { Error(TEXT("Duplicate map reference.")); continue; }
            Paths.Add(Map.ToSoftObjectPath());
            UWorld* World = Map.LoadSynchronous();
            if (!World || !World->PersistentLevel) { Error(TEXT("Could not load source level.")); continue; }
            if (World->GetWorldPartition() || !World->GetStreamingLevels().IsEmpty())
            {
                Error(TEXT("Use a self-contained non-World-Partition source map without streaming sublevels.")); continue;
            }
            ULevel* Level = World->PersistentLevel;
            // Asset-loaded maps do not register components. Refresh cached transforms from their authored hierarchy.
            for (AActor* Actor : Level->Actors)
            {
                if (!IsValid(Actor)) { continue; }
                TInlineComponentArray<USceneComponent*> Components(Actor);
                for (USceneComponent* Component : Components) { Component->UpdateComponentToWorld(); }
            }
            TArray<FHBDungeonModuleIssue> Issues;
            if (!AHBDungeonModuleDefinition::ValidateLevel(Level, Issues))
            {
                for (const auto& Issue : Issues) { Error(Issue.Code.ToString() + TEXT(": ") + Issue.Message); }
                continue;
            }
            AHBDungeonModuleDefinition* Definition = nullptr;
            for (AActor* Actor : Level->Actors)
                if (auto* Candidate = Cast<AHBDungeonModuleDefinition>(Actor)) { Definition = Candidate; break; }
            if (!Definition) { Error(TEXT("Missing module definition.")); continue; }
            if (Definition->ModuleKind != ExpectedKind) { Error(TEXT("ModuleKind does not match this array.")); continue; }
            if (Ids.Contains(Definition->ModuleId)) { Error(TEXT("Duplicate ModuleId: ") + Definition->ModuleId.ToString()); continue; }
            Ids.Add(Definition->ModuleId);
            FHBModuleCatalogEntry Entry;
            Entry.Map = Map; Entry.ModuleId = Definition->ModuleId; Entry.Kind = ExpectedKind;
            Entry.AllowedFloor = Definition->RoomInfo->AllowedFloor;
            Entry.bIsStartCandidate = Definition->RoomInfo->bIsStartCandidate;
            if (ExpectedKind == EHBDungeonModuleKind::Room)
                Entry.Role = Definition->FindComponentByClass<UHBRoomInfoComponent>()->RoomRole;
            if (!Definition->ReadModuleGeometry(FTransform::Identity, Entry.Connectors, Entry.Occupancy, Issues))
            {
                for (const auto& Issue : Issues) { Error(Issue.Message); }
                continue;
            }
            Entry.ConnectorCount = Entry.Connectors.Num();
            for (AActor* Actor : Level->Actors)
                if (const auto* Portal = Cast<AHBDungeonPortal>(Actor); IsValid(Portal))
                    Entry.Portals.Add({Portal->PortalId, Portal->Trigger->GetComponentTransform(),
                        Portal->Trigger->GetUnscaledBoxExtent(), Portal->Arrival->GetComponentTransform()});
            Entry.Portals.Sort([](const auto& A, const auto& B) { return A.PortalId.LexicalLess(B.PortalId); });
            for (const auto& Box : Entry.Occupancy)
                Entry.Bounds += FBox(-Box.Extent, Box.Extent).TransformBy(Box.LocalTransform);
            if (!Entry.Bounds.IsValid || Entry.Bounds.Min.ContainsNaN() || Entry.Bounds.Max.ContainsNaN())
            { Error(TEXT("Invalid extracted occupancy bounds.")); continue; }
            // Free-fly inspection only, NOT a guaranteed safe gameplay spawn.
            Entry.InspectionEntry = Entry.Connectors[0].LocalTransform.TransformPosition(FVector(-100,0,100));
            for (AActor* Actor : Level->Actors)
                if (IsValid(Actor) && Actor->IsA<APlayerStart>()) { Entry.InspectionEntry = Actor->GetActorLocation() + FVector(0,0,64); break; }
            Pending.Add(MoveTemp(Entry));
        }
    };
    Read(RoomMaps, EHBDungeonModuleKind::Room);
    Read(CorridorMaps, EHBDungeonModuleKind::Corridor);
    if (!OutErrors.IsEmpty()) { return false; }
    OutEntries = MoveTemp(Pending);
    return true;
}

bool HBDungeonModuleCatalog::Generate(const TArray<FHBModuleCatalogEntry>& Catalog,
    const HBDungeonLayout::FSettings& Settings, TArray<FHBPrototypeRoom>& OutRooms, FString& OutError)
{
    OutRooms.Reset(); OutError.Reset();
    TArray<int32> ByRole[4];
    for (int32 Index = 0; Index < Catalog.Num(); ++Index)
    {
        const auto& Entry = Catalog[Index];
        if (Entry.Kind != EHBDungeonModuleKind::Room) { continue; }
        const uint8 Role = static_cast<uint8>(Entry.Role);
        if (Role > 3 || Entry.Map.IsNull() || Entry.ModuleId.IsNone() || !Entry.Bounds.IsValid)
        { OutError = TEXT("Invalid room catalog entry."); return false; }
        ByRole[Role].Add(Index);
    }
    for (int32 Role = 0; Role < 4; ++Role)
    {
        if (ByRole[Role].IsEmpty())
        {
            OutError = FString::Printf(TEXT("The current full-dungeon preview requires a %s module. Add a map with that RoomRole."),
                HBDungeonLayout::RoleName(static_cast<EHBPrototypeRoomRole>(Role)));
            return false;
        }
    }
    // Generate the logical graph first, then validate the bounds of the ACTUALLY selected modules.
    HBDungeonLayout::FSettings Logical = Settings;
    Logical.AdventureBounds = Logical.BossBounds = FBox(FVector(-1), FVector(1));
    FRandomStream Random(Settings.Seed);
    for (int32 Attempt = 0; Attempt < Settings.MaxAttempts; ++Attempt)
    {
        Logical.Seed = Attempt == 0 ? Settings.Seed : static_cast<int32>(Random.GetUnsignedInt() & 0x7fffffff);
        TArray<FHBPrototypeRoom> Rooms;
        if (!HBDungeonLayout::Generate(Logical, Rooms, OutError)) { return false; }
        for (auto& Room : Rooms)
        {
            const auto& Candidates = ByRole[static_cast<uint8>(Room.Role)];
            const auto& Entry = Catalog[Candidates[Random.RandRange(0, Candidates.Num()-1)]];
            Room.ModuleId = Entry.ModuleId;
            Room.ModuleMap = Entry.Map;
            Room.LocalBounds = Entry.Bounds;
            Room.InspectionEntry = Entry.InspectionEntry;
            Room.Bounds = Entry.Bounds.TransformBy(Room.Transform);
        }
        if (HBDungeonLayout::Validate(Rooms, OutError)) { OutRooms = MoveTemp(Rooms); return true; }
    }
    OutError = FString::Printf(TEXT("Selected module bounds could not fit after %d attempts. Adjust spacing/occupancy. %s"), Settings.MaxAttempts, *OutError);
    return false;
}
