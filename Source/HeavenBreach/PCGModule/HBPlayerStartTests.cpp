#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "IpNetDriver.h"
#include "Engine/ChildConnection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBRoomPlayerStartTest, "HeavenBreach.PCGModule.StartRoom.PlayerStartInstances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBRoomPlayerStartTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TArray<ULevel*> Levels;
	TArray<FHBPrototypeRoom> Rooms;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		ULevel* Level = NewObject<ULevel>(World, FName(*FString::Printf(TEXT("RoomInstance%d"), Index)));
		Level->OwningWorld = World; Levels.Add(Level);
		FHBPrototypeRoom Room;
		Room.Id = Index; Room.ParentId = Index - 1; Room.bIsStartRoom = Index == 0;
		Room.ModuleId = TEXT("SameCandidateMap");
		Room.ModuleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Tests/SameCandidateMap.SameCandidateMap")));
		Rooms.Add(Room);
	}
	auto Spawn = [&](ULevel* Level, const TCHAR* Name)
	{
		FActorSpawnParameters Params; Params.OverrideLevel = Level; Params.Name = Name;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<APlayerStart>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	};
	auto* PersistentStart = Spawn(World->PersistentLevel, TEXT("PersistentStart"));
	auto* OtherStart = Spawn(Levels[1], TEXT("StartA"));
	auto* DuplicateStart = Spawn(Levels[2], TEXT("StartA"));
	TArray<APlayerStart*> Starts;
	FString Error;
	TestFalse(TEXT("An actual start with zero PlayerStarts fails"), HBDungeonRuntimeValidation::PreparePlayerStarts(Rooms, Levels, Starts, Error));
	TestFalse(TEXT("Preparation failure preserves ordinary instances"), OtherStart->IsActorBeingDestroyed());
	auto* StartA = Spawn(Levels[0], TEXT("StartA"));
	auto* StartB = Spawn(Levels[0], TEXT("StartB"));
	TestTrue(TEXT("Prepare repeated copies of the same candidate"), HBDungeonRuntimeValidation::PreparePlayerStarts(Rooms, Levels, Starts, Error));
	TestEqual(TEXT("Multiple untagged starts remain in the sole start instance"), Starts.Num(), 2);
	TestTrue(TEXT("Ordinary copy loses its PlayerStart"), OtherStart->IsActorBeingDestroyed());
	TestTrue(TEXT("Repeated candidate copy loses its PlayerStart"), DuplicateStart->IsActorBeingDestroyed());
	TestFalse(TEXT("Persistent-level start is outside the operation"), PersistentStart->IsActorBeingDestroyed());
	TestTrue(TEXT("Retained starts belong to the actual start level"), Starts.Contains(StartA) && Starts.Contains(StartB));
	TestTrue(TEXT("Cleanup is repeatable"), HBDungeonRuntimeValidation::PreparePlayerStarts(Rooms, Levels, Starts, Error));
	TArray<APlayerStart*> Reordered = Starts; Algo::Reverse(Reordered);
	TSet<APlayerStart*> Selected;
	for (int32 Seed = 0; Seed < 32; ++Seed)
	{
		auto* Chosen = HBDungeonRuntimeValidation::SelectPlayerStart(Starts, Seed, [](const APlayerStart&) { return true; }, Error);
		Selected.Add(Chosen);
		TestEqual(TEXT("Reordered actor enumeration preserves Seed choice"),
			HBDungeonRuntimeValidation::SelectPlayerStart(Reordered, Seed, [](const APlayerStart&) { return true; }, Error), Chosen);
		TestEqual(TEXT("Blocked point is skipped in favor of a usable start"),
			HBDungeonRuntimeValidation::SelectPlayerStart(Starts, Seed, [StartB](const APlayerStart& Point) { return &Point == StartB; }, Error), StartB);
	}
	TestEqual(TEXT("Different seeds can choose either PlayerStart"), Selected.Num(), 2);
	TestNull(TEXT("All unusable starts fail"), HBDungeonRuntimeValidation::SelectPlayerStart(Starts, 7, [](const APlayerStart&) { return false; }, Error));
	TestFalse(TEXT("Unusable starts explain failure"), Error.IsEmpty());
	const auto SavedLevels = Levels;
	Levels[1] = Levels[0];
	TestFalse(TEXT("Aliased levels cannot be mistaken for separate instances"), HBDungeonRuntimeValidation::PreparePlayerStarts(Rooms, Levels, Reordered, Error));
	Levels = SavedLevels;

	auto* Driver = NewObject<UIpNetDriver>(World); Driver->SetWorld(World);
	Driver->ServerConnection = NewObject<UChildConnection>(Driver); World->SetNetDriver(Driver);
	TestEqual(TEXT("Fixture is a client world"), World->GetNetMode(), NM_Client);
	auto* ClientCopyStart = Spawn(Levels[1], TEXT("ClientCopyStart"));
	TestTrue(TEXT("Client applies the same instance cleanup"), HBDungeonRuntimeValidation::PreparePlayerStarts(Rooms, Levels, Reordered, Error));
	TestTrue(TEXT("Client ordinary instance has no PlayerStart"), ClientCopyStart->IsActorBeingDestroyed());
	TestNull(TEXT("Client cannot independently select a spawn point"),
		HBDungeonRuntimeValidation::SelectPlayerStart(Starts, 7, [](const APlayerStart&) { return true; }, Error));
	World->SetNetDriver(nullptr); Driver->ServerConnection = nullptr; Driver->SetWorld(nullptr);
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);

	// Source maps are authoring data, never a target for runtime deletion.
	UWorld* Source = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(Source);
	auto* SourceStart = Source->SpawnActor<APlayerStart>();
	TestFalse(TEXT("Source map cleanup is refused"), HBDungeonRuntimeValidation::PreparePlayerStarts({Rooms[0]}, {Source->PersistentLevel}, Starts, Error));
	TestFalse(TEXT("Authored PlayerStart is preserved"), SourceStart->IsActorBeingDestroyed());
	Source->DestroyWorld(false); GEngine->DestroyWorldContext(Source);
	return true;
}
#endif
