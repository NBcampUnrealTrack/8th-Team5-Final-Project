#include "PCGModule/HBRoomEncounterComponent.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "AI/EnemyCharacterBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBDungeonOccupancyComponent.h"
#include "PCGModule/HBDungeonConnectorComponent.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Misc/PackageName.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBRoomEncounterSelectionTest, "HeavenBreach.PCGModule.EncounterSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHBRoomEncounterSelectionTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	TArray<AHBEnemySpawnPoint*> Points;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		auto* Point = World->SpawnActor<AHBEnemySpawnPoint>();
		Point->PointId = FName(*FString::Printf(TEXT("Point_%02d"), Index));
		Points.Add(Point);
	}
	Points[0]->bRequired = true;
	auto* Profile = NewObject<UHBRoomEncounterProfile>(GetTransientPackage());
	Profile->MinCommonCount = 3;
	Profile->MaxCommonCount = 3;
	FHBEnemySpawnOption Option;
	Option.EnemyClass = AEnemyCharacterBase::StaticClass();
	Profile->CommonEnemies.Add(Option);
	TArray<FHBEncounterSpawnChoice> Placed;
	FString Error;
	auto Accept = [](const FHBEncounterSpawnChoice&, FString&) { return true; };
	auto Signature = [](const TArray<FHBEncounterSpawnChoice>& Choices)
	{
		FString Result;
		for (const auto& Choice : Choices) Result += Choice.PointId.ToString() + TEXT("|");
		return Result;
	};
	TestTrue(TEXT("Select three common monsters"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Exact requested count"), Placed.Num(), 3);
	TestTrue(TEXT("Required point selected"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == Points[0]->PointId; }));
	const FString FirstSignature = Signature(Placed);
	Algo::Reverse(Points);
	TestTrue(TEXT("Reordered actor enumeration accepted"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Same seed and room yield the same placement"), Signature(Placed), FirstSignature);
	TSet<FString> RoomSignatures;
	for (int32 RoomId = 0; RoomId < 8; ++RoomId)
	{
		TestTrue(TEXT("Reused module plans independently for each room"),
			HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, RoomId, Accept, Placed, Error));
		RoomSignatures.Add(Signature(Placed));
	}
	TestTrue(TEXT("Different room IDs can change the placement"), RoomSignatures.Num() > 1);

	FName RejectedPoint;
	const FName RequiredId = Points.Last()->PointId;
	auto RejectOnePoint = [&RejectedPoint, RequiredId](const FHBEncounterSpawnChoice& Choice, FString& Reason)
	{
		if (Choice.PointId == RequiredId) return true;
		if (RejectedPoint.IsNone()) RejectedPoint = Choice.PointId;
		if (Choice.PointId == RejectedPoint) { Reason = TEXT("Unsafe candidate"); return false; }
		return true;
	};
	TestTrue(TEXT("Rejected point is replaced by another candidate"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, RejectOnePoint, Placed, Error));
	TestEqual(TEXT("Minimum survives one rejection"), Placed.Num(), 3);
	TestFalse(TEXT("Rejected point is absent"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == RejectedPoint; }));
	auto RejectAll = [](const FHBEncounterSpawnChoice&, FString& Reason)
	{
		Reason = TEXT("No safe floor");
		return false;
	};
	TestFalse(TEXT("Insufficient required count fails with bounded attempts"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, RejectAll, Placed, Error));
	TestTrue(TEXT("Failure names room and reason"), Error.Contains(TEXT("Room 2")) && Error.Contains(TEXT("No safe floor")));

	Profile->MinCommonCount = 0;
	Profile->MaxCommonCount = 0;
	Profile->CommonEnemies.Reset();
	Profile->BossClass = AEnemyCharacterBase::StaticClass();
	Profile->BossPointId = Points.Last()->PointId;
	Points.Last()->bAllowCommon = false;
	Points.Last()->bAllowBoss = true;
	TestTrue(TEXT("Boss is placed only at its dedicated point"),
		HBRoomEncounter::SelectAndPlace(*Profile, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Exactly one boss"), Placed.Num(), 1);
	if (Placed.Num() == 1)
	{
		TestEqual(TEXT("Boss point ID"), Placed[0].PointId, Profile->BossPointId);
		TestEqual(TEXT("Boss grade"), Placed[0].Grade, EHBEnemySpawnGrade::Boss);
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
class FHBWaitForTestMGEncounter : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Started = FPlatformTime::Seconds();

public:
	explicit FHBWaitForTestMGEncounter(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
		}
		const double Elapsed = FPlatformTime::Seconds() - Started;
		if (!World || World->GetTimeSeconds() < 1.f || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
		{
			if (Elapsed < 60.) return false;
			Test->AddError(TEXT("TestMG PIE world or navigation did not become ready."));
			return true;
		}
		AHBDungeonModuleDefinition* Definition = nullptr;
		for (TActorIterator<AHBDungeonModuleDefinition> It(World); It; ++It) { Definition = *It; break; }
		Test->TestNotNull(TEXT("TestMG has a module definition"), Definition);
		if (!Definition) return true;
		Test->TestTrue(TEXT("Standalone PIE owns the encounter on the server"), Definition->HasAuthority());
		Test->TestNotNull(TEXT("TestMG has a room profile"), Definition->EncounterProfile.Get());
		if (!Definition->EncounterProfile || !Definition->HasAuthority()) return true;
		TArray<FHBDungeonModuleIssue> Issues;
		if (!AHBDungeonModuleDefinition::ValidateLevel(Definition->GetLevel(), Issues))
		{
			for (const FHBDungeonModuleIssue& Issue : Issues)
				Test->AddError(FString::Printf(TEXT("TestMG authoring: %s: %s"), *Issue.Code.ToString(), *Issue.Message));
			return true;
		}

		FBox RoomBounds(ForceInit);
		TArray<FVector> Entries;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetLevel() != Definition->GetLevel()) continue;
			TInlineComponentArray<UHBDungeonOccupancyComponent*> Occupancy(*It);
			for (const auto* Box : Occupancy) RoomBounds += Box->Bounds.GetBox();
			TInlineComponentArray<UHBDungeonConnectorComponent*> Connectors(*It);
			for (const auto* Connector : Connectors) Entries.Add(Connector->GetComponentLocation());
		}
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			if (It->GetLevel() == Definition->GetLevel()) Entries.Add(It->GetActorLocation());
		}
		Test->TestTrue(TEXT("TestMG has room bounds"), RoomBounds.IsValid != 0);
		Test->TestTrue(TEXT("TestMG has an entry"), !Entries.IsEmpty());
		if (!RoomBounds.IsValid || Entries.IsEmpty()) return true;
		FString Error;
		auto* Encounter = Definition->Encounter.Get();
		Test->TestNotNull(TEXT("Definition owns encounter component"), Encounter);
		if (!Encounter) return true;
		if (!Encounter->StartEncounter(1, 12345, 7, RoomBounds, Entries, Error))
		{
			Test->AddError(FString::Printf(TEXT("TestMG real enemy spawn failed: %s"), *Error));
			return true;
		}
		Test->TestTrue(TEXT("Encounter prepared for this room"), Encounter->IsPreparedFor(1, 7));
		TArray<AEnemyCharacterBase*> Spawned;
		int32 CommonCount = 0;
		int32 EliteCount = 0;
		AEnemyCharacterBase* CommonToKill = nullptr;
		for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It)
		{
			AEnemyCharacterBase* Enemy = *It;
			if (Enemy->GetLevel() != Definition->GetLevel()) continue;
			Spawned.Add(Enemy);
			Test->TestNotNull(TEXT("Spawned monster has an AI controller"), Enemy->GetController());
			if (AController* Controller = Enemy->GetController())
			{
				UStateTreeAIComponent* StateTree = Controller->FindComponentByClass<UStateTreeAIComponent>();
				Test->TestNotNull(TEXT("Spawned monster has a StateTree component"), StateTree);
				if (StateTree)
					Test->TestTrue(TEXT("Spawned monster StateTree is running after possession"), StateTree->IsRunning());
			}
			Test->TestNotNull(TEXT("Spawned monster has an ASC"), Enemy->GetEnemyASC());
			Test->TestFalse(TEXT("Spawned monster starts alive"), Enemy->IsDead());
			if (Definition->EncounterProfile->EliteEnemies.ContainsByPredicate(
				[Enemy](const FHBEnemySpawnOption& Option) { return Option.EnemyClass.Get() == Enemy->GetClass(); }))
			{
				++EliteCount;
			}
			else if (Definition->EncounterProfile->CommonEnemies.ContainsByPredicate(
				[Enemy](const FHBEnemySpawnOption& Option) { return Option.EnemyClass.Get() == Enemy->GetClass(); }))
			{
				++CommonCount;
				CommonToKill = Enemy;
			}
			else Test->AddError(FString::Printf(TEXT("Unexpected monster class: %s"), *Enemy->GetClass()->GetName()));
		}
		Test->TestTrue(TEXT("One or two common monsters spawned"), CommonCount >= 1 && CommonCount <= 2);
		Test->TestEqual(TEXT("Stone Golem elite spawned once"), EliteCount, 1);
		Test->TestEqual(TEXT("Alive count matches real actors"), Encounter->AliveCount, Spawned.Num());
		if (CommonToKill)
		{
			const int32 BeforeDeath = Encounter->AliveCount;
			CommonToKill->BecomeDead();
			Test->TestTrue(TEXT("Real enemy death applied"), CommonToKill->IsDead());
			Test->TestEqual(TEXT("Death decrements alive count once"), Encounter->AliveCount, BeforeDeath - 1);
			CommonToKill->BecomeDead();
			Test->TestEqual(TEXT("Repeated death does not double count"), Encounter->AliveCount, BeforeDeath - 1);
		}
		Encounter->ClearEncounter();
		Test->TestEqual(TEXT("Cleanup clears alive count"), Encounter->AliveCount, 0);
		Test->TestFalse(TEXT("Cleanup clears generation state"), Encounter->IsPreparedFor(1, 7));
		for (AEnemyCharacterBase* Enemy : Spawned)
			Test->TestTrue(TEXT("Cleanup destroys each spawned monster"), !IsValid(Enemy) || Enemy->IsActorBeingDestroyed());
		if (!Encounter->StartEncounter(2, 12345, 7, RoomBounds, Entries, Error))
			Test->AddError(FString::Printf(TEXT("TestMG restart after cleanup failed: %s"), *Error));
		else Test->TestEqual(TEXT("Restart produces the same seeded count"), Encounter->AliveCount, Spawned.Num());
		Encounter->ClearEncounter();
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBTestMGEncounterIntegrationTest, "HeavenBreach.PCGModule.Encounter.TestMGStandalone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBTestMGEncounterIntegrationTest::RunTest(const FString&)
{
	const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestMG");
	if (!FPackageName::DoesPackageExist(Map)) { AddError(TEXT("TestMG fixture map is missing.")); return false; }
	// Dynamically spawned AI starts its StateTree once before possession. OnPossess restarts it;
	// the integration assertion above still requires the resulting tree to be running.
	AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
	EPlayNetMode PreviousMode;
	int32 PreviousClients;
	const TSharedRef<TSubclassOf<AGameModeBase>> PreviousMapMode = MakeShared<TSubclassOf<AGameModeBase>>();
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, PreviousMapMode]()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Editor && Context.World())
			{
				AWorldSettings* Settings = Context.World()->GetWorldSettings();
				*PreviousMapMode = Settings->DefaultGameMode;
				Settings->DefaultGameMode = AGameModeBase::StaticClass();
				return true;
			}
		}
		AddError(TEXT("Could not isolate TestMG from the project's gameplay GameMode."));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForTestMGEncounter(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([PreviousMode, PreviousClients, PreviousMapMode]()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Editor && Context.World())
				Context.World()->GetWorldSettings()->DefaultGameMode = *PreviousMapMode;
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
		return true;
	}));
	return true;
}
#endif
