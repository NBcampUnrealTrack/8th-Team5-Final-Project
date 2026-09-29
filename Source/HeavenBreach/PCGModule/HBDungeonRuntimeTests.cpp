#include "PCGModule/HBDungeonRuntime.h"
#include "Misc/AutomationTest.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "PCGModule/HBDungeonDoorSeal.h"
#include "PCGModule/HBDungeonPortal.h"
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBEncounterSubsystem.h"
#include "PCGModule/HBEncounterSetupActor.h"
#include "PCGPrototype/HBPCGPreview.h"
#include "AI/EnemyCharacterBase.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"

namespace
{
	constexpr TCHAR TestMGMapPath[] = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestMG");

	struct FTestMGRuntimeFixtureState
	{
		TArray<TSoftObjectPtr<UWorld>> ModuleMaps;
		int32 Seed = 0;
		bool bRandomSeedOnStart = true;
		bool bConfigured = false;
	};

	AHBDungeonRuntime* FindEditorDungeon()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Editor && Context.World())
				for (TActorIterator<AHBDungeonRuntime> It(Context.World()); It; ++It)
					return *It;
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBGameplayGateTest, "HeavenBreach.PCGModule.Gameplay.SpawnGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBGameplayGateTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Mode = World->SpawnActor<AHeavenBreachGameMode>();
    auto* Dungeon = World->SpawnActor<AHBDungeonRuntime>();
    auto* PC = World->SpawnActor<APlayerController>();
    World->SpawnActor<APlayerStart>(); // Persistent-level fallback must never be used.
    for (const auto State : {EHBDungeonRuntimeState::Idle, EHBDungeonRuntimeState::Generating,
        EHBDungeonRuntimeState::Loading, EHBDungeonRuntimeState::PreparingEncounters,
        EHBDungeonRuntimeState::Failed})
    {
        Dungeon->State = State;
        TestNull(TEXT("No start exposed until dungeon ready"), Mode->ChoosePlayerStart_Implementation(PC));
        Mode->RestartPlayer(PC);
        TestNull(TEXT("No premature gameplay pawn"), PC->GetPawn());
        Mode->StartMatch();
        TestFalse(TEXT("Direct StartMatch cannot bypass readiness"), Mode->IsMatchInProgress());
    }
    TestFalse(TEXT("Runtime generator does not inherit preview"), Dungeon->IsA<AHBPCGPreviewGenerator>());
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}

