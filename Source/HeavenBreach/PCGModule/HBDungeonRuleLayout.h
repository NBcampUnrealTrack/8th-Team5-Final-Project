#pragma once

#include "PCGModule/HBConnectorLayout.h"
#include "PCGModule/HBDungeonGenerationRules.h"

/** Two logical tiers joined by authored portals; physical door placement remains connector based. */
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
}
