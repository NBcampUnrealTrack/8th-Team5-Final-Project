#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBDungeonSnapshot.h"
#include "PCGModule/HBPortalSpawnPoint.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Misc/PackageName.h"

bool HBDungeonRuntimeValidation::ValidatePortalSnapshot(const FHBDungeonSnapshot& Snapshot, FString& Error)
{
    Error.Reset();
    const bool bLogicalTransition = Snapshot.Rooms.ContainsByPredicate([](const auto& R)
        { return R.Connection == EHBDungeonConnection::FloorTransition; });
    if (Snapshot.PortalPlacementMode == EHBDungeonPortalPlacementMode::FixedRoomPortals)
    {
        if (bLogicalTransition || Snapshot.PortalPairCount != 0 || !Snapshot.PortalClass.IsNull()
            || Snapshot.bPortalPairsReady || !Snapshot.PortalPairs.IsEmpty())
        { Error = TEXT("Fixed portal snapshots cannot contain random placement data."); return false; }
        return true;
    }
    if (Snapshot.PortalPlacementMode != EHBDungeonPortalPlacementMode::RandomPoints || !bLogicalTransition
        || Snapshot.PortalPairCount < 1 || Snapshot.PortalClass.IsNull())
    { Error = TEXT("Random portal snapshots require a logical floor transition, class and positive pair count."); return false; }
    const UClass* Class = Snapshot.PortalClass.LoadSynchronous();
    if (!Class || Class->HasAnyClassFlags(CLASS_Abstract) || Class->GetDefaultObject<AHBDungeonPortal>()->GetIsReplicated())
    { Error = TEXT("Portal snapshot class must be concrete and non-replicated."); return false; }
    if (!Snapshot.bPortalPairsReady)
    {
        if (!Snapshot.PortalPairs.IsEmpty()) { Error = TEXT("Pending portal snapshots cannot contain partial pairs."); return false; }
        return true;
    }
    if (Snapshot.PortalPairs.Num() != Snapshot.PortalPairCount)
    { Error = TEXT("Final portal snapshot does not contain exactly the requested pairs."); return false; }
    TSet<HBPortalPlacement::FPointKey> Seen;
    TArray<HBPortalPlacement::FPlacement> Reserved;
    for (int32 Index = 0; Index < Snapshot.PortalPairs.Num(); ++Index)
    {
        const auto& Pair = Snapshot.PortalPairs[Index];
        if (Pair.PairId != Index) { Error = TEXT("Invalid portal snapshot pair ID."); return false; }
        const FHBDungeonPortalEndpointSnapshot* Ends[] = {&Pair.Floor1, &Pair.Floor2};
        for (int32 FloorIndex = 0; FloorIndex < 2; ++FloorIndex)
        {
            const auto& End = *Ends[FloorIndex];
            const HBPortalPlacement::FPointKey Key{End.RoomId, End.PointId};
            const HBPortalPlacement::FPlacement Space{End.ActivationLocation, End.ActivationBounds, End.ArrivalBounds};
            auto Unit = [](const FTransform& T) { return T.IsValid() && T.GetScale3D().Equals(FVector::OneVector, 0.0001); };
            if (!Snapshot.Rooms.IsValidIndex(End.RoomId) || Snapshot.Rooms[End.RoomId].Floor != FloorIndex + 1
                || End.PointId.IsNone() || Seen.Contains(Key) || !Unit(End.ActorTransform) || !Unit(End.ArrivalTransform)
                || !End.TriggerTransform.IsValid() || End.TriggerTransform.GetScale3D().GetMin() <= 0
                || End.TriggerExtent.ContainsNaN() || End.TriggerExtent.GetMin() <= 0 || !Space.IsValid())
            { Error = TEXT("Portal snapshot endpoint is reused, wrong-floor or has invalid transforms/bounds."); return false; }
            const FVector Activation = End.TriggerTransform.InverseTransformPosition(End.ActivationLocation).GetAbs();
            const FVector Arrival = End.TriggerTransform.InverseTransformPosition(End.ArrivalTransform.GetLocation()).GetAbs();
            const FBox TriggerBounds = FBox(-End.TriggerExtent, End.TriggerExtent).TransformBy(End.TriggerTransform);
            if (Activation.X > End.TriggerExtent.X || Activation.Y > End.TriggerExtent.Y || Activation.Z > End.TriggerExtent.Z
                || (Arrival.X <= End.TriggerExtent.X && Arrival.Y <= End.TriggerExtent.Y && Arrival.Z <= End.TriggerExtent.Z)
                || !End.ActivationBounds.IsInsideOrOn(TriggerBounds.Min) || !End.ActivationBounds.IsInsideOrOn(TriggerBounds.Max)
                || !End.ArrivalBounds.IsInsideOrOn(End.ArrivalTransform.GetLocation()))
            { Error = TEXT("Portal snapshot activation, arrival and reservations do not match its trigger."); return false; }
            for (const auto& Other : Reserved)
                if (Space.Intersects(Other)) { Error = TEXT("Portal snapshot reservations overlap."); return false; }
            Seen.Add(Key); Reserved.Add(Space);
        }
    }
    return true;
}