class FHBWaitForGameplayDungeon : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
public:
    explicit FHBWaitForGameplayDungeon(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
        if (!World)
        {
            if (FPlatformTime::Seconds() - Started < 90) return false;
            Test->AddError(TEXT("PIE world did not start.")); return true;
        }
        AHBDungeonRuntime* Dungeon = nullptr;
        for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It) { Dungeon = *It; break; }
        if (!Dungeon) { Test->AddError(TEXT("Gameplay fixture is missing runtime generator.")); return true; }
        // Supply the same validated starting loadout the lobby would pass in.
        // This isolates PCG from the existing direct-PIE missing-loadout fallback.
        if (!Dungeon->IsReady())
        {
            auto* PC = World->GetFirstPlayerController();
            auto* PS = PC ? PC->GetPlayerState<AHB_PlayerState>() : nullptr;
            const auto* GM = World->GetAuthGameMode();
            const auto* CDO = GM && GM->DefaultPawnClass ? Cast<AHBPlayerCharacter>(GM->DefaultPawnClass->GetDefaultObject()) : nullptr;
            if (PS && !PS->GetSelectedJob() && CDO && CDO->GetJobData())
            {
                FHBStartingLoadout Loadout; FText Error;
                if (!MakeDefaultStartingLoadout(CDO->GetJobData(), Loadout, Error) || !PS->TrySetStartingLoadout(Loadout, Error))
                    Test->AddError(Error.ToString());
            }
        }
        if (Dungeon->State == EHBDungeonRuntimeState::Failed) { Test->AddError(Dungeon->Status); return true; }
        if (!Dungeon->IsReady())
        {
            if (FPlatformTime::Seconds() - Started < 90) return false;
            Test->AddError(TEXT("Gameplay dungeon never became ready.")); return true;
        }
        auto* Mode = Cast<AHeavenBreachGameMode>(World->GetAuthGameMode());
        Test->TestNotNull(TEXT("Main game mode is used"), Mode);
        if (!Mode) return true;
        Test->TestTrue(TEXT("Single player starts match after dungeon ready"), Mode->IsMatchInProgress());
        auto* PC = World->GetFirstPlayerController();
        auto* Pawn = PC ? Cast<AHBPlayerCharacter>(PC->GetPawn()) : nullptr;
        Test->TestNotNull(TEXT("Real HB player character is possessed"), Pawn);
        Test->TestNotNull(TEXT("Start point belongs to generated start room"), Dungeon->GetPlayerStart());
        if (Pawn && Dungeon->GetPlayerStart())
            Test->TestTrue(TEXT("Character spawned at authored start"), Pawn->GetActorLocation().Equals(Dungeon->GetPlayerStart()->GetActorLocation(), 10));
        int32 ExpectedSeals = 0, ActualSeals = 0;
        for (const auto& Room : Dungeon->Rooms) ExpectedSeals += Room.UnusedConnectorIds.Num();
        for (TActorIterator<AHBDungeonDoorSeal> It(World); It; ++It) ++ActualSeals;
        Test->TestEqual(TEXT("Unused connectors sealed"), ActualSeals, ExpectedSeals);
        for (TActorIterator<AHBPCGExplorerPawn> It(World); It; ++It) Test->AddError(TEXT("Preview pawn leaked into gameplay."));
        auto* State = World->GetGameState<AHeavenBreachGameState>();
        Test->TestTrue(TEXT("Match timer starts after preparation"), State && State->GetRemainingMatchTime() > 0);
        AHBDungeonPortal* Source = nullptr;
        for (TActorIterator<AHBDungeonPortal> It(World); It; ++It)
            if (IsValid(It->TargetPortal)) { Source = *It; break; }
        Test->TestNotNull(TEXT("Runtime portals linked"), Source);
        if (Pawn && Source)
        {
            Pawn->SetActorLocation(Source->Trigger->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
            Test->TestTrue(TEXT("Real gameplay character can traverse portal"), Source->TryTeleport(Pawn));
            Test->TestTrue(TEXT("Character reached authored arrival"), Pawn->GetActorLocation().Equals(Source->TargetPortal->Arrival->GetComponentLocation(), 1));
        }
        return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBGameplaySmokeTest, "HeavenBreach.PCGModule.Gameplay.StreamSpawnAndPortal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBGameplaySmokeTest::RunTest(const FString&)
{
    const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestPCG_Gameplay");
    if (!FPackageName::DoesPackageExist(Map))
    { AddError(TEXT("Create gameplay smoke fixture with Tools/PCGPrototype/author_gameplay_smoke.py.")); return false; }
    // Existing project input asset has a null trigger; unrelated to PCG. Keep the
    // exact known diagnostic visible/expected without masking dungeon errors.
    AddExpectedError(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
    EPlayNetMode PreviousMode;
    int32 PreviousClients;
    GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
    GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
    // Headless test uses one standalone PIE client, never an external server.
    GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
    GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForGameplayDungeon(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([PreviousMode, PreviousClients]()
    {
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
        GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
        return true;
    }));
    return true;
}

class FHBWaitForTestMGRuntime : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	TSharedRef<FTestMGRuntimeFixtureState> Fixture;
	double Started = FPlatformTime::Seconds();
	double ReadyAt = 0;
	int32 InitialAlive = INDEX_NONE;

public:
	FHBWaitForTestMGRuntime(FAutomationTestBase* InTest, TSharedRef<FTestMGRuntimeFixtureState> InFixture)
		: Test(InTest), Fixture(InFixture) {}
	virtual bool Update() override
	{
		if (!Fixture->bConfigured) return true;
		UWorld* World = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE) { World = Context.World(); break; }
		if (!World)
		{
			if (FPlatformTime::Seconds() - Started < 90.) return false;
			Test->AddError(TEXT("TestMG runtime PIE world did not start.")); return true;
		}
		AHBDungeonRuntime* Dungeon = nullptr;
		for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It) { Dungeon = *It; break; }
		if (!Dungeon) { Test->AddError(TEXT("TestMG runtime fixture has no dungeon generator.")); return true; }
		if (!Dungeon->IsReady())
		{
			// Direct PIE has no lobby; provide the same default loadout as the existing gameplay smoke test.
			APlayerController* PC = World->GetFirstPlayerController();
			AHB_PlayerState* PS = PC ? PC->GetPlayerState<AHB_PlayerState>() : nullptr;
			const AGameModeBase* GM = World->GetAuthGameMode();
			const AHBPlayerCharacter* CDO = GM && GM->DefaultPawnClass
				? Cast<AHBPlayerCharacter>(GM->DefaultPawnClass->GetDefaultObject()) : nullptr;
			if (PS && !PS->GetSelectedJob() && CDO && CDO->GetJobData())
			{
				FHBStartingLoadout Loadout; FText Error;
				if (!MakeDefaultStartingLoadout(CDO->GetJobData(), Loadout, Error)
					|| !PS->TrySetStartingLoadout(Loadout, Error)) Test->AddError(Error.ToString());
			}
		}
		if (Dungeon->State == EHBDungeonRuntimeState::Failed) { Test->AddError(Dungeon->Status); return true; }
		if (!Dungeon->IsReady())
		{
			if (FPlatformTime::Seconds() - Started < 90.) return false;
			Test->AddError(TEXT("TestMG runtime dungeon did not become ready.")); return true;
		}
		if (ReadyAt != 0. && FPlatformTime::Seconds() - ReadyAt < 0.5) return false;

		int32 RoomCount = 0;
		int32 TotalAlive = 0;
		UHBEncounterSubsystem* EncounterSubsystem = World->GetSubsystem<UHBEncounterSubsystem>();
		Test->TestNotNull(TEXT("Gameplay world has an encounter subsystem"), EncounterSubsystem);
		if (!EncounterSubsystem) return true;
		for (const FHBPrototypeRoom& Room : Dungeon->Rooms)
		{
			if (Room.ModuleMap.ToSoftObjectPath().GetLongPackageName() != TestMGMapPath) continue;
			++RoomCount;
			ULevel* Level = Dungeon->GetLoadedRoomLevel(Room.Id);
			Test->TestNotNull(TEXT("Generated TestMG room has a loaded level"), Level);
			if (!Level) continue;
			AHBEncounterSetupActor* Setup = nullptr;
			for (AActor* Actor : Level->Actors)
				if (auto* Candidate = Cast<AHBEncounterSetupActor>(Actor)) { Setup = Candidate; break; }
			Test->TestNotNull(TEXT("Generated TestMG room has an encounter setup"), Setup);
			if (!Setup || !Setup->Encounter) continue;
			Test->TestTrue(TEXT("TestMG encounter prepared before dungeon Ready"),
				EncounterSubsystem->IsPreparedFor(Dungeon->GenerationId, Room.Id, Level));
			int32 Common = 0, Elite = 0;
			for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It)
			{
				AEnemyCharacterBase* Enemy = *It;
				if (Enemy->GetLevel() != Level) continue;
				auto MatchesGrade = [Setup, Enemy](EHBEncounterGrade Grade)
				{
					return Setup->Encounter->Groups.ContainsByPredicate([Grade, Enemy](const FHBEncounterGroup& Group)
					{
						return Group.Grade == Grade && Group.Monsters.ContainsByPredicate(
							[Enemy](const FHBEncounterMonsterOption& Option) { return Option.EnemyClass.Get() == Enemy->GetClass(); });
					});
				};
				if (MatchesGrade(EHBEncounterGrade::Elite)) ++Elite;
				else if (MatchesGrade(EHBEncounterGrade::Common)) ++Common;
				else Test->AddError(FString::Printf(TEXT("Unexpected TestMG monster: %s"), *Enemy->GetClass()->GetName()));
			}
			Test->TestTrue(TEXT("TestMG has one or two common monsters"), Common >= 1 && Common <= 2);
			Test->TestEqual(TEXT("TestMG has one Stone Golem elite"), Elite, 1);
			Test->TestEqual(TEXT("TestMG server alive count matches spawned monsters"),
				EncounterSubsystem->GetAliveCount(Dungeon->GenerationId, Room.Id, Level), Common + Elite);
			TotalAlive += EncounterSubsystem->GetAliveCount(Dungeon->GenerationId, Room.Id, Level);
		}
		Test->TestTrue(TEXT("Actual dungeon selected TestMG"), RoomCount > 0);
		if (ReadyAt == 0.) { ReadyAt = FPlatformTime::Seconds(); InitialAlive = TotalAlive; return false; }
		if (FPlatformTime::Seconds() - ReadyAt < 0.5) return false;
		Test->TestEqual(TEXT("Ready does not trigger a duplicate TestMG spawn"), TotalAlive, InitialAlive);
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBTestMGRuntimeTest, "HeavenBreach.PCGModule.Encounter.TestMGRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBTestMGRuntimeTest::RunTest(const FString&)
{
	const FString FixtureMap = TEXT("/Game/Levels/Test_CW/GameplaySmoke/TestPCG_Gameplay");
	if (!FPackageName::DoesPackageExist(FixtureMap) || !FPackageName::DoesPackageExist(TestMGMapPath))
	{ AddError(TEXT("TestMG or gameplay fixture map is missing.")); return false; }
	AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
	EPlayNetMode PreviousMode;
	int32 PreviousClients;
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
	const TSharedRef<FTestMGRuntimeFixtureState> Saved = MakeShared<FTestMGRuntimeFixtureState>();
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(FixtureMap));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Saved]()
	{
		AHBDungeonRuntime* Dungeon = FindEditorDungeon();
		if (!Dungeon) { AddError(TEXT("Gameplay fixture has no editor dungeon generator.")); return true; }
		Saved->ModuleMaps = Dungeon->ModuleMaps;
		Saved->Seed = Dungeon->Seed;
		Saved->bRandomSeedOnStart = Dungeon->bRandomSeedOnStart;
		Dungeon->ModuleMaps.Add(TSoftObjectPtr<UWorld>(FSoftObjectPath(FString(TestMGMapPath) + TEXT(".TestMG"))));
		TArray<FHBModuleCatalogEntry> Catalog;
		TArray<FString> CatalogErrors;
		if (!HBDungeonModuleCatalog::Build(Dungeon->ModuleMaps, Catalog, CatalogErrors))
		{
			AddError(FString::Join(CatalogErrors, TEXT("\n")));
			return true;
		}
		HBConnectorLayout::FSettings Settings;
		Settings.Origin = Dungeon->GetActorTransform();
		Settings.bAllowModuleReuse = Dungeon->bAllowModuleReuse;
		Settings.MaxSearchSteps = Dungeon->MaxConnectionAttempts;
		Settings.MaxSearchSeconds = Dungeon->ConnectionSearchSeconds;
		int32 SelectedSeed = INDEX_NONE;
		for (int32 Candidate = 1; Candidate <= 64; ++Candidate)
		{
			Settings.Seed = Candidate;
			TArray<FHBPrototypeRoom> Rooms;
			FString Error;
			if (HBDungeonRuleLayout::Generate(Catalog, Settings, Dungeon->DungeonRules, Rooms, Error)
				&& Rooms.ContainsByPredicate([](const FHBPrototypeRoom& Room)
				{
					return Room.ModuleMap.ToSoftObjectPath().GetLongPackageName() == TestMGMapPath;
				}))
			{
				SelectedSeed = Candidate;
				break;
			}
		}
		if (SelectedSeed == INDEX_NONE)
		{
			AddError(TEXT("No fixture seed from 1 to 64 selected TestMG."));
			return true;
		}
		Dungeon->Seed = SelectedSeed;
		Dungeon->bRandomSeedOnStart = false;
		Saved->bConfigured = true;
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForTestMGRuntime(this, Saved));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Saved, PreviousMode, PreviousClients]()
	{
		if (AHBDungeonRuntime* Dungeon = FindEditorDungeon())
		{
			Dungeon->ModuleMaps = Saved->ModuleMaps;
			Dungeon->Seed = Saved->Seed;
			Dungeon->bRandomSeedOnStart = Saved->bRandomSeedOnStart;
		}
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBTestMGGameplayMapTest, "HeavenBreach.PCGModule.Encounter.TestMGGameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBTestMGGameplayMapTest::RunTest(const FString&)
{
	const FString Map = TEXT("/Game/Levels/Test_CW/GameplaySmoke/Test_MGGamePlay");
	if (!FPackageName::DoesPackageExist(Map) || !FPackageName::DoesPackageExist(TestMGMapPath))
	{ AddError(TEXT("Test_MGGamePlay or TestMG map is missing.")); return false; }
	AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Could not find context actor of type EnemyCharacterBase"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("Missing external data requirements"), EAutomationExpectedErrorFlags::Contains, 0);
	EPlayNetMode PreviousMode;
	int32 PreviousClients;
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNetMode(PreviousMode);
	GetDefault<ULevelEditorPlaySettings>()->GetPlayNumberOfClients(PreviousClients);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
	GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(1);
	const TSharedRef<FTestMGRuntimeFixtureState> Fixture = MakeShared<FTestMGRuntimeFixtureState>();
	Fixture->bConfigured = true;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		AHBDungeonRuntime* Dungeon = FindEditorDungeon();
		TestNotNull(TEXT("Test_MGGamePlay has a dungeon runtime"), Dungeon);
		if (Dungeon)
		{
			TestFalse(TEXT("Test_MGGamePlay uses a fixed seed"), Dungeon->bRandomSeedOnStart);
			TestEqual(TEXT("Test_MGGamePlay tested seed"), Dungeon->Seed, 3);
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FHBWaitForTestMGRuntime(this, Fixture));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([PreviousMode, PreviousClients]()
	{
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNetMode(PreviousMode);
		GetMutableDefault<ULevelEditorPlaySettings>()->SetPlayNumberOfClients(PreviousClients);
		return true;
	}));
	return true;
}
#endif
