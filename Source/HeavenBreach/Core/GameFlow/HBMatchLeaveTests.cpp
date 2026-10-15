#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Core/DataManagement/HBPlayerSaveGame.h"
#include "Player/HBPlayerController.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/StashComponent.h"
#include "Inventory/QuickSlotComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace
{
    struct FMatchLeaveFixture
    {
        EPlayNetMode Mode;
        int32 Players;
        bool bOneProcess;
        bool bSeparateServer;
        uint16 Port;
        TArray<FNetDriverDefinition> Drivers;
        FDelegateHandle Initialization;
    };

    class FVerifyMatchLeave : public IAutomationLatentCommand
    {
        FAutomationTestBase* Test;
        FString Case;
        int32 Phase = 0;
        double Started = FPlatformTime::Seconds();
        double StableSince = 0.;
        TWeakObjectPtr<AHBPlayerController> Host;
        TWeakObjectPtr<AHBPlayerController> Guest;
        TArray<FGuid> HostStash, GuestStash;
        FString HostSlot, GuestSlot;

        TArray<FGuid> Seed(AHB_PlayerState* State)
        {
            auto* Inventory = State->GetInventory();
            Inventory->DebugGiveItemByRow(TEXT("WPN_001"), 2);
            Test->TestFalse(TEXT("Fixture supplies actual carried weapons"), Inventory->GetEntries().IsEmpty());
            if (!Inventory->GetEntries().IsEmpty())
                State->GetEquipment()->EquipFromInventory(Inventory, Inventory->GetEntries()[0].Item.InstanceId, EEquipSlotType::PrimaryWeapon);
            if (!Inventory->GetEntries().IsEmpty()) State->GetQuickSlot()->SetSlot(0, Inventory->GetEntries()[0].Item.InstanceId);
            State->GetStash()->DebugGiveItemByRow(TEXT("WPN_001"), 1);
            TArray<FGuid> Result;
            for (const auto& Entry : State->GetStash()->GetEntries()) Result.Add(Entry.Item.InstanceId);
            State->ForceNetUpdate();
            return Result;
        }

        bool VerifyProfile(const FString& Slot, const TArray<FGuid>& ExpectedStash)
        {
            const auto* Save = Cast<UHBPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
            if (!Save) return false;
            Test->TestTrue(TEXT("ESC removes saved inventory"), Save->Inventory.IsEmpty());
            for (const auto& Item : Save->Equipment) Test->TestTrue(TEXT("ESC removes saved equipment and bag contents"), Item.ItemRow.IsNull());
            for (const auto& Id : Save->QuickSlots) Test->TestFalse(TEXT("ESC removes saved quick slot references"), Id.IsValid());
            TArray<FGuid> Stash;
            for (const auto& Entry : Save->Stash) Stash.Add(Entry.Item.InstanceId);
            Test->TestTrue(TEXT("ESC preserves every stash item"), Stash == ExpectedStash);
            Test->TestNotNull(TEXT("ESC preserves the selected job and skills"), Save->StartingLoadout.Job.Get());
            return true;
        }

    public:
        FVerifyMatchLeave(FAutomationTestBase* InTest, FString InCase) : Test(InTest), Case(MoveTemp(InCase)) {}

        bool Update() override
        {
            if (FPlatformTime::Seconds() - Started > 90.)
            { Test->AddError(FString::Printf(TEXT("Match leave fixture timed out at phase %d (%s)"), Phase, *Case)); return true; }
            UWorld* Server = Host.IsValid() ? Host->GetWorld() : nullptr;
            UWorld* Client = Guest.IsValid() ? Guest->GetWorld() : nullptr;
            for (const auto& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
                if (Context.World()->GetNetMode() != NM_Client && Context.World()->GetAuthGameMode<AHeavenBreachGameMode>()) Server = Context.World();
                if (Context.World()->GetNetMode() == NM_Client) Client = Context.World();
            }
            auto* Mode = Server ? Server->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr;
            if (Phase == 0)
            {
                const bool bSolo = Case == TEXT("Solo");
                if (!Server || !Mode || Mode->GetNumPlayers() != (bSolo ? 1 : 2) || (!bSolo && !Client)) return false;
                Host = Cast<AHBPlayerController>(Server->GetFirstPlayerController());
                if (!bSolo) Guest = Cast<AHBPlayerController>(Client->GetFirstPlayerController());
                if (!Host.IsValid() || (!bSolo && (!Guest.IsValid() || !Guest->PlayerState))) return false;
                if (StableSince == 0.) { StableSince = FPlatformTime::Seconds(); return false; }
                if (FPlatformTime::Seconds() - StableSince < 2.) return false;
                const auto* Defaults = Cast<AHBPlayerCharacter>(Mode->DefaultPawnClass->GetDefaultObject());
                for (auto It = Server->GetPlayerControllerIterator(); It; ++It)
                {
                    auto* State = It->Get()->GetPlayerState<AHB_PlayerState>();
                    FHBStartingLoadout Loadout;
                    FText Error;
                    if (!State || !Defaults || !MakeDefaultStartingLoadout(Defaults->GetJobData(), Loadout, Error) || !State->TrySetStartingLoadout(Loadout, Error))
                    { Test->AddError(TEXT("Match leave fixture cannot supply the existing default job.")); return true; }
                }
                Mode->StartMatch();
                Test->TestTrue(TEXT("Fixture starts an actual gameplay match"), Mode->IsMatchInProgress());
                for (auto It = Server->GetPlayerControllerIterator(); It; ++It)
                    if (auto* Pawn = Cast<AHBPlayerCharacter>(It->Get()->GetPawn())) Pawn->GetCharacterMovement()->DisableMovement();
                AHB_PlayerState* RemoteState = nullptr;
                for (auto It = Server->GetPlayerControllerIterator(); It; ++It)
                    if (!It->Get()->IsLocalController()) RemoteState = It->Get()->GetPlayerState<AHB_PlayerState>();
                auto* HostState = Host->GetPlayerState<AHB_PlayerState>();
                if (!HostState || (!bSolo && !RemoteState)) { Test->AddError(TEXT("Connected gameplay player states are missing.")); return true; }
                HostStash = Seed(HostState);
                HostSlot = FString::Printf(TEXT("Profile_TestAccount_%d"), Server->GetOutermost()->GetPIEInstanceID());
                if (!bSolo)
                {
                    GuestStash = Seed(RemoteState);
                    GuestSlot = FString::Printf(TEXT("Profile_TestAccount_%d"), Client->GetOutermost()->GetPIEInstanceID());
                    Test->TestFalse(TEXT("Host cannot leave with another active player"), Mode->CanPlayerLeaveMatch(Host.Get()));
                    Host->ProcessEvent(Host->FindFunctionChecked(TEXT("HandleLeaveMatchConfirmed")), nullptr);
                    Test->TestFalse(TEXT("Rejected host ESC never starts departure"), Host->IsLeaveMatchInProgress());
                    Test->TestFalse(TEXT("Rejected host ESC preserves carried inventory"), HostState->GetInventory()->GetEntries().IsEmpty());
                }
                Phase = 1;
                StableSince = FPlatformTime::Seconds();
                return false;
            }
            if (Phase == 1)
            {
                if (!Mode || !Host.IsValid()) return false;
                if (Case == TEXT("Solo"))
                {
                    const auto* Save = Cast<UHBPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot(HostSlot, 0));
                    if (!Save || Save->Inventory.IsEmpty()) return false;
                    Test->TestTrue(TEXT("A solo player may leave immediately"), Mode->CanPlayerLeaveMatch(Host.Get()));
                    Host->ProcessEvent(Host->FindFunctionChecked(TEXT("HandleLeaveMatchConfirmed")), nullptr);
                    Phase = 3;
                    return false;
                }
                if (!Guest.IsValid()) return false;
                const auto* Previous = Cast<UHBPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot(GuestSlot, 0));
                if (!Previous || Previous->Inventory.IsEmpty() || FPlatformTime::Seconds() - StableSince < 2.) return false;
                auto* RemoteState = Guest->GetPlayerState<AHB_PlayerState>();
                APlayerState* ServerRemote = nullptr;
                for (auto It = Server->GetPlayerControllerIterator(); It; ++It)
                    if (!It->Get()->IsLocalController()) ServerRemote = It->Get()->PlayerState;
                if (Case == TEXT("HostAfterDeath"))
                {
                    Mode->NotifyPlayerDied(ServerRemote);
                    Test->TestTrue(TEXT("A connected dead/spectating teammate does not block the host"), Mode->CanPlayerLeaveMatch(Host.Get()));
                    Host->ProcessEvent(Host->FindFunctionChecked(TEXT("HandleLeaveMatchConfirmed")), nullptr);
                    Phase = 3;
                }
                else
                {
                    if (Case == TEXT("DeadHostBlocked"))
                    {
                        Mode->NotifyPlayerDied(Host->PlayerState);
                        Test->TestFalse(TEXT("A dead host still cannot leave while its guest is active"), Mode->CanPlayerLeaveMatch(Host.Get()));
                        Host->ServerRequestLeaveMatch();
                        Test->TestFalse(TEXT("Server rejects a dead host's direct leave RPC"), Host->IsLeaveMatchInProgress());
                    }
                    Test->TestTrue(TEXT("A client may voluntarily leave"), Mode->CanPlayerLeaveMatch(Cast<APlayerController>(ServerRemote->GetOwner())));
                    Test->TestFalse(TEXT("Client still has its replicated items before ESC"), RemoteState->GetInventory()->GetEntries().IsEmpty());
                    Guest->ProcessEvent(Guest->FindFunctionChecked(TEXT("HandleLeaveMatchConfirmed")), nullptr);
                    Phase = 2;
                }
                return false;
            }
            if (Phase == 2)
            {
                if (Guest.IsValid()) return false;
                if (!VerifyProfile(GuestSlot, GuestStash)) return false;
                if (Case == TEXT("DeadHostBlocked")) return true;
                if (!Mode || !Host.IsValid() || Mode->GetNumPlayers() != 1) return false;
                Test->TestTrue(TEXT("Client departure releases the host's ESC restriction"), Mode->CanPlayerLeaveMatch(Host.Get()));
                Host->ProcessEvent(Host->FindFunctionChecked(TEXT("HandleLeaveMatchConfirmed")), nullptr);
                Phase = 3;
                return false;
            }
            if (Host.IsValid()) return false;
            if (!VerifyProfile(HostSlot, HostStash)) return false;
            Test->AddInfo(TEXT("Actual PIE verified server ESC policy, carried-item forfeiture and persisted stash preservation."));
            return true;
        }
    };
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FHBMatchLeaveTest, "HeavenBreach.Multiplayer.MatchLeave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FHBMatchLeaveTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const TCHAR* Case : {TEXT("Solo"), TEXT("ClientDeparture"), TEXT("HostAfterDeath"), TEXT("DeadHostBlocked")})
    { Names.Add(Case); Commands.Add(Case); }
}