bool HBDungeonRuntimeValidation::CapturePortalPairs(const TArray<HBPortalPlacement::FSpawnedPair>& Pairs,
    TArray<FHBDungeonPortalPairSnapshot>& OutPairs, FString& Error)
{
    OutPairs.Reset(); Error.Reset();
    TArray<FHBDungeonPortalPairSnapshot> Captured;
    for (const auto& Pair : Pairs)
    {
        FHBDungeonPortalPairSnapshot Record;
        Record.PairId = Pair.Selection.PairId;
        auto Capture = [&](AHBDungeonPortal* Portal, const HBPortalPlacement::FPointKey& Key,
            const HBPortalPlacement::FPlacement& Space, FHBDungeonPortalEndpointSnapshot& End)
        {
            if (!IsValid(Portal) || Portal->IsActorBeingDestroyed() || !Portal->ValidateDefinition(Error) || !Space.IsValid()) return false;
            End.RoomId = Key.RoomId; End.PointId = Key.PointId;
            End.ActorTransform = Portal->GetActorTransform();
            End.TriggerTransform = Portal->Trigger->GetComponentTransform();
            End.TriggerExtent = Portal->Trigger->GetUnscaledBoxExtent();
            End.ArrivalTransform = Portal->Arrival->GetComponentTransform();
            End.ActivationLocation = Space.ActivationLocation;
            End.ActivationBounds = Space.ActivationBounds; End.ArrivalBounds = Space.ArrivalBounds;
            return true;
        };
        if (!Capture(Pair.Floor1.Get(), Pair.Selection.Floor1, Pair.Placement1, Record.Floor1)
            || !Capture(Pair.Floor2.Get(), Pair.Selection.Floor2, Pair.Placement2, Record.Floor2)) return false;
        Captured.Add(Record);
    }
    OutPairs = MoveTemp(Captured);
    return true;
}

