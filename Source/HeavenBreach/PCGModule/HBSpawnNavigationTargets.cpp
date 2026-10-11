#include "PCGModule/HBSpawnNavigationTargets.h"
#include "PCGModule/HBSpawnNavigationAnchor.h"
#include "Engine/Level.h"

bool HBSpawnNavigationTargets::Collect(const ULevel& Level, const FBox& RoomBounds,
	TMap<FName, FVector>& OutAnchors, FString& OutError)
{
	OutAnchors.Reset();
	OutError.Reset();
	for (AActor* Actor : Level.Actors)
	{
		const auto* Anchor = Cast<AHBSpawnNavigationAnchor>(Actor);
		if (!IsValid(Anchor) || Anchor->IsActorBeingDestroyed() || Anchor->GetLevel() != &Level) continue;
		const FVector Location = Anchor->GetActorLocation();
		if (Anchor->AnchorId.IsNone() || OutAnchors.Contains(Anchor->AnchorId))
		{
			OutError = FString::Printf(TEXT("Navigation anchor IDs must be nonempty and unique within the room: %s (%s)."),
				*Anchor->AnchorId.ToString(), *Anchor->GetPathName());
			return false;
		}
		if (Location.ContainsNaN() || !RoomBounds.IsInsideOrOn(Location))
		{
			OutError = FString::Printf(TEXT("Navigation anchor %s must be finite and inside its room bounds."), *Anchor->AnchorId.ToString());
			return false;
		}
		OutAnchors.Add(Anchor->AnchorId, Location);
	}
	return true;
}

bool HBSpawnNavigationTargets::Resolve(FName AnchorId, const TMap<FName, FVector>& Anchors,
	const TArray<FHBRoomSpawnEntry>& Entries, TArray<FHBSpawnNavigationTarget>& OutTargets, FString& OutError)
{
	OutTargets.Reset();
	OutError.Reset();
	if (!AnchorId.IsNone())
	{
		const FVector* Location = Anchors.Find(AnchorId);
		if (!Location)
		{
			OutError = FString::Printf(TEXT("Navigation anchor %s was not found in this room instance."), *AnchorId.ToString());
			return false;
		}
		FHBSpawnNavigationTarget& Target = OutTargets.AddDefaulted_GetRef();
		Target.Location = *Location;
		Target.AnchorId = AnchorId;
		return true;
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (!Entries[Index].bUseForNavigation) continue;
		FHBSpawnNavigationTarget& Target = OutTargets.AddDefaulted_GetRef();
		Target.Location = Entries[Index].NavigationLocation;
		Target.EntryIndex = Index;
		Target.bHasConnectionLocation = true;
		Target.ConnectionLocation = Entries[Index].ConnectionLocation;
	}
	return true;
}