bool FHBMatchLeaveTest::RunTest(const FString& Case)
{
    FString UserDir;
    if (!FParse::Param(FCommandLine::Get(), TEXT("HBMatchLeaveTest")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) ||
        !FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(UserDir), FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/QA"))))
    { AddError(TEXT("Match leave tests require -HBMatchLeaveTest and an isolated -UserDir under project Saved/QA.")); return false; }
    AddExpectedErrorPlain(TEXT("Null input trigger detected in mapping to input action 'IA_Interact'"), EAutomationExpectedErrorFlags::Contains, 0);
    if (Case == TEXT("HostAfterDeath"))
    {
        AddExpectedErrorPlain(TEXT("UEngine::BroadcastNetworkFailure: FailureType = FailureReceived, ErrorString = 호스트가 접속을 끊었습니다."), EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedErrorPlain(TEXT("UEngine::BroadcastNetworkFailure: FailureType = ConnectionLost, ErrorString = 호스트와의 접속이 끊어졌습니다."), EAutomationExpectedErrorFlags::Contains, 1);
    }
    const auto Fixture = MakeShared<FMatchLeaveFixture>();
    auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->GetPlayNetMode(Fixture->Mode);
    Settings->GetPlayNumberOfClients(Fixture->Players);
    Settings->GetRunUnderOneProcess(Fixture->bOneProcess);
    Settings->GetServerPort(Fixture->Port);
    Fixture->bSeparateServer = Settings->bLaunchSeparateServer;
    Fixture->Drivers = GEngine->NetDriverDefinitions;
    Settings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
    Settings->SetPlayNumberOfClients(Case == TEXT("Solo") ? 1 : 2);
    Settings->SetRunUnderOneProcess(true);
    Settings->SetServerPort(19879);
    Settings->bLaunchSeparateServer = false;
    for (auto& Driver : GEngine->NetDriverDefinitions)
        if (Driver.DefName == NAME_GameNetDriver) Driver.DriverClassName = Driver.DriverClassNameFallback = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
    Fixture->Initialization = FWorldDelegates::OnWorldInitializedActors.AddLambda([](const FActorsInitializedParams& Params)
    {
        if (Params.World && Params.World->WorldType == EWorldType::PIE)
            if (auto* Mode = Params.World->GetAuthGameMode<AHeavenBreachGameMode>())
                FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutoStartMatch"))->SetPropertyValue_InContainer(Mode, false);
    });
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Engine/Maps/Entry")));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
    {
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::Editor && Context.World())
                Context.World()->GetWorldSettings()->DefaultGameMode = LoadClass<AHeavenBreachGameMode>(nullptr,
                    TEXT("/Game/Core/MainGameModes/BP_HeavenBreachGameMode.BP_HeavenBreachGameMode_C"));
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVerifyMatchLeave>(this, Case));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Fixture]()
    {
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) return false;
        FWorldDelegates::OnWorldInitializedActors.Remove(Fixture->Initialization);
        GEngine->NetDriverDefinitions = Fixture->Drivers;
        auto* Restore = GetMutableDefault<ULevelEditorPlaySettings>();
        Restore->SetPlayNetMode(Fixture->Mode);
        Restore->SetPlayNumberOfClients(Fixture->Players);
        Restore->SetRunUnderOneProcess(Fixture->bOneProcess);
        Restore->SetServerPort(Fixture->Port);
        Restore->bLaunchSeparateServer = Fixture->bSeparateServer;
        return true;
    }));
    return true;
}
#endif
