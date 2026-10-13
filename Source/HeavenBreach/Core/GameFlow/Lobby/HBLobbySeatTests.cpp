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
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "UI/Lobby/HBLobbyWidget.h"
#include "UI/Lobby/HBLobbyHUDWidget.h"
#include "UI/Skill/HB_SkillMainWidget.h"
#include "UI/Skill/HB_SkillIconCardWidget.h"
#include "Inventory/InventoryWidget.h"
#include "Inventory/StashComponent.h"
#include "Shop/ShopComponent.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"
#endif

#if WITH_EDITOR
namespace
{
	FString ExpectedLobbyAnimClassName(const AHBPlayerCharacter* Pawn)
	{
		const FString Tag = Pawn->GetEquippedWeaponTag().ToString();
		return Tag == TEXT("Weapon.Bow") ? TEXT("ABP_Player_Bow_C")
			: Tag == TEXT("Weapon.Staff") ? TEXT("ABP_Player_Staff_C") : TEXT("ABP_Warrior_F_C");
	}
}
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
		auto* Idle = Pawn->GetMesh()->GetAnimInstance();
		TestTrue(TEXT("Equipment refresh preserves the original weapon AnimBP and Idle corrections"), Idle
			&& Pawn->GetMesh()->GetAnimationMode() == EAnimationMode::AnimationBlueprint
			&& Idle->GetClass()->GetName() == ExpectedLobbyAnimClassName(Pawn));
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
		for (const FString WeaponType : {FString(TEXT("Sword")), FString(TEXT("Bow")), FString(TEXT("Staff"))})
		{
			bool bVerified = false;
			for (const FName RowName : Weapons->GetRowNames())
			{
				const auto* Row = Weapons->FindRow<FWeaponData>(RowName, TEXT("LobbyEquipmentAppearance"));
				if (!Row || Row->WeaponType != WeaponType || Row->EquipMeshPath.IsEmpty()) continue;
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
			TestTrue(FString::Printf(TEXT("%s equipment and original AnimBP are verified"), *WeaponType), bVerified);
		}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

namespace
{
	UObject* LobbyUIReference(UObject* Object, FName Name)
	{
		auto* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
	}

	class FHBLobbySeatReplication : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		int32 PlayerCount;
		double Started = FPlatformTime::Seconds();
		bool bGuestLeft = false;
		TWeakObjectPtr<UGameInstance> Server;
		TArray<TWeakObjectPtr<UGameInstance>> RemainingClients;
		TWeakObjectPtr<AHBPlayerCharacter> LeavingPawn;
		bool bMenusVerified = false;
		int32 TradeStep = 0;
		FShopEntry TradeItem;
		FGuid TradeItemId;
		int32 InitialGuestGold = 0;
		int32 InitialHostGold = 0;
		int32 SkillStep = 0;
		FHBStartingLoadout InitialClientSkills;
		FHBStartingLoadout InitialHostSkills;
		FHBStartingLoadout ChangedClientSkills;

		bool VerifyClientSkills(const TArray<UWorld*>& Worlds, UWorld* HostWorld)
		{
			if (SkillStep == 3) return true;
			UWorld* ClientWorld = *Worlds.FindByPredicate([](UWorld* World) { return World->GetNetMode() == NM_Client; });
			auto* Controller = ClientWorld->GetFirstPlayerController();
			auto* Client = Controller->GetPlayerState<AHBLobbyPlayerState>();
			auto* Host = HostWorld->GetFirstPlayerController()->GetPlayerState<AHBLobbyPlayerState>();
			AHBLobbyPlayerState* ServerClient = nullptr;
			for (APlayerState* Player : HostWorld->GetGameState()->PlayerArray)
				if (auto* LobbyPlayer = Cast<AHBLobbyPlayerState>(Player); LobbyPlayer && LobbyPlayer->GetLobbySeatIndex() == Client->GetLobbySeatIndex())
					ServerClient = LobbyPlayer;
			if (!ServerClient) return false;
			auto* HUD = CastChecked<UHBLobbyHUDWidget>(LobbyUIReference(Controller, TEXT("LobbyWidget")));
			auto* Skills = CastChecked<UHB_SkillMainWidget>(HUD->GetContentWidget(EHBLobbyHUDMenu::ChoiceSkill));
			auto* Grid = CastChecked<UPanelWidget>(Skills->GetWidgetFromName(TEXT("ActiveSkillGrid")));
			auto FindCard = [&](FName Id) -> UHB_SkillIconCardWidget*
			{
				for (UWidget* Child : Grid->GetAllChildren())
					if (auto* Card = Cast<UHB_SkillIconCardWidget>(Child); Card && Card->GetSkillId() == Id) return Card;
				return nullptr;
			};
			auto ClickSkill = [&](FName Id)
			{
				HUD->SwitchToMenu(EHBLobbyHUDMenu::ChoiceSkill);
				auto* Slot = Cast<UHB_SkillSlotWidget>(Skills->GetWidgetFromName(TEXT("Slot_Q")));
				auto* SlotButton = Slot ? Cast<UButton>(Slot->GetWidgetFromName(TEXT("SlotButton"))) : nullptr;
				auto* Card = FindCard(Id);
				auto* CardButton = Card ? Cast<UButton>(Card->GetWidgetFromName(TEXT("CardButton"))) : nullptr;
				if (!SlotButton || !CardButton)
				{ Test->AddError(TEXT("Client skill slot/card buttons are missing.")); return false; }
				Test->TestTrue(TEXT("Client skill buttons are enabled"), SlotButton->GetIsEnabled() && CardButton->GetIsEnabled());
				if (!Slot->IsSlotSelected()) SlotButton->OnClicked.Broadcast();
				CardButton->OnClicked.Broadcast();
				return true;
			};
			if (SkillStep == 0)
			{
				if (Client->GetStartingLoadout() != ServerClient->GetStartingLoadout()) return false;
				InitialClientSkills = Client->GetStartingLoadout();
				InitialHostSkills = Host->GetStartingLoadout();
				ChangedClientSkills = InitialClientSkills;
				for (UWidget* Child : Grid->GetAllChildren())
					if (auto* Card = Cast<UHB_SkillIconCardWidget>(Child); Card
						&& Card->GetSkillId() != InitialClientSkills.SkillQId && Card->GetSkillId() != InitialClientSkills.SkillEId)
					{ ChangedClientSkills.SkillQId = Card->GetSkillId(); break; }
				if (ChangedClientSkills == InitialClientSkills)
				{ Test->AddError(TEXT("Client has no alternative Q skill to verify.")); SkillStep = 3; return true; }
				if (!ClickSkill(ChangedClientSkills.SkillQId)) { SkillStep = 3; return true; }
				SkillStep = 1;
				return false;
			}
			if (SkillStep == 1)
			{
				if (Client->GetStartingLoadout() != ChangedClientSkills || ServerClient->GetStartingLoadout() != ChangedClientSkills) return false;
				if (auto* Border = Cast<UWidget>(LobbyUIReference(FindCard(ChangedClientSkills.SkillQId), TEXT("SelectedBorder"))))
					Test->TestEqual(TEXT("Replicated client skill is highlighted automatically"), Border->GetVisibility(), ESlateVisibility::Visible);
				auto* Slot = CastChecked<UHB_SkillSlotWidget>(Skills->GetWidgetFromName(TEXT("Slot_Q")));
				Test->TestNotNull(TEXT("Replicated client skill has its detail tooltip"), Slot->GetToolTip());
				Test->TestTrue(TEXT("Client skill changes preserve the host's loadout"), Host->GetStartingLoadout() == InitialHostSkills);
				const FName RestoreId = InitialClientSkills.SkillQId.IsNone() ? ChangedClientSkills.SkillQId : InitialClientSkills.SkillQId;
				if (!ClickSkill(RestoreId)) { SkillStep = 3; return true; }
				SkillStep = 2;
				return false;
			}
			if (Client->GetStartingLoadout() != InitialClientSkills || ServerClient->GetStartingLoadout() != InitialClientSkills) return false;
			if (auto* Border = Cast<UWidget>(LobbyUIReference(FindCard(ChangedClientSkills.SkillQId), TEXT("SelectedBorder"))))
				Test->TestEqual(TEXT("Replaced client skill loses its highlight automatically"), Border->GetVisibility(), ESlateVisibility::Collapsed);
			Test->AddInfo(TEXT("Client skill slot/card UI changed and restored Q through the real server RPC; replication updated the highlight and tooltip without changing the host's loadout."));
			HUD->SwitchToMenu(EHBLobbyHUDMenu::Lobby);
			SkillStep = 3;
			return true;
		}

		bool VerifyClientTrade(const TArray<UWorld*>& Worlds, UWorld* HostWorld)
		{
			if (PlayerCount != 2 || TradeStep == 5) return true;
			UWorld* ClientWorld = *Worlds.FindByPredicate([](UWorld* World) { return World->GetNetMode() == NM_Client; });
			auto* ClientController = ClientWorld->GetFirstPlayerController();
			auto* Client = ClientController->GetPlayerState<AHBLobbyPlayerState>();
			auto* Host = HostWorld->GetFirstPlayerController()->GetPlayerState<AHBLobbyPlayerState>();
			AHBLobbyPlayerState* ServerGuest = nullptr;
			for (APlayerState* Player : HostWorld->GetGameState()->PlayerArray)
				if (auto* LobbyPlayer = Cast<AHBLobbyPlayerState>(Player); LobbyPlayer && LobbyPlayer->GetLobbySeatIndex() == 1)
					ServerGuest = LobbyPlayer;
			if (!ServerGuest) return false;
			auto* Stash = Client->GetStash();
			auto* ServerStash = ServerGuest->GetStash();
			auto* Shop = Client->GetShop();
			const FName Gold = Shop->GetGoldItemRow().RowName;
			auto* HUD = CastChecked<UHBLobbyHUDWidget>(LobbyUIReference(ClientController, TEXT("LobbyWidget")));
			auto* UI = CastChecked<UUserWidget>(HUD->GetContentWidget(EHBLobbyHUDMenu::Shop));
			auto* Confirm = Cast<UButton>(UI->GetWidgetFromName(TEXT("ConfirmButton")));
			if (!Confirm) { Test->AddError(TEXT("Shop confirmation button is missing.")); TradeStep = 5; return true; }
			auto ClickConfirm = [&]()
			{
				HUD->SwitchToMenu(EHBLobbyHUDMenu::Shop);
				Test->TestTrue(TEXT("Client shop confirmation button is enabled"), Confirm->GetIsEnabled());
				Confirm->OnClicked.Broadcast();
			};
			if (TradeStep == 0)
			{
				if (Stash->GetTotalGold(Gold) != ServerStash->GetTotalGold(Gold) || Host->GetStash()->GetTotalGold(Gold) == 0) return false;
				for (const FShopEntry& Entry : Shop->GetShopEntries())
					if (Entry.BuyPrice > 0 && Entry.ViewData.SellValue > 0 && !Entry.ViewData.IsBag()
						&& Entry.ViewData.GridSize.X <= 2 && Entry.ViewData.GridSize.Y <= 2
						&& Stash->GetTotalGold(Gold) >= Entry.BuyPrice
						&& !Stash->GetEntries().ContainsByPredicate([&](const FInventoryEntry& Item) { return Item.Item.ItemRow == Entry.ItemRow; }))
					{ TradeItem = Entry; break; }
				if (TradeItem.ItemRow.IsNull()) return false;
				InitialGuestGold = Stash->GetTotalGold(Gold);
				InitialHostGold = Host->GetStash()->GetTotalGold(Gold);
				Shop->SetTradeMode(ETradeMode::Buy);
				if (!Test->TestTrue(TEXT("Client can add an available product to the basket"), Shop->AddToBuyBasket(TradeItem.ItemRow, 1)))
				{ TradeStep = 5; return true; }
				ClickConfirm();
				TradeStep = 1;
				return false;
			}
			if (TradeStep == 1)
			{
				const auto* Bought = Stash->GetEntries().FindByPredicate([&](const FInventoryEntry& Item) { return Item.Item.ItemRow == TradeItem.ItemRow; });
				if (!Bought || Shop->GetBasket().Num() != 0 || Stash->GetTotalGold(Gold) != InitialGuestGold - TradeItem.BuyPrice
					|| ServerStash->FindEntryIndexById(Bought->Item.InstanceId) == INDEX_NONE) return false;
				TradeItemId = Bought->Item.InstanceId;
				Stash->Server_TransferItemAuto(Client->GetInventory(), TradeItemId);
				TradeStep = 2;
				return false;
			}
			if (TradeStep == 2)
			{
				if (Client->GetInventory()->FindEntryIndexById(TradeItemId) == INDEX_NONE
					|| ServerGuest->GetInventory()->FindEntryIndexById(TradeItemId) == INDEX_NONE
					|| Stash->FindEntryIndexById(TradeItemId) != INDEX_NONE || ServerStash->FindEntryIndexById(TradeItemId) != INDEX_NONE) return false;
				Client->GetInventory()->Server_TransferItemAuto(Stash, TradeItemId);
				TradeStep = 3;
				return false;
			}
			if (TradeStep == 3)
			{
				if (Stash->FindEntryIndexById(TradeItemId) == INDEX_NONE || ServerStash->FindEntryIndexById(TradeItemId) == INDEX_NONE
					|| Client->GetInventory()->FindEntryIndexById(TradeItemId) != INDEX_NONE
					|| ServerGuest->GetInventory()->FindEntryIndexById(TradeItemId) != INDEX_NONE) return false;
				Shop->SetTradeMode(ETradeMode::Sell);
				if (!Test->TestTrue(TEXT("Client can sell its purchased item"), Shop->AddToSellBasket(TradeItemId)))
				{ TradeStep = 5; return true; }
				ClickConfirm();
				TradeStep = 4;
				return false;
			}
			const int32 ExpectedGold = InitialGuestGold - TradeItem.BuyPrice + TradeItem.ViewData.SellValue;
			if (Stash->FindEntryIndexById(TradeItemId) != INDEX_NONE || ServerStash->FindEntryIndexById(TradeItemId) != INDEX_NONE
				|| Stash->GetTotalGold(Gold) != ExpectedGold || ServerStash->GetTotalGold(Gold) != ExpectedGold || Shop->GetBasket().Num() != 0) return false;
			Test->TestEqual(TEXT("Client trades never change the host's stash gold"), Host->GetStash()->GetTotalGold(Gold), InitialHostGold);
			Test->AddInfo(TEXT("Client shop confirmation UI bought and sold through real server RPCs; stash/inventory round trip replicated on both sides without affecting the host."));
			HUD->SwitchToMenu(EHBLobbyHUDMenu::Lobby);
			TradeStep = 5;
			return true;
		}

		bool PersonalMenusArrived(UWorld* World) const
		{
			auto* Controller = World->GetFirstPlayerController();
			auto* State = Controller ? Controller->GetPlayerState<AHBLobbyPlayerState>() : nullptr;
			auto* HUD = Cast<UHBLobbyHUDWidget>(LobbyUIReference(Controller, TEXT("LobbyWidget")));
			if (!State || !HUD) return false;
			auto* Stash = HUD->GetContentWidget(EHBLobbyHUDMenu::Storage);
			auto* Shop = Cast<UUserWidget>(HUD->GetContentWidget(EHBLobbyHUDMenu::Shop));
			auto* Rows = Shop ? Cast<UPanelWidget>(Shop->GetWidgetFromName(TEXT("ShopItemList"))) : nullptr;
			auto* Skills = Cast<UHB_SkillMainWidget>(HUD->GetContentWidget(EHBLobbyHUDMenu::ChoiceSkill));
			auto* Job = State->GetSelectedJob();
			auto* Active = Skills ? Cast<UPanelWidget>(Skills->GetWidgetFromName(TEXT("ActiveSkillGrid"))) : nullptr;
			auto* Passive = Skills ? Cast<UPanelWidget>(Skills->GetWidgetFromName(TEXT("PassiveSkillGrid"))) : nullptr;
			if (!Job || !Active || !Passive) return false;
			int32 ActiveCount = 0, PassiveCount = 0;
			for (const auto& Skill : Job->GetActiveSkillCatalog()) if (Skill.AbilityClass) ++ActiveCount;
			for (const auto& Skill : Job->GetPassiveSkillCatalog()) if (Skill.AbilityClass) ++PassiveCount;
			return Active->GetChildrenCount() == ActiveCount && Passive->GetChildrenCount() == PassiveCount
				&& ActiveCount > 0 && PassiveCount > 0
				&& Rows && Rows->GetChildrenCount() == State->GetShop()->GetShopEntries().Num()
				&& Rows->GetChildrenCount() > 0
				&& LobbyUIReference(Stash, TEXT("PlayerInv")) == State->GetInventory()
				&& LobbyUIReference(Stash, TEXT("StashInv")) == State->GetStash()
				&& LobbyUIReference(Shop, TEXT("Shop")) == State->GetShop()
				&& LobbyUIReference(Shop, TEXT("StashRef")) == State->GetStash();
		}

		void VerifyPersonalMenus(UWorld* World) const
		{
			auto* Controller = World->GetFirstPlayerController();
			auto* State = Controller->GetPlayerState<AHBLobbyPlayerState>();
			auto* HUD = CastChecked<UHBLobbyHUDWidget>(LobbyUIReference(Controller, TEXT("LobbyWidget")));
			auto* Stash = CastChecked<UUserWidget>(HUD->GetContentWidget(EHBLobbyHUDMenu::Storage));
			auto* Shop = CastChecked<UUserWidget>(HUD->GetContentWidget(EHBLobbyHUDMenu::Shop));
			auto* Rows = CastChecked<UPanelWidget>(Shop->GetWidgetFromName(TEXT("ShopItemList")));
			auto* ShopStash = Cast<UInventoryWidget>(Shop->GetWidgetFromName(TEXT("StashPanel")));
			Test->TestTrue(TEXT("Each player's shop uses their own stash panel"), ShopStash && ShopStash->GetBoundInventory() == State->GetStash());
			Test->TestTrue(TEXT("Shop inventory ownership comes from the local PlayerState"), LobbyUIReference(ShopStash, TEXT("OwningInventory")) == State->GetInventory());
			const int32 RowCount = Rows->GetChildrenCount();
			const int32 BasketBindings = State->GetShop()->OnBasketChanged.GetAllObjects().Num();
			const int32 StashBindings = State->GetStash()->OnInventoryChanged.GetAllObjects().Num();
			auto* Skills = CastChecked<UHB_SkillMainWidget>(HUD->GetContentWidget(EHBLobbyHUDMenu::ChoiceSkill));
			auto* Active = CastChecked<UPanelWidget>(Skills->GetWidgetFromName(TEXT("ActiveSkillGrid")));
			auto* Passive = CastChecked<UPanelWidget>(Skills->GetWidgetFromName(TEXT("PassiveSkillGrid")));
			const int32 ActiveCount = Active->GetChildrenCount(), PassiveCount = Passive->GetChildrenCount();
			const int32 SkillBindings = State->OnStartingLoadoutChanged.GetAllObjects().Num();
			Test->TestTrue(TEXT("Repeated HUD refresh completes without rebuilding personal menus"), HUD->RefreshPlayerMenus() && HUD->RefreshPlayerMenus());
			// Exercise the existing BP entry points too, including widget reconstruction/rebinding.
			for (int32 Repeat = 0; Repeat < 2; ++Repeat)
			{
				Stash->ProcessEvent(Stash->FindFunctionChecked(TEXT("InitStash")), nullptr);
				Shop->ProcessEvent(Shop->FindFunctionChecked(TEXT("InitShop")), nullptr);
			}
			Test->TestEqual(TEXT("Reinitializing the shop never duplicates products"), Rows->GetChildrenCount(), RowCount);
			Test->TestEqual(TEXT("Reinitializing never duplicates basket delegates"), State->GetShop()->OnBasketChanged.GetAllObjects().Num(), BasketBindings);
			Test->TestEqual(TEXT("Reinitializing never duplicates stash delegates"), State->GetStash()->OnInventoryChanged.GetAllObjects().Num(), StashBindings);
			auto* Switcher = CastChecked<UWidgetSwitcher>(LobbyUIReference(HUD, TEXT("LobbyContentSwitcher")));
			for (const auto Menu : {EHBLobbyHUDMenu::Storage, EHBLobbyHUDMenu::Shop, EHBLobbyHUDMenu::ChoiceSkill, EHBLobbyHUDMenu::ChoiceSkill})
			{
				HUD->SwitchToMenu(Menu);
				Test->TestTrue(TEXT("Host and client can both display their personal menu"), Switcher->GetActiveWidget() == HUD->GetContentWidget(Menu));
			}
			Test->TestEqual(TEXT("Reopening skills never duplicates active cards"), Active->GetChildrenCount(), ActiveCount);
			Test->TestEqual(TEXT("Reopening skills never duplicates passive cards"), Passive->GetChildrenCount(), PassiveCount);
			Test->TestEqual(TEXT("Reopening skills never duplicates loadout delegates"), State->OnStartingLoadoutChanged.GetAllObjects().Num(), SkillBindings);
			HUD->SwitchToMenu(EHBLobbyHUDMenu::Lobby);
		}

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
				auto* Idle = It->GetMesh()->GetAnimInstance();
				if (!Idle || It->GetMesh()->GetAnimationMode() != EAnimationMode::AnimationBlueprint
					|| Idle->GetClass()->GetName() != ExpectedLobbyAnimClassName(*It)
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
		FHBLobbySeatReplication(FAutomationTestBase* InTest, int32 InPlayerCount) : Test(InTest), PlayerCount(InPlayerCount) {}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 60.)
			{
				if (bMenusVerified && SkillStep < 3)
				{ Test->AddError(FString::Printf(TEXT("Client skill selection did not finish; step=%d."), SkillStep)); return true; }
				if (bMenusVerified && TradeStep < 5 && PlayerCount == 2)
				{ Test->AddError(FString::Printf(TEXT("Client trade/transfer did not finish; step=%d."), TradeStep)); return true; }
				Test->AddError(bGuestLeft ? TEXT("Lobby seats did not compact and replicate after guest logout.")
					: TEXT("Lobby pawns, seats or personal menus did not initialize and replicate."));
				return true;
			}
			if (bGuestLeft)
			{
				if (!Server.IsValid() || !SeatsArrived(Server->GetWorld(), PlayerCount - 1)) return false;
				for (const auto& Client : RemainingClients)
					if (!Client.IsValid() || !SeatsArrived(Client->GetWorld(), PlayerCount - 1)) return false;
				Test->TestFalse(TEXT("Actual Logout destroys the leaving guest's display pawn"), LeavingPawn.IsValid());
				Test->AddInfo(FString::Printf(TEXT("%d-player local IP listen lobby replicated pawns, ownership, camera and seats; guest Logout left %d players with matching seats."), PlayerCount, PlayerCount - 1));
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
			if (Worlds.Num() != PlayerCount || !HostWorld) return false;
			for (UWorld* World : Worlds) if (!SeatsArrived(World, PlayerCount)) return false;
			for (UWorld* World : Worlds) if (!PersonalMenusArrived(World)) return false;
			if (!bMenusVerified)
			{
				for (UWorld* World : Worlds) VerifyPersonalMenus(World);
				Test->AddInfo(TEXT("Host and client personal menus and skill lists initialized automatically; ownership, card counts and delegates remain stable after repeated initialization."));
				bMenusVerified = true;
			}
			if (!VerifyClientSkills(Worlds, HostWorld) || !VerifyClientTrade(Worlds, HostWorld)) return false;
			if (PlayerCount == 2)
			{
				auto* Controller = Cast<AHBLobbyPlayerController>(HostWorld->GetFirstPlayerController());
				auto* ContentProperty = Controller ? FindFProperty<FObjectPropertyBase>(Controller->GetClass(), TEXT("LobbyContent")) : nullptr;
				auto* Lobby = ContentProperty ? Cast<UHBLobbyWidget>(ContentProperty->GetObjectPropertyValue_InContainer(Controller)) : nullptr;
				if (!Lobby || !Lobby->WidgetTree) return false;
				auto* StartModule = Cast<UUserWidget>(Lobby->WidgetTree->FindWidget(TEXT("StartMatchButtonModule")));
				auto* Start = StartModule && StartModule->WidgetTree ? Cast<UButton>(StartModule->WidgetTree->FindWidget(TEXT("Button_Action"))) : nullptr;
				if (!Start) return false;
				auto* State = HostWorld->GetGameState<AHBLobbyGameState>();
				Lobby->RefreshLobbyActions();
				Test->TestTrue(TEXT("Two-player roster is valid without guest Ready"), State->HasValidStartRoster());
				for (APlayerState* Player : State->PlayerArray)
					Test->TestFalse(TEXT("Neither player needs to set Ready"), CastChecked<AHBLobbyPlayerState>(Player)->IsReady());
				Test->TestTrue(TEXT("Production start action is bound"), Lobby->OnStartRequested.IsBound());
				Test->TestFalse(TEXT("Local IP PIE has no Steam lobby host"), Lobby->IsLocalLobbyHost());
				Test->TestFalse(TEXT("Start stays disabled without a hosted Steam session"), Start->GetIsEnabled());
				Test->TestFalse(TEXT("Controller start request is rejected without a hosted Steam session"), Controller->RequestStartDedicatedMatch());
				if (auto* Ready = Lobby->WidgetTree->FindWidget(TEXT("ReadyButtonModule")))
					Test->TestEqual(TEXT("Ready module is hidden"), Ready->GetVisibility(), ESlateVisibility::Collapsed);
				Test->AddInfo(TEXT("Reproduced disabled Start in two-player single-process listen PIE: roster is valid and both players are unready, but no Steam hosted session exists."));
			}
			const int32 LeavingSeat = FMath::Min(2, PlayerCount - 1);
			APlayerController* Leaving = nullptr;
			for (FConstPlayerControllerIterator It = HostWorld->GetPlayerControllerIterator(); It; ++It)
				if (auto* Player = It->Get()->GetPlayerState<AHBLobbyPlayerState>(); Player && Player->GetLobbySeatIndex() == LeavingSeat)
					Leaving = It->Get();
			if (!Leaving) return false;
			Server = HostWorld->GetGameInstance();
			for (UWorld* World : Worlds)
				if (World != HostWorld && World->GetFirstPlayerController()->GetPlayerState<AHBLobbyPlayerState>()->GetLobbySeatIndex() != LeavingSeat)
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
	// 초기 복제 전 한 번 발생하는 안내 이후, 실제 스킬 목록과 장착 RPC까지 검증한다.
	AddExpectedError(TEXT("SkillMain | 목록을 만들 수 없음: GetEditingJob() == nullptr"), EAutomationExpectedErrorFlags::Contains, 0);
	// This fixture uses local IP only. Production continues to use SteamSockets unchanged.
	int32 PlayerCount = 4;
	FParse::Value(FCommandLine::Get(), TEXT("HBLobbyTestPlayers="), PlayerCount);
	PlayerCount = FMath::Clamp(PlayerCount, 2, 4);
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
	Settings->SetPlayNumberOfClients(PlayerCount);
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
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHBLobbySeatReplication>(this, PlayerCount));
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
