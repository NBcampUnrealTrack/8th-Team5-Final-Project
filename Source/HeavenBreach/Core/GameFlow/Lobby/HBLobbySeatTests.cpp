#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/GameFlow/Lobby/HBLobbyGameState.h"
#include "Player/HBPlayerCharacter.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Equipment/EquipmentComponent.h"
#include "Item/Item.h"
#include "Player/Job/HBJobData.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Engine/LatentActionManager.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#if WITH_EDITOR
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBLobbySeatsTest, "HeavenBreach.Multiplayer.LobbySeats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FHBLobbySeatsTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Mode = World->SpawnActor<AHBLobbyGameMode>();
	// Seat ordering is independent of gameplay initialization; the network fixture uses the real child BP.
	Mode->DefaultPawnClass = ADefaultPawn::StaticClass();
	auto* State = World->SpawnActor<AHBLobbyGameState>();
	Mode->GameState = State;
	World->SetGameState(State);
	TArray<ATargetPoint*> Seats;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Seat = World->SpawnActor<ATargetPoint>();
		Seat->Tags = {TEXT("LobbyCharacterSlot"), FName(*FString::Printf(TEXT("LobbySlot_%d"), Index))};
		Seat->SetActorTransform(FTransform(FRotator(0., Index * 30., 0.), FVector(Index * 200., 600., 170.)));
		Seats.Add(Seat);
	}
	TArray<APlayerController*> Players;
	auto Join = [&]()
	{
		auto* Controller = World->SpawnActor<APlayerController>();
		auto* Player = World->SpawnActor<AHBLobbyPlayerState>();
		Player->SetOwner(Controller);
		Controller->PlayerState = Player;
		State->AddPlayerState(Player);
		Players.Add(Controller);
		return Controller;
	};
	auto* Host = Join();
	State->HostPlayerState = Host->GetPlayerState<AHBLobbyPlayerState>();
	Mode->RefreshLobbySeats(Host);
	for (int32 Index = 1; Index < 4; ++Index) Mode->RefreshLobbySeats(Join());
	TArray<APawn*> OriginalPawns;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Player = Players[Index]->GetPlayerState<AHBLobbyPlayerState>();
		TestEqual(TEXT("Host is seat zero and guests follow accepted join order"), Player->GetLobbySeatIndex(), Index);
		APawn* Pawn = Players[Index]->GetPawn();
		OriginalPawns.Add(Pawn);
		TestNotNull(TEXT("Each accepted player possesses a lobby pawn"), Cast<ADefaultPawn>(Pawn));
		if (Pawn) TestTrue(TEXT("Pawn uses the authored seat transform"), Pawn->GetActorTransform().Equals(Seats[Index]->GetActorTransform()));
	}
	Algo::Reverse(State->PlayerArray);
	Mode->RefreshLobbySeats();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestEqual(TEXT("PlayerArray reordering never changes seats"), Players[Index]->GetPlayerState<AHBLobbyPlayerState>()->GetLobbySeatIndex(), Index);
		TestTrue(TEXT("Refreshing seats reuses existing pawns"), Players[Index]->GetPawn() == OriginalPawns[Index]);
	}
	// Simulate an exiting controller that is still present in PlayerArray during Logout.
	Mode->RefreshLobbySeats(nullptr, Players[2]);
	TestEqual(TEXT("The later guest moves forward after a middle guest leaves"), Players[3]->GetPlayerState<AHBLobbyPlayerState>()->GetLobbySeatIndex(), 2);
	TestTrue(TEXT("Compaction moves the existing pawn"), OriginalPawns[3] && OriginalPawns[3]->GetActorTransform().Equals(Seats[2]->GetActorTransform()));
	State->RemovePlayerState(Players[2]->PlayerState);
	Players[2]->GetPlayerState<AHBLobbyPlayerState>()->SetLobbySeatIndex(INDEX_NONE);
	Players[2]->UnPossess();
	if (OriginalPawns[2]) OriginalPawns[2]->Destroy();
	auto* Rejoined = Join();
	Mode->RefreshLobbySeats(Rejoined);
	TestEqual(TEXT("A rejoining guest goes to the end"), Rejoined->GetPlayerState<AHBLobbyPlayerState>()->GetLobbySeatIndex(), 3);
	auto* Overflow = Join();
	Mode->RefreshLobbySeats(Overflow);
	TestEqual(TEXT("No fifth seat is allocated"), Overflow->GetPlayerState<AHBLobbyPlayerState>()->GetLobbySeatIndex(), INDEX_NONE);
	TestNull(TEXT("An unseated player never spawns a pawn"), Overflow->GetPawn());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBLobbyEquipmentAppearanceTest, "HeavenBreach.Multiplayer.LobbyEquipmentAppearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHBLobbyEquipmentAppearanceTest::RunTest(const FString&)
{
	UClass* PawnClass = LoadClass<AHBPlayerCharacter>(nullptr, TEXT("/Game/Core/Lobby/BP_HBLobbyPawn.BP_HBLobbyPawn_C"));
	auto* Job = LoadObject<UHBJobData>(nullptr, TEXT("/Game/Characters/Warrior/DA_Job_Warrior.DA_Job_Warrior"));
	auto* Items = LoadObject<UDataTable>(nullptr, TEXT("/Game/Items/DataTables/DT_Item.DT_Item"));
	if (!TestNotNull(TEXT("Existing lobby child BP loads"), PawnClass)
		|| !TestNotNull(TEXT("Existing warrior job loads"), Job) || !TestNotNull(TEXT("Existing item table loads"), Items)) return false;
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	// Actor ProcessEvent suppresses Blueprint/delegate calls until this flag is initialized.
	World->InitializeActorsForPlay(FURL());
	World->SetGameState(World->SpawnActor<AHBLobbyGameState>());
	auto* Seat = World->SpawnActor<ATargetPoint>();
	Seat->Tags = {TEXT("LobbyCharacterSlot"), TEXT("LobbySlot_0")};
	auto* Controller = World->SpawnActor<APlayerController>();
	auto* Player = World->SpawnActor<AHBLobbyPlayerState>();
	Player->SetOwner(Controller);
	Controller->PlayerState = Player;
	FHBStartingLoadout Loadout;
	FText Error;
	TestTrue(TEXT("Existing warrior start configuration is valid"), MakeDefaultStartingLoadout(Job, Loadout, Error));
	TestTrue(TEXT("Lobby accepts the existing start configuration"), Player->TrySetStartingLoadout(Loadout, Error));
	Player->SetLobbySeatIndex(0);
	auto* Pawn = World->SpawnActor<AHBPlayerCharacter>(PawnClass);
	Controller->Possess(Pawn);
	// No GameInstance or PlayerState BeginPlay: the fixture cannot load or save account data.
	Pawn->DispatchBeginPlay();
	auto SettlePose = [&]()
	{
		World->GetLatentActionManager().BeginFrame();
		World->GetLatentActionManager().ProcessLatentActions(nullptr, 1.f / 30.f);
		auto* Idle = Pawn->GetMesh()->GetSingleNodeInstance();
		TestTrue(TEXT("Equipment refresh preserves arms-down lobby idle"), Idle && Idle->GetCurrentAsset()
			&& Idle->GetCurrentAsset()->GetName() == TEXT("AN_PlayerIdle1"));
	};
	SettlePose();
	TArray<USkeletalMeshComponent*> Meshes;
	Pawn->GetComponents(Meshes);
	const TPair<EEquipSlotType, FName> Parts[] = {
		{EEquipSlotType::Helmet, TEXT("HelmetMeshComp")}, {EEquipSlotType::Chest, TEXT("ChestMeshComp")},
		{EEquipSlotType::Legs, TEXT("LegsMeshComp")}};
	for (const auto& Part : Parts)
	{
		bool bVerified = false;
		for (const FName RowName : Items->GetRowNames())
		{
			const auto* Row = Items->FindRow<FItemData>(RowName, TEXT("LobbyEquipmentAppearance"));
			if (!Row || Row->EquipSlotType != Part.Key || Row->WorldMeshPath.IsEmpty()) continue;
			auto* ExpectedMesh = LoadObject<USkeletalMesh>(nullptr, *Row->WorldMeshPath);
			if (!ExpectedMesh) continue;
			FItemInstance Item;
			Item.ItemRow.DataTable = Items;
			Item.ItemRow.RowName = RowName;
			Item.InstanceId = FGuid::NewGuid();
			if (!TestTrue(TEXT("Existing equipment API accepts the test item"), Player->GetEquipment()->EquipDirect(Item, Part.Key))) break;
			USkeletalMeshComponent* const* Component = Meshes.FindByPredicate([&](const auto* Mesh) { return Mesh->GetFName() == Part.Value; });
			TestTrue(TEXT("Equipped armor is displayed by inherited character components"), Component && (*Component)->GetSkeletalMeshAsset() == ExpectedMesh);
			SettlePose();
			Player->GetEquipment()->ClearSlot(Part.Key);
			SettlePose();
			bVerified = true;
			break;
		}
		TestTrue(FString::Printf(TEXT("Existing armor data verifies slot %d"), static_cast<int32>(Part.Key)), bVerified);
	}
	auto* Weapons = LoadObject<UDataTable>(nullptr, TEXT("/Game/Items/DataTables/DT_Weapon.DT_Weapon"));
	TArray<UStaticMeshComponent*> StaticMeshes;
	Pawn->GetComponents(StaticMeshes);
	if (TestNotNull(TEXT("Existing weapon table loads"), Weapons))
		for (const bool bBow : {false, true})
		{
			bool bVerified = false;
			for (const FName RowName : Weapons->GetRowNames())
			{
				const auto* Row = Weapons->FindRow<FWeaponData>(RowName, TEXT("LobbyEquipmentAppearance"));
				if (!Row || (Row->WeaponType == TEXT("Bow")) != bBow || Row->EquipMeshPath.IsEmpty()) continue;
				FItemInstance Item;
				Item.ItemRow.DataTable = Weapons;
				Item.ItemRow.RowName = RowName;
				Item.InstanceId = FGuid::NewGuid();
				if (!TestTrue(TEXT("Existing equipment API accepts the weapon"), Player->GetEquipment()->EquipDirect(Item, EEquipSlotType::PrimaryWeapon))) break;
				bool bVisibleWeapon = false;
				for (auto* Mesh : StaticMeshes)
					if (Mesh->GetFName() == TEXT("WeaponMeshComp")) bVisibleWeapon |= Mesh->GetStaticMesh() != nullptr && Mesh->IsVisible();
				for (auto* Mesh : Meshes)
					if (Mesh->GetFName() == TEXT("WeaponSkeletalMeshComp")) bVisibleWeapon |= Mesh->GetSkeletalMeshAsset() != nullptr && Mesh->IsVisible();
				TestTrue(TEXT("Equipped weapon is displayed by inherited character components"), bVisibleWeapon);
				SettlePose();
				Player->GetEquipment()->ClearSlot(EEquipSlotType::PrimaryWeapon);
				SettlePose();
				bVerified = true;
				break;
			}
			TestTrue(bBow ? TEXT("Bow equipment and animation replacement are verified") : TEXT("Melee equipment and animation replacement are verified"), bVerified);
		}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

namespace
{
	class FHBLobbySeatReplication : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		double Started = FPlatformTime::Seconds();
		bool bGuestLeft = false;
		TWeakObjectPtr<UGameInstance> Server;
		TArray<TWeakObjectPtr<UGameInstance>> RemainingClients;
		TWeakObjectPtr<AHBPlayerCharacter> LeavingPawn;

		bool SeatsArrived(UWorld* World, int32 Expected) const
		{
			auto* State = World ? World->GetGameState<AHBLobbyGameState>() : nullptr;
			if (!State || State->PlayerArray.Num() != Expected || !State->HostPlayerState) return false;
			TSet<int32> Seen;
			for (TActorIterator<AHBPlayerCharacter> It(World); It; ++It)
			{
				if (It->IsActorBeingDestroyed()) continue;
				auto* Player = It->GetPlayerState<AHBLobbyPlayerState>();
				if (!Player || Player->GetLobbySeatIndex() < 0 || Player->GetLobbySeatIndex() >= Expected
					|| !It->GetMesh()->GetSkeletalMeshAsset()) return false;
				auto* Idle = It->GetMesh()->GetSingleNodeInstance();
				if (!Idle || !Idle->GetCurrentAsset() || Idle->GetCurrentAsset()->GetName() != TEXT("AN_PlayerIdle1")
					|| It->GetCharacterMovement()->MovementMode != MOVE_None || It->GetActorEnableCollision()) return false;
				const int32 Index = Player->GetLobbySeatIndex();
				if (Seen.Contains(Index) || (Index == 0) != (Player == State->HostPlayerState)) return false;
				bool bAtSeat = false;
				const FName Tag(*FString::Printf(TEXT("LobbySlot_%d"), Index));
				for (TActorIterator<ATargetPoint> Anchor(World); Anchor; ++Anchor)
					if (Anchor->ActorHasTag(TEXT("LobbyCharacterSlot")) && Anchor->ActorHasTag(Tag))
						bAtSeat = It->GetActorTransform().Equals(Anchor->GetActorTransform(), 0.1);
				if (!bAtSeat) return false;
				Seen.Add(Index);
			}
			auto* Controller = World->GetFirstPlayerController();
			return Seen.Num() == Expected && Controller && Cast<AHBPlayerCharacter>(Controller->GetPawn())
				&& Controller->GetViewTarget() && Controller->GetViewTarget() != Controller->GetPawn();
		}

	public:
		explicit FHBLobbySeatReplication(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 60.)
			{
				Test->AddError(bGuestLeft ? TEXT("Lobby seats did not compact and replicate after guest logout.")
					: TEXT("Four-player lobby pawns, meshes, ownership, camera or seats did not replicate."));
				return true;
			}
			if (bGuestLeft)
			{
				if (!Server.IsValid() || !SeatsArrived(Server->GetWorld(), 3)) return false;
				for (const auto& Client : RemainingClients)
					if (!Client.IsValid() || !SeatsArrived(Client->GetWorld(), 3)) return false;
				Test->TestFalse(TEXT("Actual Logout destroys the leaving guest's display pawn"), LeavingPawn.IsValid());
				Test->AddInfo(TEXT("Listen host and three local IP clients displayed identical job meshes and seats; actual Logout compacted and replicated seats to both remaining clients."));
				return true;
			}
			TArray<UWorld*> Worlds;
			UWorld* HostWorld = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
				if (Context.WorldType == EWorldType::PIE && Context.World())
				{
					Worlds.Add(Context.World());
					if (Context.World()->GetNetMode() == NM_ListenServer) HostWorld = Context.World();
				}
			if (Worlds.Num() != 4 || !HostWorld) return false;
			for (UWorld* World : Worlds) if (!SeatsArrived(World, 4)) return false;
			APlayerController* Leaving = nullptr;
			for (FConstPlayerControllerIterator It = HostWorld->GetPlayerControllerIterator(); It; ++It)
				if (auto* Player = It->Get()->GetPlayerState<AHBLobbyPlayerState>(); Player && Player->GetLobbySeatIndex() == 2)
					Leaving = It->Get();
			if (!Leaving) return false;
			Server = HostWorld->GetGameInstance();
			for (UWorld* World : Worlds)
				if (World != HostWorld && World->GetFirstPlayerController()->GetPlayerState<AHBLobbyPlayerState>()->GetLobbySeatIndex() != 2)
					RemainingClients.Add(World->GetGameInstance());
			LeavingPawn = Cast<AHBPlayerCharacter>(Leaving->GetPawn());
			// A normal local map departure closes the connection and invokes the server's actual Logout.
			Leaving->ClientTravel(TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"), TRAVEL_Absolute);
			bGuestLeft = true;
			Started = FPlatformTime::Seconds();
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBLobbySeatReplicationTest, "HeavenBreach.Multiplayer.LobbySeatReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHBLobbySeatReplicationTest::RunTest(const FString&)
{
	// This fixture uses local IP only. Production continues to use SteamSockets unchanged.
	EPlayNetMode PreviousMode;
	int32 PreviousClients;
	bool PreviousSingleProcess;
	uint16 PreviousPort;
	auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
	Settings->GetPlayNetMode(PreviousMode);
	Settings->GetPlayNumberOfClients(PreviousClients);
	Settings->GetRunUnderOneProcess(PreviousSingleProcess);
	Settings->GetServerPort(PreviousPort);
	Settings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
	Settings->SetPlayNumberOfClients(4);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetServerPort(19877);
	const auto PreviousDrivers = GEngine->NetDriverDefinitions;
	for (auto& Driver : GEngine->NetDriverDefinitions)
		if (Driver.DefName == TEXT("GameNetDriver"))
			Driver.DriverClassName = Driver.DriverClassNameFallback = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
	const auto Initialization = FWorldDelegates::OnWorldInitializedActors.AddLambda([](const FActorsInitializedParams& Params)
	{
		if (Params.World && Params.World->WorldType == EWorldType::PIE)
			if (auto* Mode = Params.World->GetAuthGameMode<AHBLobbyGameMode>())
				FindFProperty<FBoolProperty>(Mode->GetClass(), TEXT("bAutomaticallyHostInviteHub"))->SetPropertyValue_InContainer(Mode, false);
	});
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/Lobby/L_Lobby")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBLobbySeatReplication>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([PreviousMode, PreviousClients, PreviousSingleProcess, PreviousPort, PreviousDrivers, Initialization]()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) return false;
		FWorldDelegates::OnWorldInitializedActors.Remove(Initialization);
		GEngine->NetDriverDefinitions = PreviousDrivers;
		auto* Restore = GetMutableDefault<ULevelEditorPlaySettings>();
		Restore->SetPlayNetMode(PreviousMode);
		Restore->SetPlayNumberOfClients(PreviousClients);
		Restore->SetRunUnderOneProcess(PreviousSingleProcess);
		Restore->SetServerPort(PreviousPort);
		return true;
	}));
	return true;
}
#endif
#endif
