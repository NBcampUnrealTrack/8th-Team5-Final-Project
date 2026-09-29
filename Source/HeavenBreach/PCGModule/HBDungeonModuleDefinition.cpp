#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBRoomInfoComponent.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "PCGModule/HBDungeonOccupancyComponent.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGModule/HBEncounterComponent.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "PCGModule/HBRoomSpawnExclusionVolume.h"
#include "AI/EnemyCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Engine/Level.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

namespace
{
	template<class T>
	TArray<T*> GatherComponents(ULevel* Level)
	{
		TArray<T*> Result;
		if (!Level) { return Result; }
		for (AActor* Actor : Level->Actors)
		{
			if (!IsValid(Actor)) { continue; }
			TInlineComponentArray<T*> Components(Actor);
			Result.Append(Components);
		}
		return Result;
	}

	bool IsUnitTransform(const FTransform& Transform)
	{
		return Transform.IsValid() && Transform.GetScale3D().Equals(FVector::OneVector, 0.0001);
	}

	bool HasPositiveExtent(const FVector& Extent)
	{
		return !Extent.ContainsNaN() && Extent.X > 0 && Extent.Y > 0 && Extent.Z > 0;
	}

	void AddIssue(TArray<FHBDungeonModuleIssue>& Issues, FName Code, UObject* Source, const FString& Message)
	{
		Issues.Add({ Code, Message, Source });
	}
}

AHBDungeonModuleDefinition::AHBDungeonModuleDefinition()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ModuleSettingsRoot"));
	RoomInfo = CreateDefaultSubobject<UHBRoomInfoComponent>(TEXT("RoomInfo"));
}