bool HBDungeonRuntimeValidation::RestorePortalPairs(UWorld& World, AActor* Owner, TSubclassOf<AHBDungeonPortal> PortalClass,
    const FHBDungeonSnapshot& Snapshot, const TArray<ULevel*>& Levels,
    TArray<HBPortalPlacement::FSpawnedPair>& OutPairs, FString& Error)
{
    Error.Reset();
    if (World.GetNetMode() != NM_Client || !OutPairs.IsEmpty() || !Snapshot.bPortalPairsReady
        || !PortalClass || PortalClass->HasAnyClassFlags(CLASS_Abstract) || PortalClass.GetDefaultObject()->GetIsReplicated()
        || FSoftObjectPath(PortalClass.Get()) != Snapshot.PortalClass.ToSoftObjectPath()
        || Levels.Num() != Snapshot.Rooms.Num())
    { Error = TEXT("Client portal restoration requires a final snapshot, matching non-replicated class and empty output."); return false; }
    if (!ValidatePortalSnapshot(Snapshot, Error)) return false;
    for (ULevel* Level : Levels)
        if (!IsValid(Level) || Level->GetWorld() != &World) { Error = TEXT("Missing client room for portal restoration."); return false; }
    TArray<HBPortalPlacement::FSpawnedPair> Prepared;
    TArray<AHBDungeonPortal*> Created;
    auto Abort = [&]()
    {
        for (AHBDungeonPortal* Portal : Created) if (IsValid(Portal)) { Portal->Disconnect(); Portal->Destroy(); }
        return false;
    };
    auto Restore = [&](const FHBDungeonPortalEndpointSnapshot& End) -> AHBDungeonPortal*
    {
        ULevel* Level = Levels[End.RoomId];
        AHBPortalSpawnPoint* Marker = nullptr;
        for (AActor* Actor : Level->Actors)
            if (auto* Point = Cast<AHBPortalSpawnPoint>(Actor); IsValid(Point) && !Point->IsActorBeingDestroyed() && Point->PointId == End.PointId)
            {
                if (Marker) { Error = TEXT("Duplicate selected portal point in a client room."); return nullptr; }
                Marker = Point;
            }
        if (!Marker || !Marker->ValidateDefinition(Error)
            || FVector::Dist2D(Marker->Direction->GetComponentLocation(), End.ActorTransform.GetLocation()) > 0.1
            || !Marker->Direction->GetComponentQuat().Equals(End.ActorTransform.GetRotation(), 0.0001))
        { Error = FString::Printf(TEXT("Client portal snapshot does not match Room=%d Point=%s."), End.RoomId, *End.PointId.ToString()); return nullptr; }
        FActorSpawnParameters Params;
        Params.Owner = Owner; Params.OverrideLevel = Level; Params.bDeferConstruction = true;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Portal = World.SpawnActor<AHBDungeonPortal>(PortalClass, End.ActorTransform, Params);
        if (!IsValid(Portal)) { Error = TEXT("Could not spawn a client portal."); return nullptr; }
        Created.Add(Portal);
        Portal->SetTravelEnabled(false); Portal->SetActorEnableCollision(false);
        Portal->FinishSpawning(End.ActorTransform);
        if (!IsValid(Portal) || Portal->IsActorBeingDestroyed() || Portal->GetIsReplicated()
            || !IsValid(Portal->Trigger) || !IsValid(Portal->Arrival))
        { Error = TEXT("Client portal construction produced an invalid actor."); return nullptr; }
        Portal->SetTravelEnabled(false); Portal->SetActorEnableCollision(false);
        Portal->PortalId = FName(*FString::Printf(TEXT("PCG_R%d_%s"), End.RoomId, *End.PointId.ToString()));
        Portal->SetActorTransform(End.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Portal->Trigger->SetBoxExtent(End.TriggerExtent);
        Portal->Trigger->SetWorldTransform(End.TriggerTransform);
        Portal->Arrival->SetWorldTransform(End.ArrivalTransform);
        TInlineComponentArray<UPrimitiveComponent*> Components(Portal);
        for (UPrimitiveComponent* Component : Components)
        { Component->SetCollisionEnabled(ECollisionEnabled::NoCollision); Component->SetCanEverAffectNavigation(false); }
        if (!Portal->ValidateDefinition(Error) || Portal->GetLevel() != Level
            || !Portal->GetActorTransform().Equals(End.ActorTransform, 0.1)
            || !Portal->Trigger->GetComponentTransform().Equals(End.TriggerTransform, 0.1)
            || !Portal->Arrival->GetComponentTransform().Equals(End.ArrivalTransform, 0.1)) return nullptr;
        return Portal;
    };
    for (const auto& Pair : Snapshot.PortalPairs)
    {
        AHBDungeonPortal* A = Restore(Pair.Floor1);
        if (!A) return Abort();
        AHBDungeonPortal* B = Restore(Pair.Floor2);
        if (!B) return Abort();
        // These local visual links reflect authority data. Client actors never execute teleportation.
        A->TargetPortal = B; B->TargetPortal = A;
        Prepared.Add({{Pair.PairId, {Pair.Floor1.RoomId, Pair.Floor1.PointId}, {Pair.Floor2.RoomId, Pair.Floor2.PointId}}, A, B,
            {Pair.Floor1.ActivationLocation, Pair.Floor1.ActivationBounds, Pair.Floor1.ArrivalBounds},
            {Pair.Floor2.ActivationLocation, Pair.Floor2.ActivationBounds, Pair.Floor2.ArrivalBounds}});
    }
    if (!ValidateRandomPortalLinks(Snapshot.Rooms, Levels, Snapshot.PortalPairCount, Prepared, Error)) return Abort();
    OutPairs = MoveTemp(Prepared);
    return true;
}

bool HBDungeonRuntimeValidation::ValidatePortalCandidates(const TArray<FHBPrototypeRoom>& Rooms,
    const TArray<ULevel*>& Levels, const TArray<HBPortalPlacement::FCandidate>& Candidates, FString& Error)
{
    Error.Reset();
    if (Levels.Num() != Rooms.Num()) { Error = TEXT("Missing room levels for portal point validation."); return false; }
    TMap<HBPortalPlacement::FPointKey, const HBPortalPlacement::FCandidate*> Expected;
    for (const auto& Candidate : Candidates)
    {
        if (!Rooms.IsValidIndex(Candidate.Key.RoomId) || Expected.Contains(Candidate.Key)
            || Candidate.Floor != Rooms[Candidate.Key.RoomId].Floor)
        { Error = TEXT("Invalid or duplicate planned portal point."); return false; }
        Expected.Add(Candidate.Key, &Candidate);
    }
    TSet<HBPortalPlacement::FPointKey> Seen;
    for (int32 RoomId = 0; RoomId < Rooms.Num(); ++RoomId)
    {
        ULevel* Level = Levels[RoomId];
        if (!IsValid(Level)) { Error = TEXT("Missing loaded portal point room."); return false; }
        for (AActor* Actor : Level->Actors)
            if (const auto* Point = Cast<AHBPortalSpawnPoint>(Actor); IsValid(Point) && !Point->IsActorBeingDestroyed() && Point->GetLevel() == Level)
            {
                const HBPortalPlacement::FPointKey Key{RoomId, Point->PointId};
                const auto* Planned = Expected.Find(Key);
                if (!Point->ValidateDefinition(Error) || Seen.Contains(Key) || !Planned
                    || !Point->Direction->GetComponentTransform().Equals((*Planned)->SpawnTransform, 0.1))
                {
                    Error = FString::Printf(TEXT("Room=%d Point=%s does not match the catalog portal point: %s"),
                        RoomId, *Point->PointId.ToString(), *Error);
                    return false;
                }
                Seen.Add(Key);
            }
    }
    if (Seen.Num() != Expected.Num()) { Error = TEXT("A planned portal point is missing from its loaded room instance."); return false; }
    return true;
}

bool HBDungeonRuntimeValidation::PreparePortalSpace(UWorld& World, AHBDungeonPortal& Portal,
    const FVector& GroundPoint, const ACharacter& PlayerDefaults, const FBox& RoomBounds,
    HBPortalPlacement::FPlacement& OutPlacement, FString& Error)
{
    OutPlacement = {};
    Error.Reset();
    if (!Portal.ValidateDefinition(Error)) return false;
    const UCapsuleComponent* Capsule = PlayerDefaults.GetCapsuleComponent();
    const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;
    const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
    if (GroundPoint.ContainsNaN() || !RoomBounds.IsValid || !RoomBounds.IsInsideOrOn(GroundPoint)
        || !FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || Radius <= 0 || HalfHeight < Radius)
    { Error = TEXT("Portal grounding requires a floor marker inside the room and a valid player capsule."); return false; }

    constexpr double Clearance = 2.;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HBPortalGrounding), false);
    Query.AddIgnoredActor(&Portal);
    FHitResult TriggerFloor;
    // Stay close to the authored floor; never snap to another storey or search sideways.
    if (!World.LineTraceSingleByChannel(TriggerFloor, GroundPoint + FVector(0, 0, 20),
        GroundPoint - FVector(0, 0, 100), ECC_WorldStatic, Query) || TriggerFloor.ImpactNormal.Z < 0.7f)
    { Error = TEXT("Portal floor marker has no nearby walkable physical floor."); return false; }

    const FVector OriginalArrival = Portal.Arrival->GetComponentLocation();
    const FVector ArrivalGround(OriginalArrival.X, OriginalArrival.Y, TriggerFloor.ImpactPoint.Z);
    FHitResult ArrivalFloor;
    if (!World.LineTraceSingleByChannel(ArrivalFloor, ArrivalGround + FVector(0, 0, 100),
        ArrivalGround - FVector(0, 0, 100), ECC_WorldStatic, Query) || ArrivalFloor.ImpactNormal.Z < 0.7f)
    { Error = TEXT("Portal arrival has no nearby walkable physical floor at its Blueprint XY offset."); return false; }

    // Use the constructed BP's actual trigger bounds, including component offsets, rotation and positive scale.
    const double RaiseBy = TriggerFloor.ImpactPoint.Z + Clearance - Portal.Trigger->Bounds.GetBox().Min.Z;
    if (!FMath::IsFinite(RaiseBy)
        || !Portal.SetActorLocation(Portal.GetActorLocation() + FVector(0, 0, RaiseBy), false, nullptr, ETeleportType::TeleportPhysics))
    { Error = TEXT("Could not align the portal trigger above its floor marker."); return false; }
    Portal.Arrival->SetWorldLocation(FVector(OriginalArrival.X, OriginalArrival.Y,
        ArrivalFloor.ImpactPoint.Z + HalfHeight + Clearance), false, nullptr, ETeleportType::TeleportPhysics);
    return ValidatePortalSpace(World, Portal, PlayerDefaults, RoomBounds, OutPlacement, Error);
}

