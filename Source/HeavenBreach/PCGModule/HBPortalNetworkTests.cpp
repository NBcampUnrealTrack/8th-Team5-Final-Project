#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "PCGModule/HBDungeonRuntime.h"
#include "PCGModule/HBDungeonRuntimeValidation.h"
#include "PCGModule/HBDungeonModuleDefinition.h"
#include "PCGModule/HBDungeonMarkerActors.h"
#include "PCGModule/HBDungeonOccupancyComponent.h"
#include "PCGModule/HBRoomInfoComponent.h"
#include "PCGModule/HBPortalSpawnPoint.h"
#include "PCGModule/HBDungeonPortal.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Escape/ExtractionZone.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Editor.h"
#include "PCGGraph.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
    AHBDungeonRuntime* DungeonIn(UWorld* World)
    {
        if (World) for (TActorIterator<AHBDungeonRuntime> It(World); It; ++It) return *It;
        return nullptr;
    }
    struct FPortalNetworkFixture
    {
        FString Root = TEXT("/HBPortalNetwork_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("/");
        FString Directory;
        TArray<TStrongObjectPtr<UWorld>> Modules;
        TArray<TSoftObjectPtr<UWorld>> Maps;
        TArray<FString> Files;
        EPlayNetMode SavedMode;
        int32 SavedClients = 0;
        bool bSavedOneProcess = false;
        bool bSavedSeparateServer = false;
        TArray<FNetDriverDefinition> SavedDrivers;
        TSubclassOf<APawn> SavedPawnClass;
        bool bSavedAutoStart = true;
        bool bConfigured = false;

        bool MakeModules(FAutomationTestBase* Test)
        {
            Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / Root.Mid(1, Root.Len() - 2));
            IFileManager::Get().MakeDirectory(*Directory, true);
            FPackageName::RegisterMountPoint(Root, Directory + TEXT("/"));
            for (int32 Index = 0; Index < 4; ++Index)
            {
                const FString Name = FString::Printf(TEXT("Room%d"), Index);
                UPackage* Package = CreatePackage(*(Root + Name)); Package->SetPackageFlags(PKG_ContainsMap);
                UWorld* World = UWorld::CreateWorld(EWorldType::Inactive, false, FName(*Name), Package, false);
                World->ClearFlags(RF_Transient); World->SetFlags(RF_Public | RF_Standalone);
                Modules.Emplace(World);
                auto* Definition = World->SpawnActor<AHBDungeonModuleDefinition>();
                Definition->ModuleId = FName(*Name);
                Definition->ModuleKind = Index == 3 ? EHBDungeonModuleKind::Corridor : EHBDungeonModuleKind::Room;
                Definition->RoomInfo->RoomRole = Index == 2 ? EHBPrototypeRoomRole::Boss
                    : Index == 3 ? EHBPrototypeRoomRole::Unassigned : EHBPrototypeRoomRole::Adventure;
                Definition->RoomInfo->AllowedFloor = Index == 0 ? EHBAllowedFloor::Any : EHBAllowedFloor::Floor2;
                Definition->RoomInfo->bIsStartCandidate = Index == 0;
                auto* Occupancy = World->SpawnActor<AHBDungeonOccupancy>(FVector(0, 0, 200), FRotator::ZeroRotator);
                Occupancy->Occupancy->SetBoxExtent(FVector(1500, 1500, 250));
                auto* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0, 0, -10), FRotator::ZeroRotator);
                Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
                Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
                Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
                Floor->SetActorScale3D(FVector(30, 30, 0.2));
                if (Index < 2)
                {
                    auto* Extraction = World->SpawnActor<AExtractionZone>(FVector(-700, 600, 100), FRotator::ZeroRotator);
                    FindFProperty<FBoolProperty>(Extraction->GetClass(), TEXT("bExtractionCandidate"))->SetPropertyValue_InContainer(Extraction, true);
                }
                for (int32 Door = 0; Door < 4; ++Door)
                {
                    const FRotator Rotation(0, Door * 90, 0);
                    auto* Connector = World->SpawnActor<AHBDungeonConnector>(Rotation.Vector() * 1500., Rotation);
                    Connector->Connector->ConnectorId = FName(*FString::Printf(TEXT("D%d"), Door));
                    if (Index == 2) Connector->Connector->Role = EHBDungeonConnectorRole::BossEntrance;
                    if (Index == 3 && Door == 0) Connector->Connector->Role = EHBDungeonConnectorRole::DungeonSide;
                    if (Index == 3 && Door == 2) Connector->Connector->Role = EHBDungeonConnectorRole::BossSide;
                }
                World->SpawnActor<APlayerStart>(FVector(-700, 0, 100), FRotator::ZeroRotator);
                for (int32 PointIndex = 0; PointIndex < 2; ++PointIndex)
                {
                    auto* Point = World->SpawnActor<AHBPortalSpawnPoint>(FVector(0, PointIndex == 0 ? -600 : 600, 0), FRotator(0, PointIndex * 180, 0));
                    Point->PointId = FName(*FString::Printf(TEXT("P%d"), PointIndex));
                }
                TArray<FHBDungeonModuleIssue> Issues;
                if (!AHBDungeonModuleDefinition::ValidateLevel(World->PersistentLevel, Issues))
                { for (const auto& Issue : Issues) Test->AddError(Issue.Message); return false; }
                const FString File = Directory / (Name + TEXT(".umap"));
                FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
                if (!UPackage::SavePackage(Package, World, *File, Args))
                { Test->AddError(TEXT("Could not save temporary portal network module.")); return false; }
                Files.Add(File); Maps.Add(TSoftObjectPtr<UWorld>(World));
            }
            return true;
        }
        void Restore()
        {
            GEngine->NetDriverDefinitions = SavedDrivers;
            auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
            Settings->SetPlayNetMode(SavedMode); Settings->SetPlayNumberOfClients(SavedClients);
            Settings->SetRunUnderOneProcess(bSavedOneProcess); Settings->bLaunchSeparateServer = bSavedSeparateServer;
            Settings->SaveConfig();
            auto* Mode = GetMutableDefault<AHeavenBreachGameMode>();
            Mode->DefaultPawnClass = SavedPawnClass;
            FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutoStartMatch"))->SetPropertyValue_InContainer(Mode, bSavedAutoStart);
            for (auto& World : Modules) World->DestroyWorld(false);
            Modules.Reset();
            FPackageName::UnRegisterMountPoint(Root, Directory + TEXT("/"));
            for (const FString& File : Files)
                if (FPaths::IsUnderDirectory(File, Directory)) IFileManager::Get().Delete(*File, false, true);
            if (!Directory.IsEmpty()) IFileManager::Get().DeleteDirectory(*Directory, false, false);
        }
    };

    class FWaitForRandomPortalNetwork : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        TSharedRef<FPortalNetworkFixture> Fixture;
        double Started = FPlatformTime::Seconds();
        double PhaseStarted = Started;
        int32 Phase = 0;
        int32 Generation = 0;
        TWeakObjectPtr<ACharacter> Pawn;
        TWeakObjectPtr<APlayerController> Remote;
        TArray<TWeakObjectPtr<AHBDungeonPortal>> Retired;
        FHBDungeonPortalPairSnapshot TravelPair;

        AHBDungeonPortal* PortalIn(AHBDungeonRuntime* Dungeon, const FHBDungeonPortalEndpointSnapshot& End)
        {
            FString Error;
            return AHBDungeonPortal::FindInLevel(Dungeon->GetLoadedRoomLevel(End.RoomId),
                FName(*FString::Printf(TEXT("PCG_R%d_%s"), End.RoomId, *End.PointId.ToString())), Error);
        }
        bool Verify(UWorld* Server, const TArray<UWorld*>& Clients)
        {
            auto* Host = DungeonIn(Server);
            const auto& Snapshot = Server->GetGameState<AHeavenBreachGameState>()->GetDungeonSnapshot();
            for (UWorld* Client : Clients)
            {
                auto* Local = DungeonIn(Client);
                auto* State = Client->GetGameState<AHeavenBreachGameState>();
                if (!Local || !Local->IsReady() || !State || !State->GetDungeonSnapshot().bPortalPairsReady
                    || Local->GetGenerationId() != Snapshot.GenerationId) return false;
                Test->TestEqual(TEXT("Client retains every final server pair"), State->GetDungeonSnapshot().PortalPairs.Num(), 2);
                for (const auto& Pair : Snapshot.PortalPairs)
                {
                    auto* A = PortalIn(Local, Pair.Floor1); auto* B = PortalIn(Local, Pair.Floor2);
                    if (!A || !B) { Test->AddError(TEXT("Client is ready without selected portal actors.")); return false; }
                    Test->TestTrue(TEXT("Client portals match final server placement and reciprocal pairing"),
                        A->TargetPortal == B && B->TargetPortal == A && !A->GetIsReplicated() && !B->GetIsReplicated()
                        && A->GetActorTransform().Equals(Pair.Floor1.ActorTransform, 0.1)
                        && B->Arrival->GetComponentTransform().Equals(Pair.Floor2.ArrivalTransform, 0.1));
                }
            }
            return Host && Host->IsReady();
        }
        void Capture(AHBDungeonRuntime* Dungeon)
        {
            for (const auto& Room : Dungeon->Rooms)
                for (AActor* Actor : Dungeon->GetLoadedRoomLevel(Room.Id)->Actors)
                    if (auto* Portal = Cast<AHBDungeonPortal>(Actor); IsValid(Portal) && !Portal->IsActorBeingDestroyed()) Retired.Add(Portal);
        }
        void Next(int32 Value) { Phase = Value; PhaseStarted = FPlatformTime::Seconds(); }
    public:
        FWaitForRandomPortalNetwork(FAutomationTestBase* InTest, TSharedRef<FPortalNetworkFixture> InFixture) : Test(InTest), Fixture(InFixture) {}
        bool Update() override
        {
            if (!Fixture->bConfigured) return true;
            if (FPlatformTime::Seconds() - PhaseStarted > 90.)
            { Test->AddError(FString::Printf(TEXT("Random portal network timed out in phase %d."), Phase)); return true; }
            UWorld* Server = nullptr; TArray<UWorld*> Clients;
            for (const auto& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::PIE && Context.World())
                {
                    if (Context.World()->GetNetMode() == NM_ListenServer) Server = Context.World();
                    if (Context.World()->GetNetMode() == NM_Client) Clients.Add(Context.World());
                }
            if (!Server || Clients.IsEmpty()) return false;
            auto* Host = DungeonIn(Server); auto* Mode = Server->GetAuthGameMode<AHeavenBreachGameMode>();
            if (!Host || !Mode) return false;
            // PIE may reconstruct native defaults; apply the test's waiting-room
            // policy to the live mode before client acknowledgements arrive.
            FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutoStartMatch"))->SetPropertyValue_InContainer(Mode, false);
            Mode->DefaultPawnClass = ACharacter::StaticClass();
            for (UWorld* World : Clients)
                if (auto* Local = DungeonIn(World); Local && Local->State == EHBDungeonRuntimeState::Failed)
                { Test->AddError(Local->Status); return true; }
            if (Host->State == EHBDungeonRuntimeState::Failed) { Test->AddError(Host->Status); return true; }
            if (Phase == 0)
            {
                if (!Host->IsReady() || !Verify(Server, Clients) || !Mode->AreDungeonPlayersLoaded()) return false;
                Generation = Host->GetGenerationId();
                for (auto It = Server->GetPlayerControllerIterator(); It; ++It)
                    if (It->IsValid() && !It->Get()->IsLocalController()) { Remote = It->Get(); break; }
                if (!Remote.IsValid()) return false;
                const auto& Snapshot = Server->GetGameState<AHeavenBreachGameState>()->GetDungeonSnapshot();
                TravelPair = Snapshot.PortalPairs[0];
                Pawn = Server->SpawnActor<ACharacter>(TravelPair.Floor1.ActivationLocation, FRotator::ZeroRotator);
                Remote->Possess(Pawn.Get()); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
                for (const auto& Pair : Snapshot.PortalPairs)
                { PortalIn(Host, Pair.Floor1)->SetActorTickEnabled(false); PortalIn(Host, Pair.Floor2)->SetActorTickEnabled(false); }
                Test->TestTrue(TEXT("Remote player traverses a server-selected random portal"), PortalIn(Host, TravelPair.Floor1)->TryTeleport(Pawn.Get()));
                Next(1); return false;
            }
            if (Phase == 1 || Phase == 3)
            {
                ACharacter* LocalPawn = Cast<ACharacter>(Clients[0]->GetFirstPlayerController()->GetPawn());
                const auto& Destination = Phase == 1 ? TravelPair.Floor2.ArrivalTransform : TravelPair.Floor1.ArrivalTransform;
                if (!LocalPawn || !LocalPawn->GetActorLocation().Equals(Destination.GetLocation(), 20.)) return false;
                Test->TestTrue(TEXT("Owning client receives portal position and facing"),
                    Clients[0]->GetFirstPlayerController()->GetControlRotation().Equals(Destination.Rotator(), 1.));
                if (Phase == 1)
                { Pawn->SetActorLocation(TravelPair.Floor2.ArrivalTransform.GetLocation()); Next(2); return false; }
                Remote->UnPossess(); Pawn->Destroy(); GEditor->RequestLateJoin(); Next(4); return false;
            }
            if (Phase == 2)
            {
                PortalIn(Host, TravelPair.Floor2)->Tick(0);
                if (FPlatformTime::Seconds() - PhaseStarted < 0.6) return false;
                Pawn->SetActorLocation(TravelPair.Floor2.ActivationLocation);
                Test->TestTrue(TEXT("Remote player returns through the same paired portal"), PortalIn(Host, TravelPair.Floor2)->TryTeleport(Pawn.Get()));
                Next(3); return false;
            }
            if (Phase == 4)
            {
                if (Clients.Num() != 2 || !Verify(Server, Clients) || !Mode->AreDungeonPlayersLoaded()) return false;
                Test->TestTrue(TEXT("Late join receives final pair data without any server republish"), Host->GetGenerationId() == Generation);
                Capture(Host); for (auto* World : Clients) Capture(DungeonIn(World));
                if (!Test->TestTrue(TEXT("Random portal generation can be reset and recreated"), Mode->RegenerateDungeon())) return true;
                Next(5); return false;
            }
            if (Phase == 5)
            {
                if (Host->GetGenerationId() <= Generation || !Host->IsReady() || !Verify(Server, Clients) || !Mode->AreDungeonPlayersLoaded()) return false;
                for (const auto& Old : Retired)
                    if (Old.IsValid() && !Old->IsActorBeingDestroyed()) return false;
                Test->TestEqual(TEXT("Previous-generation portals were checked in all three worlds"), Retired.Num(), 12);
                Generation = Host->GetGenerationId(); Retired.Reset();
                Capture(Host); for (auto* World : Clients) Capture(DungeonIn(World));
                Test->TestTrue(TEXT("Explicit generation clear succeeds"), Host->ResetGeneration());
                Next(6); return false;
            }
            for (auto* World : Clients)
            {
                auto* Local = DungeonIn(World);
                if (!Local || Local->State != EHBDungeonRuntimeState::Idle || !Local->Rooms.IsEmpty()) return false;
            }
            for (const auto& Old : Retired)
                if (Old.IsValid() && (!Old->IsActorBeingDestroyed() || IsValid(Old->TargetPortal))) return false;
            Test->AddInfo(TEXT("Listen random portals verified final pair replication, owning-client round trip, late join, regeneration and clear."));
            return true;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBPortalListenNetworkTest, "HeavenBreach.PCGModule.Network.RandomPortals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBPortalListenNetworkTest::RunTest(const FString&)
{
    const auto Fixture = MakeShared<FPortalNetworkFixture>();
    auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->GetPlayNetMode(Fixture->SavedMode); Settings->GetPlayNumberOfClients(Fixture->SavedClients);
    Settings->GetRunUnderOneProcess(Fixture->bSavedOneProcess); Fixture->bSavedSeparateServer = Settings->bLaunchSeparateServer;
    Fixture->SavedDrivers = GEngine->NetDriverDefinitions;
    auto* Mode = GetMutableDefault<AHeavenBreachGameMode>(); Fixture->SavedPawnClass = Mode->DefaultPawnClass;
    auto* AutoStart = FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutoStartMatch"));
    Fixture->bSavedAutoStart = AutoStart->GetPropertyValue_InContainer(Mode); AutoStart->SetPropertyValue_InContainer(Mode, false);
    Mode->DefaultPawnClass = ACharacter::StaticClass();
    Settings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer); Settings->SetPlayNumberOfClients(2);
    Settings->SetRunUnderOneProcess(true); Settings->bLaunchSeparateServer = false;
    for (auto& Driver : GEngine->NetDriverDefinitions)
        if (Driver.DefName == NAME_GameNetDriver) { Driver.DriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"); Driver.DriverClassNameFallback = Driver.DriverClassName; }
    if (!Fixture->MakeModules(this)) { Fixture->Restore(); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Engine/Maps/Entry")));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Fixture]()
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        World->GetWorldSettings()->DefaultGameMode = AHeavenBreachGameMode::StaticClass();
        auto* Dungeon = World->SpawnActor<AHBDungeonRuntime>();
        Dungeon->Graph = LoadObject<UPCGGraphInterface>(nullptr, TEXT("/Game/Levels/Test_CW/Prototype/PCG_DungeonPrototype.PCG_DungeonPrototype"));
        Dungeon->ModuleMaps = Fixture->Maps; Dungeon->bRandomSeedOnStart = false; Dungeon->Seed = 7283;
        Dungeon->PortalPlacementMode = EHBDungeonPortalPlacementMode::RandomPoints; Dungeon->PortalPairCount = 2;
        Dungeon->PortalClass = LoadClass<AHBDungeonPortal>(nullptr, TEXT("/Game/Levels/Test_CW/Prototype/Blueprints/BP_HBDungeonPortal.BP_HBDungeonPortal_C"));
        Dungeon->DungeonRules.Floor1ExitMap = Fixture->Maps[0]; Dungeon->DungeonRules.Floor2EntryMap = Fixture->Maps[1];
        Dungeon->DungeonRules.BossCorridorMap = Fixture->Maps[3]; Dungeon->DungeonRules.Floor1MainRooms = 2;
        Dungeon->DungeonRules.Floor2MainRooms = 3; Dungeon->DungeonRules.Floor1BranchRooms = 0;
        Dungeon->DungeonRules.Floor2BranchRooms = 1; Dungeon->DungeonRules.Floor2Offset = FVector(20000, 0, 1200);
        Dungeon->MinExtractionPointsPerFloor = Dungeon->MaxExtractionPointsPerFloor = 0;
        Fixture->bConfigured = Dungeon->Graph && Dungeon->PortalClass;
        if (!Fixture->bConfigured) AddError(TEXT("Portal network test needs the current graph and portal BP."));
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FWaitForRandomPortalNetwork>(this, Fixture));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    const auto EndStarted = MakeShared<double>(0.);
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Fixture, EndStarted]()
    {
        if (*EndStarted == 0.) *EndStarted = FPlatformTime::Seconds();
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE && Context.World())
            {
                if (FPlatformTime::Seconds() - *EndStarted < 30.) return false;
                AddError(TEXT("PIE portal worlds did not shut down.")); return true;
            }
        Fixture->Restore(); return true;
    }));
    return true;
}
#endif
