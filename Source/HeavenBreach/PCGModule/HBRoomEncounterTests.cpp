#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGModule/HBEnemySpawnPoint.h"
#include "AI/EnemyCharacterBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
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
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Misc/PackageName.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBEncounterSelectionTest, "HeavenBreach.PCGModule.EncounterSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FHBEncounterSelectionTest::RunTest(const FString&)
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
	UHBEncounterComponent* Config = NewObject<UHBEncounterComponent>(GetTransientPackage());
	FHBEncounterGroup Common;
	Common.GroupId = TEXT("Common");
	Common.MinCount = Common.MaxCount = 3;
	FHBEncounterMonsterOption Option;
	Option.EnemyClass = AEnemyCharacterBase::StaticClass();
	Common.Monsters.Add(Option);
	Config->Groups.Add(Common);
	TArray<FHBEncounterSpawnChoice> Placed;
	FString Error;
	auto Accept = [](const FHBEncounterSpawnChoice&, FString&) { return true; };
	auto Signature = [](const TArray<FHBEncounterSpawnChoice>& Choices)
	{
		FString Result;
		for (const FHBEncounterSpawnChoice& Choice : Choices) Result += Choice.PointId.ToString() + TEXT("|");
		return Result;
	};
	TestTrue(TEXT("Select three common monsters"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Exact requested count"), Placed.Num(), 3);
	TestTrue(TEXT("Required point selected"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == Points[0]->PointId; }));
	const FString FirstSignature = Signature(Placed);
	Algo::Reverse(Points);
	TestTrue(TEXT("Reordered actor enumeration accepted"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Same seed and room yield the same placement"), Signature(Placed), FirstSignature);
	TSet<FString> RoomSignatures;
	for (int32 RoomId = 0; RoomId < 8; ++RoomId)
	{
		TestTrue(TEXT("Reused module plans independently for each room"),
			HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, RoomId, Accept, Placed, Error));
		RoomSignatures.Add(Signature(Placed));
	}
	TestTrue(TEXT("Different room IDs can change placement"), RoomSignatures.Num() > 1);
	Config->Groups[0].MinCount = 2;
	Config->Groups[0].MaxCount = 4;
	TSet<int32> SeenCounts;
	for (int32 Seed = 1; Seed <= 32; ++Seed)
	{
		TestTrue(TEXT("Ranged count remains placeable"),
			HBEncounterSelection::SelectAndPlace(*Config, Points, Seed, 2, Accept, Placed, Error));
		TestTrue(TEXT("Ranged count stays within LD limits"), Placed.Num() >= 2 && Placed.Num() <= 4);
		SeenCounts.Add(Placed.Num());
	}
	TestTrue(TEXT("Changing dungeon seed can vary the count"), SeenCounts.Num() > 1);
	Config->Groups[0].MinCount = Config->Groups[0].MaxCount = 3;
	Points[1]->GroupId = TEXT("AnotherGroup");
	TestTrue(TEXT("Other group point is not selected"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestFalse(TEXT("Group filtering excludes the marked point"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == Points[1]->PointId; }));
	Points[1]->GroupId = NAME_None;

	FName RejectedPoint;
	const FName RequiredId = Points.Last()->PointId;
	auto RejectOne = [&RejectedPoint, RequiredId](const FHBEncounterSpawnChoice& Choice, FString& Reason)
	{
		if (Choice.PointId == RequiredId) return true;
		if (RejectedPoint.IsNone()) RejectedPoint = Choice.PointId;
		if (Choice.PointId == RejectedPoint) { Reason = TEXT("Unsafe candidate"); return false; }
		return true;
	};
	TestTrue(TEXT("Rejected point is replaced"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, RejectOne, Placed, Error));
	TestEqual(TEXT("Minimum survives one rejection"), Placed.Num(), 3);
	TestFalse(TEXT("Rejected point is absent"), Placed.ContainsByPredicate(
		[&](const FHBEncounterSpawnChoice& Choice) { return Choice.PointId == RejectedPoint; }));
	auto RejectAll = [](const FHBEncounterSpawnChoice&, FString& Reason)
	{
		Reason = TEXT("No safe floor");
		return false;
	};
	TestFalse(TEXT("Insufficient required count fails"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, RejectAll, Placed, Error));
	TestTrue(TEXT("Failure names room and reason"), Error.Contains(TEXT("Room 2")) && Error.Contains(TEXT("No safe floor")));

	Config->Groups.Reset();
	FHBEncounterGroup Boss;
	Boss.GroupId = TEXT("Boss");
	Boss.Grade = EHBEncounterGrade::Boss;
	Boss.MinCount = Boss.MaxCount = 1;
	Boss.Monsters.Add(Option);
	Config->Groups.Add(Boss);
	Points.Last()->bAllowCommon = false;
	Points.Last()->bAllowBoss = true;
	Points.Last()->GroupId = TEXT("Boss");
	TestTrue(TEXT("Boss uses its dedicated point group"),
		HBEncounterSelection::SelectAndPlace(*Config, Points, 12345, 2, Accept, Placed, Error));
	TestEqual(TEXT("Exactly one boss"), Placed.Num(), 1);
	if (Placed.Num() == 1)
	{
		TestEqual(TEXT("Boss point ID"), Placed[0].PointId, Points.Last()->PointId);
		TestEqual(TEXT("Boss grade"), Placed[0].Grade, EHBEncounterGrade::Boss);
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
	bool bCreatedTestNavigationBounds = false;
public:
	explicit FHBWaitForTestMGEncounter(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
		const double Elapsed = FPlatformTime::Seconds() - Started;
		if (!World || World->GetTimeSeconds() < 1.f)
		{
			if (Elapsed < 60.) return false;
			Test->AddError(TEXT("TestMG PIE world did not start."));
			return true;
		}
		AHBDungeonModuleDefinition* Definition = nullptr;
		AHBEncounterSetupActor* Setup = nullptr;
		for (TActorIterator<AHBDungeonModuleDefinition> It(World); It; ++It) { Definition = *It; break; }
		Test->TestNotNull(TEXT("TestMG has a module definition"), Definition);
		if (!Definition) return true;
		for (AActor* Actor : Definition->GetLevel()->Actors)
			if (auto* Candidate = Cast<AHBEncounterSetupActor>(Actor)) { Setup = Candidate; break; }
		Test->TestNotNull(TEXT("TestMG has an Encounter Setup"), Setup);
		if (!Setup || !Setup->Encounter) return true;
		Test->TestTrue(TEXT("Standalone PIE owns the encounter on the server"), Setup->HasAuthority());
		UHBEncounterSubsystem* Subsystem = World->GetSubsystem<UHBEncounterSubsystem>();
		Test->TestNotNull(TEXT("TestMG world has an encounter subsystem"), Subsystem);
		if (!Subsystem) return true;
		TArray<FHBDungeonModuleIssue> Issues;
		if (!AHBDungeonModuleDefinition::ValidateLevel(Definition->GetLevel(), Issues))
		{
			for (const FHBDungeonModuleIssue& Issue : Issues)
				Test->AddError(FString::Printf(TEXT("TestMG authoring: %s: %s"), *Issue.Code.ToString(), *Issue.Message));
			return true;
		}
		FBox RoomBounds(ForceInit);
		TArray<FVector> Entries;
		for (AActor* Actor : Definition->GetLevel()->Actors)
		{
			if (!IsValid(Actor)) continue;
			TInlineComponentArray<UHBDungeonOccupancyComponent*> Occupancy(Actor);
			for (const auto* Box : Occupancy) RoomBounds += Box->Bounds.GetBox();
			TInlineComponentArray<UHBDungeonConnectorComponent*> Connectors(Actor);
			for (const auto* Connector : Connectors) Entries.Add(Connector->GetComponentLocation());
			if (const auto* Start = Cast<APlayerStart>(Actor)) Entries.Add(Start->GetActorLocation());
		}
		Test->TestTrue(TEXT("TestMG has room bounds"), RoomBounds.IsValid != 0);
		Test->TestTrue(TEXT("TestMG has an entry"), !Entries.IsEmpty());
		if (!RoomBounds.IsValid || Entries.IsEmpty()) return true;
		if (!bCreatedTestNavigationBounds)
		{
			int32 AuthoredVolumeCount = 0;
			for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It) ++AuthoredVolumeCount;
			Test->TestEqual(TEXT("TestMG has no authored navigation volume"), AuthoredVolumeCount, 0);
			if (AuthoredVolumeCount != 0) return true;
			const FBox NavBounds = RoomBounds.ExpandBy(FVector(500, 500, 500));
			FActorSpawnParameters Params;
			Params.OverrideLevel = World->PersistentLevel;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(
				ANavMeshBoundsVolume::StaticClass(), NavBounds.GetCenter(), FRotator::ZeroRotator, Params);
			UBrushComponent* Brush = Volume ? Volume->GetBrushComponent() : nullptr;
			UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
			Test->TestTrue(TEXT("Test navigation volume and system exist"), Brush && Nav);
			if (!Brush || !Nav) return true;
			UBodySetup* Body = NewObject<UBodySetup>(Brush);
			FKBoxElem Box;
			Box.X = NavBounds.GetSize().X;
			Box.Y = NavBounds.GetSize().Y;
			Box.Z = NavBounds.GetSize().Z;
			Body->AggGeom.BoxElems.Add(Box);
			Brush->BrushBodySetup = Body;
			Brush->UpdateBounds();
			Nav->OnNavigationBoundsUpdated(Volume);
			bCreatedTestNavigationBounds = true;
			return false;
		}
		UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
		bool bHasNavigablePoint = false;
		for (AActor* Actor : Definition->GetLevel()->Actors)
			if (const AHBEnemySpawnPoint* Point = Cast<AHBEnemySpawnPoint>(Actor); IsValid(Point))
			{
				FNavLocation OnNav;
				if (Nav && Nav->ProjectPointToNavigation(Point->GetActorLocation(), OnNav, FVector(150, 150, 350)))
				{ bHasNavigablePoint = true; break; }
			}
		if (!bHasNavigablePoint || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
		{
			if (Elapsed < 60.) return false;
			Test->AddError(TEXT("TestMG runtime navigation query did not become ready."));
			return true;
		}
		FString Error;
		if (!Subsystem->StartEncounter(1, 12345, 7, Definition->GetLevel(), RoomBounds, Entries, Error))
		{
			Test->AddError(FString::Printf(TEXT("TestMG real enemy spawn failed: %s"), *Error));
			return true;
		}
		Test->TestTrue(TEXT("Encounter prepared for this room"),
			Subsystem->IsPreparedFor(1, 7, Definition->GetLevel()));
		TArray<AEnemyCharacterBase*> Spawned;
		int32 CommonCount = 0, EliteCount = 0;
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
				if (StateTree) Test->TestTrue(TEXT("Spawned monster StateTree runs after possession"), StateTree->IsRunning());
			}
			Test->TestNotNull(TEXT("Spawned monster has an ASC"), Enemy->GetEnemyASC());
			Test->TestFalse(TEXT("Spawned monster starts alive"), Enemy->IsDead());
			auto MatchesGrade = [Setup, Enemy](EHBEncounterGrade Grade)
			{
				return Setup->Encounter->Groups.ContainsByPredicate([Grade, Enemy](const FHBEncounterGroup& Group)
				{
					return Group.Grade == Grade && Group.Monsters.ContainsByPredicate(
						[Enemy](const FHBEncounterMonsterOption& Option) { return Option.EnemyClass.Get() == Enemy->GetClass(); });
				});
			};
			if (MatchesGrade(EHBEncounterGrade::Elite)) ++EliteCount;
			else if (MatchesGrade(EHBEncounterGrade::Common)) { ++CommonCount; CommonToKill = Enemy; }
			else Test->AddError(FString::Printf(TEXT("Unexpected monster class: %s"), *Enemy->GetClass()->GetName()));
		}
		Test->TestTrue(TEXT("One or two common monsters spawned"), CommonCount >= 1 && CommonCount <= 2);
		Test->TestEqual(TEXT("Stone Golem elite spawned once"), EliteCount, 1);
		Test->TestEqual(TEXT("Alive count matches real actors"),
			Subsystem->GetAliveCount(1, 7, Definition->GetLevel()), Spawned.Num());
		if (CommonToKill)
		{
			const int32 BeforeDeath = Subsystem->GetAliveCount(1, 7, Definition->GetLevel());
			CommonToKill->BecomeDead();
			Test->TestTrue(TEXT("Real enemy death applied"), CommonToKill->IsDead());
			Test->TestEqual(TEXT("Death decrements alive count once"),
				Subsystem->GetAliveCount(1, 7, Definition->GetLevel()), BeforeDeath - 1);
			CommonToKill->BecomeDead();
			Test->TestEqual(TEXT("Repeated death does not double count"),
				Subsystem->GetAliveCount(1, 7, Definition->GetLevel()), BeforeDeath - 1);
		}
		Subsystem->ClearGeneration(1);
		Test->TestEqual(TEXT("Cleanup clears alive count"), Subsystem->GetAliveCount(1, 7, Definition->GetLevel()), 0);
		Test->TestFalse(TEXT("Cleanup clears generation state"), Subsystem->IsPreparedFor(1, 7, Definition->GetLevel()));
		for (AEnemyCharacterBase* Enemy : Spawned)
			Test->TestTrue(TEXT("Cleanup destroys each spawned monster"), !IsValid(Enemy) || Enemy->IsActorBeingDestroyed());
		if (!Subsystem->StartEncounter(2, 12345, 7, Definition->GetLevel(), RoomBounds, Entries, Error))
			Test->AddError(FString::Printf(TEXT("TestMG restart after cleanup failed: %s"), *Error));
		else Test->TestEqual(TEXT("Restart produces the same seeded count"),
			Subsystem->GetAliveCount(2, 7, Definition->GetLevel()), Spawned.Num());
		Subsystem->ClearGeneration(2);
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBTestMGEncounterIntegrationTest, "HeavenBreach.PCGModule.Encounter.TestMGStandalone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBTestMGEncounterIntegrationTest::RunTest(const FString&)
{
	const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestMG");
	if (!FPackageName::DoesPackageExist(Map)) { AddError(TEXT("TestMG fixture map is missing.")); return false; }
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
			if (Context.WorldType == EWorldType::Editor && Context.World())
			{
				AWorldSettings* Settings = Context.World()->GetWorldSettings();
				*PreviousMapMode = Settings->DefaultGameMode;
				Settings->DefaultGameMode = AGameModeBase::StaticClass();
				return true;
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