bool HBDungeonRuntimeValidation::ValidatePortalSpace(UWorld& World, const AHBDungeonPortal& Portal,
    const ACharacter& PlayerDefaults, const FBox& RoomBounds, HBPortalPlacement::FPlacement& OutPlacement, FString& Error)
{
    OutPlacement = {};
    Error.Reset();
    if (!Portal.ValidateDefinition(Error)) return false;
    const UCapsuleComponent* Capsule = PlayerDefaults.GetCapsuleComponent();
    const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;
    const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
    if (!RoomBounds.IsValid || !FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || Radius <= 0 || HalfHeight < Radius)
    { Error = TEXT("Portal safety requires finite room bounds and a valid player capsule."); return false; }
    const FVector Extent(Radius, Radius, HalfHeight);
    FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(HBPortalFloor), false);
    FloorQuery.AddIgnoredActor(&Portal);
    const FVector TriggerCenter = Portal.Trigger->GetComponentLocation();
    const FVector TriggerFoot(TriggerCenter.X, TriggerCenter.Y, Portal.Trigger->Bounds.GetBox().Min.Z);
    FHitResult ActivationFloor;
    // Probe at the grounded bottom, so an obstruction higher inside the trigger cannot masquerade as its floor.
    if (!World.LineTraceSingleByChannel(ActivationFloor, TriggerFoot + FVector(0, 0, 20),
        TriggerFoot - FVector(0, 0, 100), ECC_WorldStatic, FloorQuery) || ActivationFloor.ImpactNormal.Z < 0.7f)
    { Error = TEXT("Portal trigger has no walkable physical floor at its authored position."); return false; }
    const FVector Activation = ActivationFloor.ImpactPoint + FVector(0, 0, HalfHeight + 2.f);
    if (!Portal.Contains(Activation))
    { Error = TEXT("A standing player's origin does not fit in the portal trigger."); return false; }
    const FVector Arrival = Portal.Arrival->GetComponentLocation();
    FHitResult ArrivalFloor;
    const FVector Feet = Arrival - FVector(0, 0, HalfHeight);
    FCollisionQueryParams CapsuleQuery(SCENE_QUERY_STAT(HBPortalCapsule), false);
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
    for (const FVector& Location : {Activation, Arrival})
    {
        if (!RoomBounds.IsInsideOrOn(Location - Extent) || !RoomBounds.IsInsideOrOn(Location + Extent))
        { Error = TEXT("Portal trigger or Arrival player capsule extends outside its room bounds."); return false; }
        if (World.OverlapBlockingTestByProfile(Location, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, CapsuleQuery))
        {
            TArray<FOverlapResult> Overlaps;
            World.OverlapMultiByProfile(Overlaps, Location, FQuat::Identity, Capsule->GetCollisionProfileName(), Shape, CapsuleQuery);
            TArray<FString> Blockers;
            for (const FOverlapResult& Overlap : Overlaps)
                if (Overlap.bBlockingHit && Blockers.Num() < 4)
                    Blockers.Add(FString::Printf(TEXT("%s/%s"), *GetNameSafe(Overlap.GetActor()), *GetNameSafe(Overlap.GetComponent())));
            Error = FString::Printf(TEXT("Portal %s player capsule overlaps blocking geometry. Location=%s Radius=%.1f HalfHeight=%.1f Blockers=[%s]"),
                Location.Equals(Activation) ? TEXT("trigger") : TEXT("authored Arrival"), *Location.ToString(), Radius, HalfHeight,
                *FString::Join(Blockers, TEXT(", ")));
            return false;
        }
    }
    if (!World.LineTraceSingleByChannel(ArrivalFloor, Feet + FVector(0, 0, 20), Feet - FVector(0, 0, 100),
        ECC_WorldStatic, FloorQuery) || ArrivalFloor.ImpactNormal.Z < 0.7f
        || Feet.Z < ArrivalFloor.ImpactPoint.Z - 0.5f || Feet.Z - ArrivalFloor.ImpactPoint.Z > 100.f)
    { Error = TEXT("Authored portal Arrival has no nearby supporting walkable floor for the player capsule."); return false; }
    OutPlacement.ActivationLocation = Activation;
    OutPlacement.ActivationBounds = Portal.Trigger->Bounds.GetBox().ExpandBy(Extent + FVector(2));
    OutPlacement.ArrivalBounds = FBox(Arrival - Extent - FVector(2), Arrival + Extent + FVector(2));
    return true;
}