bool AHBDungeonModuleDefinition::ValidateLevel(ULevel* Level, TArray<FHBDungeonModuleIssue>& OutIssues)
{
	OutIssues.Reset();
	if (!IsValid(Level))
	{
		AddIssue(OutIssues, TEXT("MissingLevel"), nullptr, TEXT("No module level was supplied."));
		return false;
	}

	TArray<AHBDungeonModuleDefinition*> Definitions;
	for (AActor* Actor : Level->Actors)
	{
		if (!IsValid(Actor)) { continue; }
		if (auto* Definition = Cast<AHBDungeonModuleDefinition>(Actor)) { Definitions.Add(Definition); }

	}
	if (Definitions.Num() != 1)
	{
		AddIssue(OutIssues, TEXT("DefinitionCount"), Level,
			FString::Printf(TEXT("Expected exactly one module definition in this level; found %d."), Definitions.Num()));
	}

	for (AHBDungeonModuleDefinition* Definition : Definitions)
	{
		AHBEncounterSetupActor* Setup = nullptr;
		int32 SetupCount = 0;
		for (AActor* Actor : Level->Actors)
			if (auto* Candidate = Cast<AHBEncounterSetupActor>(Actor); IsValid(Candidate))
			{
				Setup = Candidate;
				++SetupCount;
			}
		if (SetupCount > 1)
			AddIssue(OutIssues, TEXT("EncounterSetupCount"), Level, TEXT("Place at most one Encounter Setup actor in a room."));
		if (Setup)
		{
			if (Definition->ModuleKind != EHBDungeonModuleKind::Room)
				AddIssue(OutIssues, TEXT("CorridorEncounter"), Setup, TEXT("Encounter setups belong to rooms, not corridors."));
			if (!IsValid(Setup->Encounter))
				AddIssue(OutIssues, TEXT("EncounterComponent"), Setup, TEXT("Encounter Setup needs its settings component."));
			else
			{
				TArray<FString> ConfigErrors;
				Setup->Encounter->Validate(ConfigErrors);
				for (const FString& ConfigError : ConfigErrors)
					AddIssue(OutIssues, TEXT("EncounterComponent"), Setup->Encounter, ConfigError);
			}
		}
		TSet<FName> SpawnPointIds;
		int32 SpawnPointCount = 0;
		for (AActor* Actor : Level->Actors)
		{
			if (const auto* Point = Cast<AHBEnemySpawnPoint>(Actor); IsValid(Point))
			{
				++SpawnPointCount;
				if (Point->PointId.IsNone() || SpawnPointIds.Contains(Point->PointId))
				{
					AddIssue(OutIssues, TEXT("EnemySpawnPointId"), Actor, TEXT("PointId must be nonempty and unique within the room map."));
				}
				SpawnPointIds.Add(Point->PointId);
				if (!IsUnitTransform(Point->GetActorTransform()))
				{
					AddIssue(OutIssues, TEXT("EnemySpawnPointTransform"), Actor, TEXT("Spawn point position and rotation must be finite, with scale (1,1,1)."));
				}
				if (!Point->bAllowCommon && !Point->bAllowElite && !Point->bAllowBoss)
				{
					AddIssue(OutIssues, TEXT("EnemySpawnPointGrade"), Actor, TEXT("Enable at least one permitted monster grade."));
				}
				if (!FMath::IsFinite(Point->MaxCapsuleRadius) || Point->MaxCapsuleRadius < 0.f ||
					!FMath::IsFinite(Point->MaxCapsuleHalfHeight) || Point->MaxCapsuleHalfHeight < 0.f)
				{
					AddIssue(OutIssues, TEXT("EnemySpawnPointSize"), Actor, TEXT("Capsule limits must be finite and nonnegative."));
				}
				for (const auto& AllowedClass : Point->AllowedClasses)
				{
					if (!AllowedClass) AddIssue(OutIssues, TEXT("EnemySpawnPointClass"), Actor, TEXT("AllowedClasses cannot contain an empty class."));
				}
			}
			if (const auto* Exclusion = Cast<AHBRoomSpawnExclusionVolume>(Actor); IsValid(Exclusion))
			{
				const UBoxComponent* Box = Exclusion->Bounds;
				if (!Box || !Box->GetComponentTransform().IsValid() || !HasPositiveExtent(Box->GetScaledBoxExtent()) ||
					!HasPositiveExtent(Box->GetComponentScale()))
				{
					AddIssue(OutIssues, TEXT("SpawnExclusionSize"), Actor, TEXT("Exclusion bounds must have finite transform and positive size."));
				}
				if (Box && (Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision || Box->GetGenerateOverlapEvents() || Box->CanEverAffectNavigation()))
				{
					AddIssue(OutIssues, TEXT("SpawnExclusionCollision"), Actor, TEXT("Exclusion is metadata and must not collide, overlap or affect navigation."));
				}
			}
		}
		if (Setup && IsValid(Setup->Encounter))
		{
			for (const FHBEncounterGroup& Group : Setup->Encounter->Groups)
			{
				int32 Eligible = 0;
				for (AActor* Actor : Level->Actors)
					if (const auto* Point = Cast<AHBEnemySpawnPoint>(Actor); IsValid(Point)
						&& (Point->GroupId.IsNone() || Point->GroupId == Group.GroupId))
					{
						const bool bGrade = Group.Grade == EHBEncounterGrade::Common ? Point->bAllowCommon
							: Group.Grade == EHBEncounterGrade::Elite ? Point->bAllowElite : Point->bAllowBoss;
						if (!bGrade) continue;
						if (Group.Monsters.ContainsByPredicate([Point](const FHBEncounterMonsterOption& Option)
						{
							return Option.EnemyClass && (Point->AllowedClasses.IsEmpty()
								|| Point->AllowedClasses.Contains(Option.EnemyClass));
						})) ++Eligible;
					}
				if (Eligible < Group.MinCount)
					AddIssue(OutIssues, TEXT("EncounterPoints"), Setup,
						FString::Printf(TEXT("Group %s needs %d usable points but has %d."),
							*Group.GroupId.ToString(), Group.MinCount, Eligible));
			}
		}
		else if (SpawnPointCount > 0)
		{
			AddIssue(OutIssues, TEXT("MissingEncounterSetup"), Definition, TEXT("Spawn points need an Encounter Setup actor."));
		}
		if (Definition->ModuleId.IsNone())
		{
			AddIssue(OutIssues, TEXT("MissingModuleId"), Definition, TEXT("ModuleId must be assigned."));
		}
		if (!IsUnitTransform(Definition->GetActorTransform()))
		{
			AddIssue(OutIssues, TEXT("DefinitionTransform"), Definition, TEXT("Definition transform must be finite, with a normalized rotation and scale (1,1,1)."));
		}
		if (Definition->ModuleKind != EHBDungeonModuleKind::Room && Definition->ModuleKind != EHBDungeonModuleKind::Corridor)
		{
			AddIssue(OutIssues, TEXT("InvalidKind"), Definition, TEXT("ModuleKind is invalid."));
		}
		TInlineComponentArray<UHBRoomInfoComponent*> Info(Definition);
		if (Info.Num() != 1)
		{
			AddIssue(OutIssues, TEXT("RoomInfoCount"), Definition, TEXT("Expected exactly one Room Info component (ignored for corridor roles)."));
		}
		if (Definition->ModuleKind == EHBDungeonModuleKind::Room && Info.Num() == 1)
		{
			const auto Role = Info[0]->RoomRole;
			if (Role != EHBPrototypeRoomRole::Adventure && Role != EHBPrototypeRoomRole::Recovery &&
				Role != EHBPrototypeRoomRole::Upgrade && Role != EHBPrototypeRoomRole::Boss)
			{
				AddIssue(OutIssues, TEXT("MissingRole"), Info[0], TEXT("Select a fixed RoomRole: Adventure, Recovery, Upgrade or Boss."));
			}
		}
		if (Info.Num() == 1)
		{
			if (static_cast<uint8>(Info[0]->AllowedFloor) > 2)
				AddIssue(OutIssues, TEXT("AllowedFloor"), Info[0], TEXT("Select Any, Floor1 or Floor2."));
			if (Definition->ModuleKind == EHBDungeonModuleKind::Room && Info[0]->RoomRole == EHBPrototypeRoomRole::Boss
				&& Info[0]->AllowedFloor == EHBAllowedFloor::Floor1)
				AddIssue(OutIssues, TEXT("BossFloor"), Info[0], TEXT("Boss rooms cannot be restricted to Floor1."));
		}
		TSet<FName> PortalIds;
		for (AActor* Actor : Level->Actors)
		{
			if (const auto* Portal = Cast<AHBDungeonPortal>(Actor); IsValid(Portal))
			{
				FString Error;
				if (!Portal->ValidateDefinition(Error)) AddIssue(OutIssues, TEXT("PortalDefinition"), Actor, Error);
				if (PortalIds.Contains(Portal->PortalId)) AddIssue(OutIssues, TEXT("PortalId"), Actor, TEXT("PortalId must be unique within the room map."));
				PortalIds.Add(Portal->PortalId);
			}
		}

		auto Connectors = GatherComponents<UHBDungeonConnectorComponent>(Level);
		if (Connectors.IsEmpty() || (Definition->ModuleKind == EHBDungeonModuleKind::Corridor && Connectors.Num() < 2))
		{
			AddIssue(OutIssues, TEXT("ConnectorCount"), Definition, TEXT("Rooms need at least one connector; corridors need at least two."));
		}
		TSet<FName> Ids;
		for (UHBDungeonConnectorComponent* Connector : Connectors)
		{
			if (Connector->GetEffectiveConnectorId().IsNone() || Ids.Contains(Connector->GetEffectiveConnectorId()))
			{
				AddIssue(OutIssues, TEXT("ConnectorId"), Connector, TEXT("ConnectorId must be nonempty and unique within the module."));
			}
			Ids.Add(Connector->GetEffectiveConnectorId());
			if (!IsUnitTransform(Connector->GetComponentTransform()) || !IsUnitTransform(Connector->GetRelativeTransform()))
			{
				AddIssue(OutIssues, TEXT("ConnectorTransform"), Connector, TEXT("Connector transforms must be finite with scale (1,1,1). Move and rotate the marker instead of scaling."));
			}
		}

		auto Boxes = GatherComponents<UHBDungeonOccupancyComponent>(Level);
		if (Boxes.IsEmpty())
		{
			AddIssue(OutIssues, TEXT("MissingOccupancy"), Definition, TEXT("Add at least one Occupancy component."));
		}
		for (UHBDungeonOccupancyComponent* Box : Boxes)
		{
			if (!HasPositiveExtent(Box->GetUnscaledBoxExtent()) || !HasPositiveExtent(Box->GetScaledBoxExtent()))
			{
				AddIssue(OutIssues, TEXT("OccupancySize"), Box, TEXT("All Box Extent values must be finite and greater than zero (half size, cm)."));
			}
			if (!Box->GetComponentTransform().IsValid() || !Box->GetRelativeTransform().IsValid() ||
				!HasPositiveExtent(Box->GetComponentScale()) || !HasPositiveExtent(Box->GetRelativeScale3D()))
			{
				AddIssue(OutIssues, TEXT("OccupancyTransform"), Box, TEXT("Occupancy transforms must be finite with positive scale on all axes. Resize using Scale or Box Extent."));
			}
			if (Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision || Box->GetGenerateOverlapEvents())
			{
				AddIssue(OutIssues, TEXT("OccupancyCollision"), Box, TEXT("Occupancy is metadata: collision and overlap events must be disabled."));
			}
		}
	}
	return OutIssues.IsEmpty();
}

