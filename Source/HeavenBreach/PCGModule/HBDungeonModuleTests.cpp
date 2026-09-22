#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBRoomInfoComponent.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "PCGModule/HBDungeonOccupancyComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "PCGModule/HBDungeonMarkerActors.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FModuleTestWorld
	{
		UWorld* World = nullptr;
		AHBDungeonConnector* Door = nullptr;
		AHBDungeonOccupancy* Bounds = nullptr;
		FModuleTestWorld()
		{
			const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(false).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr,
				true, ERHIFeatureLevel::Num, &Initialization);
			GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
		}
		~FModuleTestWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		AHBDungeonModuleDefinition* MakeRoom()
		{
			auto* Room = World->SpawnActor<AHBDungeonModuleDefinition>();
			Room->ModuleId = TEXT("TestRoom");
			Room->RoomInfo->RoomRole = EHBPrototypeRoomRole::Recovery;
			Door = World->SpawnActor<AHBDungeonConnector>();
			Bounds = World->SpawnActor<AHBDungeonOccupancy>();
			Bounds->Occupancy->SetBoxExtent(FVector(500, 500, 200));
			return Room;
		}
	};

	bool HasIssue(const TArray<FHBDungeonModuleIssue>& Issues, FName Code)
	{
		return Issues.ContainsByPredicate([Code](const auto& Issue) { return Issue.Code == Code; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBModuleValidationTest, "HeavenBreach.PCGModule.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBModuleValidationTest::RunTest(const FString&)
{
	FModuleTestWorld Fixture;
	TArray<FHBDungeonModuleIssue> Issues;
	TestFalse(TEXT("Missing definition rejected"), AHBDungeonModuleDefinition::ValidateLevel(Fixture.World->PersistentLevel, Issues));
	TestTrue(TEXT("Missing definition diagnostic"), HasIssue(Issues, TEXT("DefinitionCount")));
	auto* Room = Fixture.MakeRoom();
	TestTrue(TEXT("Configured room passes"), AHBDungeonModuleDefinition::ValidateLevel(Room->GetLevel(), Issues));
	TestFalse(TEXT("Occupancy has no overlap events"), Fixture.Bounds->Occupancy->GetGenerateOverlapEvents());
	TestEqual(TEXT("Occupancy has no physical collision"), Fixture.Bounds->Occupancy->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	Room->RoomInfo->RoomRole = EHBPrototypeRoomRole::Unassigned;
	Room->ModuleId = NAME_None;
	Fixture.Bounds->Occupancy->SetBoxExtent(FVector::ZeroVector);
	TestFalse(TEXT("Incomplete settings rejected"), AHBDungeonModuleDefinition::ValidateLevel(Room->GetLevel(), Issues));
	for (FName Code : { FName(TEXT("MissingRole")), FName(TEXT("MissingModuleId")), FName(TEXT("OccupancySize")) })
	{
		TestTrue(*Code.ToString(), HasIssue(Issues, Code));
	}
	Room->ModuleId = TEXT("Corridor");
	Room->ModuleKind = EHBDungeonModuleKind::Corridor;
	Fixture.Bounds->Occupancy->SetBoxExtent(FVector(100));
	TestFalse(TEXT("One-ended corridor rejected"), AHBDungeonModuleDefinition::ValidateLevel(Room->GetLevel(), Issues));
	TestTrue(TEXT("Corridor end diagnostic"), HasIssue(Issues, TEXT("ConnectorCount")));
	TestFalse(TEXT("Corridor does not require a room role"), HasIssue(Issues, TEXT("MissingRole")));
	auto* OtherActor = Fixture.World->SpawnActor<AHBDungeonConnector>();
	auto* Other = OtherActor->Connector.Get();
	TestTrue(TEXT("Independent markers automatically receive unique IDs"),
		Other->GetEffectiveConnectorId() != Fixture.Door->Connector->GetEffectiveConnectorId());
	Fixture.Door->Connector->ConnectorId = TEXT("Door_A");
	Other->ConnectorId = TEXT("Door_A");
	TestFalse(TEXT("Duplicate connector IDs rejected"), AHBDungeonModuleDefinition::ValidateLevel(Room->GetLevel(), Issues));
	TestTrue(TEXT("Duplicate ID diagnostic"), HasIssue(Issues, TEXT("ConnectorId")));
	Other->ConnectorId = TEXT("Door_B");
	TestTrue(TEXT("Corridor passes with two ends and no role"), AHBDungeonModuleDefinition::ValidateLevel(Room->GetLevel(), Issues));
	Other->SetRelativeScale3D(FVector(2));
	TestFalse(TEXT("Scaled connector rejected"), AHBDungeonModuleDefinition::ValidateLevel(Room->GetLevel(), Issues));
	TestTrue(TEXT("Scale diagnostic"), HasIssue(Issues, TEXT("ConnectorTransform")));
	Other->SetRelativeScale3D(FVector::OneVector);
	auto* Third = Fixture.World->SpawnActor<AHBDungeonConnector>();
	auto* ExtraBox = Fixture.World->SpawnActor<AHBDungeonOccupancy>();
	ExtraBox->Occupancy->SetBoxExtent(FVector(80,90,100));
	ULevel* ForeignLevel = NewObject<ULevel>(Fixture.World, TEXT("OtherModuleLevel"));
	ForeignLevel->OwningWorld = Fixture.World;
	FActorSpawnParameters Spawn;
	Spawn.OverrideLevel = ForeignLevel;
	auto* Foreign = Fixture.World->SpawnActor<AHBDungeonConnector>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	Foreign->Connector->ConnectorId = TEXT("Door_A");
	TArray<FHBDungeonConnectorDefinition> Connections;
	TArray<FHBDungeonOccupancyDefinition> Occupancy;
	TestTrue(TEXT("Three doors, two boxes and foreign duplicate ID are accepted"),
		Room->ReadModuleGeometry(FTransform::Identity, Connections, Occupancy, Issues));
	TestEqual(TEXT("All three independent doors collected, foreign door excluded"), Connections.Num(), 3);
	TestEqual(TEXT("Both independent boxes collected"), Occupancy.Num(), 2);
	TInlineComponentArray<UHBDungeonConnectorComponent*> Attached(Room);
	TestEqual(TEXT("Definition has no embedded connectors"), Attached.Num(), 0);
	Foreign->Destroy();
	Fixture.MakeRoom();
	TestFalse(TEXT("Duplicate definitions rejected"), AHBDungeonModuleDefinition::ValidateLevel(Room->GetLevel(), Issues));
	TestTrue(TEXT("Duplicate definition diagnostic"), HasIssue(Issues, TEXT("DefinitionCount")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBModuleCoordinatesTest, "HeavenBreach.PCGModule.ModuleCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBModuleCoordinatesTest::RunTest(const FString&)
{
	FModuleTestWorld Fixture;
	auto* Room = Fixture.MakeRoom();
	Fixture.Door->SetActorTransform(FTransform(FRotator(0,90,0), FVector(1000,2200,100)));
	Fixture.Bounds->SetActorTransform(FTransform(FRotator(0,35,0), FVector(1000,2000,300)));
	Fixture.Bounds->SetActorScale3D(FVector(2,3,0.5));
	Room->SetActorLocation(FVector(9000,8000,7000));
	TArray<FHBDungeonConnectorDefinition> Connectors;
	TArray<FHBDungeonOccupancyDefinition> Boxes;
	TArray<FHBDungeonModuleIssue> Issues;
	TestTrue(TEXT("Read source map geometry"), Room->ReadModuleGeometry(FTransform::Identity, Connectors, Boxes, Issues));
	if (Connectors.Num() != 1 || Boxes.Num() != 1) { AddError(TEXT("Missing extracted geometry")); return false; }
	TestTrue(TEXT("Independent connector position preserved despite definition movement"), Connectors[0].LocalTransform.GetLocation().Equals(FVector(1000,2200,100), 0.01));
	const FTransform OriginalConnector = Connectors[0].LocalTransform;
	const FTransform OriginalBox = Boxes[0].LocalTransform;
	TestTrue(TEXT("Nonuniform occupancy scale baked into half-size"), Boxes[0].Extent.Equals(FVector(1000,1500,100), 0.01));
	TestTrue(TEXT("Extracted transform cannot double-apply scale"), OriginalBox.GetScale3D().Equals(FVector::OneVector));
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Sign((Corner & 1) ? 1 : -1, (Corner & 2) ? 1 : -1, (Corner & 4) ? 1 : -1);
		TestTrue(TEXT("Export matches rotated scaled box corners"),
			OriginalBox.TransformPosition(Boxes[0].Extent * Sign).Equals(
				Fixture.Bounds->GetActorTransform().TransformPosition(Fixture.Bounds->Occupancy->GetUnscaledBoxExtent() * Sign), 0.01));
	}
	const FTransform ModuleToWorld(FRotator(0,-90,0), FVector(-5000,600,800));
	Room->SetActorTransform(Room->GetActorTransform() * ModuleToWorld);
	Fixture.Door->SetActorTransform(Fixture.Door->GetActorTransform() * ModuleToWorld);
	Fixture.Bounds->SetActorTransform(Fixture.Bounds->GetActorTransform() * ModuleToWorld);
	TestTrue(TEXT("Read instanced module geometry"), Room->ReadModuleGeometry(ModuleToWorld, Connectors, Boxes, Issues));
	TestTrue(TEXT("Instance transform removed without using definition transform"), Connectors[0].LocalTransform.Equals(OriginalConnector, 0.01));
	TestTrue(TEXT("Rotated occupancy box retained"), Boxes[0].LocalTransform.Equals(OriginalBox, 0.01));
	TestTrue(TEXT("Instance frame preserves scaled extent"), Boxes[0].Extent.Equals(FVector(1000,1500,100), 0.01));
	for (const FVector BadScale : { FVector(0,1,1), FVector(-1,1,1) })
	{
		Fixture.Bounds->SetActorScale3D(BadScale);
		TestFalse(TEXT("Zero/negative occupancy scale rejected"), Room->ReadModuleGeometry(ModuleToWorld, Connectors, Boxes, Issues));
		TestTrue(TEXT("Invalid occupancy scale diagnostic"), HasIssue(Issues, TEXT("OccupancyTransform")));
		TestTrue(TEXT("Invalid occupancy scale clears outputs"), Connectors.IsEmpty() && Boxes.IsEmpty());
	}
	Fixture.Bounds->SetActorScale3D(FVector::OneVector);
	Fixture.Door->SetActorScale3D(FVector(2));
	TestFalse(TEXT("Invalid geometry cannot be exported"), Room->ReadModuleGeometry(ModuleToWorld, Connectors, Boxes, Issues));
	TestTrue(TEXT("Failure clears both outputs"), Connectors.IsEmpty() && Boxes.IsEmpty());
	return true;
}
#endif