bool HBDungeonRuntimeValidation::ValidateRandomPortalLinks(const TArray<FHBPrototypeRoom>& Rooms,
    const TArray<ULevel*>& Levels, int32 PairCount, const TArray<HBPortalPlacement::FSpawnedPair>& Pairs, FString& Error)
{
    Error.Reset();
    if (PairCount < 1 || Pairs.Num() != PairCount || Levels.Num() != Rooms.Num())
    { Error = TEXT("Random portal generation must contain exactly the requested number of complete pairs."); return false; }
    TSet<HBPortalPlacement::FPointKey> UsedPoints;
    TSet<const AHBDungeonPortal*> UsedActors;
    TArray<HBPortalPlacement::FPlacement> Reservations;
    for (int32 Index = 0; Index < Pairs.Num(); ++Index)
    {
        const auto& Pair = Pairs[Index];
        AHBDungeonPortal* Portals[2] = {Pair.Floor1.Get(), Pair.Floor2.Get()};
        const HBPortalPlacement::FPointKey Keys[2] = {Pair.Selection.Floor1, Pair.Selection.Floor2};
        const HBPortalPlacement::FPlacement Placements[2] = {Pair.Placement1, Pair.Placement2};
        if (Pair.Selection.PairId != Index) { Error = TEXT("Invalid generation-local portal pair ID."); return false; }
        for (int32 FloorIndex = 0; FloorIndex < 2; ++FloorIndex)
        {
            const auto& Key = Keys[FloorIndex];
            const AHBDungeonPortal* Portal = Portals[FloorIndex];
            if (!Rooms.IsValidIndex(Key.RoomId) || Rooms[Key.RoomId].Floor != FloorIndex + 1
                || Key.PointId.IsNone() || UsedPoints.Contains(Key) || !IsValid(Portal) || Portal->IsActorBeingDestroyed()
                || UsedActors.Contains(Portal) || !IsValid(Levels[Key.RoomId]) || Portal->GetLevel() != Levels[Key.RoomId]
                || Portal->GetIsReplicated() || Portal->TargetPortal != Portals[1 - FloorIndex]
                || !IsValid(Portals[1 - FloorIndex]) || !Portal->ValidateDefinition(Error) || !Placements[FloorIndex].IsValid())
            { Error = TEXT("Random portal pair has a missing, reused, wrong-floor or incorrectly linked endpoint."); return false; }
            for (const auto& Reserved : Reservations)
                if (Reserved.Intersects(Placements[FloorIndex]))
                { Error = TEXT("Random portal pairs overlap reserved activation or arrival space."); return false; }
            Reservations.Add(Placements[FloorIndex]);
            UsedPoints.Add(Key); UsedActors.Add(Portal);
        }
    }
    for (ULevel* Level : Levels)
        for (AActor* Actor : Level->Actors)
            if (const auto* Portal = Cast<AHBDungeonPortal>(Actor); IsValid(Portal) && !Portal->IsActorBeingDestroyed()
                && (!UsedActors.Contains(Portal) || Portal->GetLevel() != Level))
            { Error = TEXT("Random mode contains a portal outside its selected pairs."); return false; }
    return true;
}

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