void AHBDungeonModuleDefinition::ValidateModule()
{
#if WITH_EDITOR
	TArray<FHBDungeonModuleIssue> Issues;
	const bool bValid = ValidateLevel(GetLevel(), Issues);
	FMessageLog Log(TEXT("MapCheck"));
	Log.NewPage(FText::FromString(FString::Printf(TEXT("Dungeon module: %s"), *ModuleId.ToString())));
	for (const FHBDungeonModuleIssue& Issue : Issues)
	{
		Log.Error()->AddToken(FUObjectToken::Create(Issue.Source.Get()))
			->AddToken(FTextToken::Create(FText::FromString(FString::Printf(TEXT(" [%s] %s"), *Issue.Code.ToString(), *Issue.Message))));
	}
	if (bValid)
	{
		Log.Info()->AddToken(FUObjectToken::Create(this))->AddToken(FTextToken::Create(FText::FromString(
			TEXT(" Module settings passed. Physical passage, geometry coverage and catalog-wide ID uniqueness have NOT been tested."))));
	}
	Log.Open(EMessageSeverity::Info, true);
#endif
}

bool AHBDungeonModuleDefinition::ReadModuleGeometry(const FTransform& ModuleToWorld,
	TArray<FHBDungeonConnectorDefinition>& OutConnectors, TArray<FHBDungeonOccupancyDefinition>& OutOccupancy,
	TArray<FHBDungeonModuleIssue>& OutIssues) const
{
	OutConnectors.Reset();
	OutOccupancy.Reset();
	if (!ValidateLevel(GetLevel(), OutIssues)) { return false; }
	if (!IsUnitTransform(ModuleToWorld))
	{
		AddIssue(OutIssues, TEXT("ModuleFrame"), const_cast<AHBDungeonModuleDefinition*>(this), TEXT("Module frame must be finite with unit scale."));
		return false;
	}
	auto Connectors = GatherComponents<UHBDungeonConnectorComponent>(GetLevel());
	for (const UHBDungeonConnectorComponent* Connector : Connectors)
	{
		OutConnectors.Add({ Connector->GetEffectiveConnectorId(),
			Connector->GetComponentTransform().GetRelativeTransform(ModuleToWorld) });
	}
	OutConnectors.Sort([](const auto& A, const auto& B) { return A.ConnectorId.LexicalLess(B.ConnectorId); });
	auto Boxes = GatherComponents<UHBDungeonOccupancyComponent>(GetLevel());
	Boxes.Sort([](const UHBDungeonOccupancyComponent& A, const UHBDungeonOccupancyComponent& B) { return A.GetPathName() < B.GetPathName(); });
	for (const UHBDungeonOccupancyComponent* Box : Boxes)
	{
		FTransform LocalTransform = Box->GetComponentTransform().GetRelativeTransform(ModuleToWorld);
		const FVector ScaledExtent = Box->GetUnscaledBoxExtent() * LocalTransform.GetScale3D();
		LocalTransform.SetScale3D(FVector::OneVector);
		OutOccupancy.Add({ LocalTransform, ScaledExtent });
	}
	return true;
}
