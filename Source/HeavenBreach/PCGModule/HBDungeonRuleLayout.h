#pragma once

#include "PCGModule/HBConnectorLayout.h"
#include "PCGModule/HBDungeonGenerationRules.h"
#include "PCGModule/HBPortalPlacement.h"

/** Two logical tiers; random portal pairs are selected separately from the room progression tree. */
namespace HBDungeonRuleLayout
{
	HEAVENBREACH_API bool ResolvePortalIds(const TArray<FHBModuleCatalogEntry>& Catalog,
		FHBDungeonGenerationRules& Rules, FString& OutError);
	HEAVENBREACH_API bool Generate(const TArray<FHBModuleCatalogEntry>& Catalog,
		const HBConnectorLayout::FSettings& Settings, const FHBDungeonGenerationRules& Rules,
		TArray<FHBPrototypeRoom>& OutRooms, FString& OutError);
	HEAVENBREACH_API bool Validate(const TArray<FHBModuleCatalogEntry>& Catalog,
		const HBConnectorLayout::FSettings& Settings, const FHBDungeonGenerationRules& Rules,
		const TArray<FHBPrototypeRoom>& Rooms, FString& OutError);
	/** Collect only selected room instances. Reused maps contribute separate room-local point identities. */
	HEAVENBREACH_API bool CollectPortalCandidates(const TArray<FHBModuleCatalogEntry>& Catalog,
		const TArray<FHBPrototypeRoom>& Rooms, TArray<HBPortalPlacement::FCandidate>& OutCandidates, FString& OutError);
	/** Authoring/planning capacity only; runtime navigation and collision checks may reject further candidates. */
	HEAVENBREACH_API bool ValidatePortalQuota(const TArray<FHBModuleCatalogEntry>& Catalog,
		const TArray<FHBPrototypeRoom>& Rooms, int32 PairCount, FString& OutError);
}
