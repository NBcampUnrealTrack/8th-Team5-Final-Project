#include "Player/HBPlayerController.h"

#include "Inventory/InventoryUIComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/HBPlayerCharacter.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Inventory/QuickSlotComponent.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Core/GameFlow/HeavenBreachGameState.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "EngineUtils.h"
#include "CommonActivatableWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/Tag/UI/HB_UITag.h"
#include "UI/HB_UILayout.h"
#include "UI/HUD/HB_HUDLayer.h"
#include "UI/HB_ContainerScreen.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "UI/Skill/HB_SkillAugment.h"
#include "UI/Common/HB_ConfirmPopup.h"
#include "Core/GameFlow/HBMatchResultSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "UI/Settings/HBSettingsSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "UI/HUD/HB_SpectateWidget.h"
#include "Player/HBNotificationThrottle.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Attack/GA_PlayerBowAttack.h"
#include "Inventory/InventoryComponent.h"

AHBPlayerController::AHBPlayerController()
{
	InventoryUI = CreateDefaultSubobject<UInventoryUIComponent>(TEXT("InventoryUI"));
}

void AHBPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (TryShowPostMatchResultWidget())
	{
		// 결과 화면(ExtractionResult 맵)에서는 인게임 카메라/입력매핑/HUD 세팅이 필요 없다.
		return;
	}

	if (PlayerCameraManager)
	{
		// 카메라가 머리 뼈에 붙어 있어, 에임오프셋(ABP 의 AimPitchMin/Max, AO_ComboWarrior_Pitch)이
		// 따라갈 수 있는 범위를 넘으면 조준과 화면이 어긋난다. 두 값을 같이 맞춘다.
		PlayerCameraManager->ViewPitchMin = -70.0f;
		PlayerCameraManager->ViewPitchMax = 45.0f;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (PlayerMappingContext)
		{
			InputSubsystem->AddMappingContext(PlayerMappingContext, MappingPriority);
		}
	}

	// 입력 모드와 커서는 HUD 레이어의 InputConfig(Game)가 결정한다.
	// 메뉴가 스택에 올라오면 그쪽 Menu 설정이 이기고, 닫히면 여기로 자동 복귀한다.
	CreateUILayout();
}

bool AHBPlayerController::TryShowPostMatchResultWidget()
{
	// 이 함수는 모든 맵에서 BeginPlay 때마다 호출되므로, ExtractionResult 맵일 때만 동작한다.
	// (구 Level Blueprint의 "Create Widget (WBP_ExtractionResult)" 노드를 대체함 - 그 노드는 제거해야 함)
	const UWorld* World = GetWorld();
	if (!World || !World->GetMapName().Contains(TEXT("ExtractionResult")))
	{
		return false;
	}

	EHBPostMatchReason Reason = EHBPostMatchReason::None;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UHBMatchResultSubsystem* ResultSubsystem = GI->GetSubsystem<UHBMatchResultSubsystem>())
		{
			Reason = ResultSubsystem->GetPostMatchReason();

			// 한 번 보여줬으면 초기화. 다음 매치에서 이전 결과가 남아있지 않도록.
			ResultSubsystem->SetPostMatchReason(EHBPostMatchReason::None);
		}
	}

	const TSubclassOf<UUserWidget> WidgetClassToShow =
		(Reason == EHBPostMatchReason::Died) ? DeathResultWidgetClass : ExtractionResultWidgetClass;

	if (!WidgetClassToShow)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("HBPlayerController | PostMatch result widget class not set | Reason: %d"),
			static_cast<int32>(Reason)
			);

		return true;
	}

	if (UUserWidget* ResultWidget = CreateWidget<UUserWidget>(this, WidgetClassToShow))
	{
		ResultWidget->AddToViewport();

		// 이 맵은 매치와 연결이 끊긴 상태라 팀원을 관전할 수 없다. 관전 버튼은 숨긴다.
		if (UWidget* SpectateButton = ResultWidget->GetWidgetFromName(TEXT("Button_Spectate")))
		{
			SpectateButton->SetVisibility(ESlateVisibility::Collapsed);
		}

		// 결과 화면은 버튼 클릭 등 UI 조작이 필요하므로 마우스 커서와 UI 입력모드를 켠다.
		FInputModeUIOnly InputMode;
		SetInputMode(InputMode);
		SetShowMouseCursor(true);
	}

	return true;
}

void AHBPlayerController::CreateUILayout()
{
	if (UILayout != nullptr || UILayoutClass == nullptr)
	{
		return;
	}

	UILayout = CreateWidget<UHB_UILayout>(this, UILayoutClass);
	if (UILayout == nullptr)
	{
		return;
	}

	// The layout is the only widget added to the viewport directly.
	// Everything else is pushed into one of its activatable widget stacks.
	UILayout->AddToViewport();

	if (HUDLayerClass)
	{
		// 피격 연출 등에서 HUD 를 직접 호출해야 하므로 참조를 들고 있는다
		HUDLayer = Cast<UHB_HUDLayer>(
			UILayout->PushWidgetToLayer(UITag::TAG_UI_Layer_Game, HUDLayerClass));
	}
}

void AHBPlayerController::ClientPlayHitEffect_Implementation(float NewHP, float NewMaxHP, bool bSuppressScreenEffect)
{
	if (!HUDLayer)
	{
		return;
	}

	// HP 바는 막았든 안 막았든 갱신한다. 서버가 보낸 값이 항상 최신이다.
	HUDLayer->UpdateHPBar(NewHP, NewMaxHP);

	// 막아냈으면 붉은 화면은 건너뛴다.
	if (bSuppressScreenEffect)
	{
		return;
	}

	HUDLayer->PlayHitEffect();
}

void AHBPlayerController::OpenContainer(UInventoryComponent* InContainer)
{
	if (InContainer == nullptr || ContainerScreenClass == nullptr)
	{
		return;
	}

	// 이미 열려 있으면 대상만 바꾸지 않고 무시한다.
	// (다른 상자를 열려면 먼저 닫는 것이 흐름상 명확하다)
	if (ContainerScreen != nullptr)
	{
		return;
	}

	if (UILayout == nullptr)
	{
		return;
	}

	// 초기화 함수는 활성화 이전에 실행되므로, 첫 프레임부터 컨테이너가 채워져 있다
	ContainerScreen = UILayout->PushWidgetToLayer<UHB_ContainerScreen>(
		UITag::TAG_UI_Layer_Menu, ContainerScreenClass,
		[InContainer](UHB_ContainerScreen& Screen)
		{
			Screen.SetContainer(InContainer);
		});

	if (ContainerScreen != nullptr)
	{
		// ESC / Tab 처럼 위젯이 스스로 닫히는 경로가 있으므로, 참조를 여기서 정리한다.
		// 이걸 놓치면 이미 닫힌 화면을 열려 있다고 오판한다
		// 뒤지는 동작은 F 를 누르고 있는 동안만 재생한다(InteractionComponent 가 켜고 끈다). 창은 관여하지 않는다.
		ContainerScreen->OnDeactivated().AddWeakLambda(this, [this]()
		{
			ContainerScreen = nullptr;
		});
	}
}

void AHBPlayerController::CloseContainer()
{
	if (ContainerScreen == nullptr)
	{
		return;
	}

	// 스택이 제거하고, 아래에 남은 위젯의 InputConfig 를 다시 계산해 적용한다
	ContainerScreen->DeactivateWidget();
	ContainerScreen = nullptr;
}

void AHBPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// ESC: 열려 있는 메뉴가 없는 게임 입력 상태에서만 여기로 온다. 메뉴가 열려 있으면 그 메뉴가 먼저 받는다.
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHBPlayerController::HandleEscapePressed);
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))	
	{
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(
				MoveAction, ETriggerEvent::Triggered, this, &AHBPlayerController::HandleMove);
		}

		if (LookAction)
		{
			EnhancedInputComponent->BindAction(
				LookAction, ETriggerEvent::Triggered, this, &AHBPlayerController::HandleLook);
		}

		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(
				JumpAction, ETriggerEvent::Started, this, &AHBPlayerController::StartJump);
			EnhancedInputComponent->BindAction(
				JumpAction, ETriggerEvent::Completed, this, &AHBPlayerController::StopJump);
		}

		if (CrouchAction)
		{
			EnhancedInputComponent->BindAction(
				CrouchAction, ETriggerEvent::Started, this, &AHBPlayerController::StartCrouch);
			EnhancedInputComponent->BindAction(
				CrouchAction, ETriggerEvent::Completed, this, &AHBPlayerController::StopCrouch);
		}

		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(
				AttackAction, ETriggerEvent::Started, this, &AHBPlayerController::Attack);

			// 활은 떼는 순간 쏜다. 근접 평타는 떼기를 쓰지 않는다.
			EnhancedInputComponent->BindAction(
				AttackAction, ETriggerEvent::Completed, this, &AHBPlayerController::StopAttack);
			EnhancedInputComponent->BindAction(
				AttackAction, ETriggerEvent::Canceled, this, &AHBPlayerController::StopAttack);
		}

		if (BlockAction)
		{
			EnhancedInputComponent->BindAction(
				BlockAction, ETriggerEvent::Started, this, &AHBPlayerController::Block);
			EnhancedInputComponent->BindAction(
				BlockAction, ETriggerEvent::Completed, this, &AHBPlayerController::StopBlock);
			EnhancedInputComponent->BindAction(
				BlockAction, ETriggerEvent::Canceled, this, &AHBPlayerController::StopBlock);
		}

		if (ToggleInventoryAction)
		{
			EnhancedInputComponent->BindAction(
				ToggleInventoryAction, ETriggerEvent::Started, this, &AHBPlayerController::ToggleInventory);
		}

		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AHBPlayerController::StartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AHBPlayerController::StopSprint);
		}

		if (QuickSlot1Action)
		{
			EnhancedInputComponent->BindAction(
				QuickSlot1Action, ETriggerEvent::Started, this, &AHBPlayerController::UseQuickSlot1);
		}

		if (QuickSlot2Action)
		{
			EnhancedInputComponent->BindAction(
				QuickSlot2Action, ETriggerEvent::Started, this, &AHBPlayerController::UseQuickSlot2);
		}

		if (QuickSlot3Action)
		{
			EnhancedInputComponent->BindAction(
				QuickSlot3Action, ETriggerEvent::Started, this, &AHBPlayerController::UseQuickSlot3);
		}

		if (QuickSlot4Action)
		{
			EnhancedInputComponent->BindAction(
				QuickSlot4Action, ETriggerEvent::Started, this, &AHBPlayerController::UseQuickSlot4);
		}

		if (Skill1Action)
		{
			EnhancedInputComponent->BindAction(
				Skill1Action, ETriggerEvent::Started, this, &AHBPlayerController::ActivateSkill1);
		}

		if (Skill2Action)
		{
			EnhancedInputComponent->BindAction(
				Skill2Action, ETriggerEvent::Started, this, &AHBPlayerController::ActivateSkill2);
		}

		if (SkillAugmentAction)
		{
			EnhancedInputComponent->BindAction(
				SkillAugmentAction, ETriggerEvent::Started, this, &AHBPlayerController::ToggleSkillAugment);
		}

	}
}


/////////// Lobby Test /////////////////////////
void AHBPlayerController::TestCreateSteamLobby()
{
	UHBOnlineSessionSubsystem* OnlineSessionSubsystem = 
		GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>();
	
	if (OnlineSessionSubsystem == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Online | Lobby | Subsystem unavailable")
			);
		
		return;
	}
	
	OnlineSessionSubsystem->CreateLobby(10);
}

void AHBPlayerController::TestFindSteamLobby()
{
	UHBOnlineSessionSubsystem* OnlineSessionSubsystem = 
		GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>();
	
	if (OnlineSessionSubsystem == nullptr)
	{
		return;
	}
	
	OnlineSessionSubsystem->FindLobbies();
}

void AHBPlayerController::TestJoinSteamLobby(int32 InSearchResultIndex)
{
	if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
	{
		OnlineSessionSubsystem->JoinLobby(InSearchResultIndex);
	}
}

void AHBPlayerController::TestLeaveSteamLobby()
{
	if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem =
		GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
	{
		OnlineSessionSubsystem->LeaveLobby();
	}
}

void AHBPlayerController::TestConnectMatchServer()
{
	if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem = 
		GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
	{
		OnlineSessionSubsystem->ConnectToMatchServer(TEXT("127.0.0.1:17777"));
	}
}

void AHBPlayerController::TestPublishMatchServer()
{
	if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem =
		GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
	{
		OnlineSessionSubsystem->PublishMatchServer(TEXT("127.0.0.1:17777"));
	}
}

void AHBPlayerController::TestConnectPublishdMatchServer()
{
	if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem = 
		GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
	{
		OnlineSessionSubsystem->ConnectToPublishMatchServer();
	}
}

void AHBPlayerController::TestStartMatch()
{
	if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem = 
		GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
	{
		OnlineSessionSubsystem->StartMatchAsLobbyOwner(FString());
	}
}

// 테스트 전용: 지정한 전용 서버 주소로 매치 시작 이동을 검증한다.
void AHBPlayerController::TestStartMatchAtAddress(const FString& ServerAddress)
{
	if (UHBOnlineSessionSubsystem* OnlineSessionSubsystem =
		GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
	{
		OnlineSessionSubsystem->StartMatchAsLobbyOwner(ServerAddress);
	}
}

void AHBPlayerController::TestExtract()
{
	if (!IsLocalController())
	{
		return;
	}

	ServerTestExtract();
}

void AHBPlayerController::ReturnToHub()
{
	if (!IsLocalController())
	{
		return;
	}

	const FString HubMap =
		TEXT("/Game/Levels/Lobby/L_Lobby");

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("GameFlow | Returning to Hub | Map: %s"),
		*HubMap
	);

	ClientTravel(
		HubMap,
		TRAVEL_Absolute
	);
}

void AHBPlayerController::TestReturnToHub()
{
	ReturnToHub();
}

void AHBPlayerController::ServerTestExtract_Implementation()
{
	AHeavenBreachGameMode* GameMode = GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>();

	if (GameMode == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Test | Extract failed: GameMode unavailable")
		);

		return;
	}

	if (PlayerState == nullptr)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Test | Extract failed: PlayerState unavailable")
		);

		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Test | Request Player Extract")
	);

	GameMode->NotifyPlayerExtracted(PlayerState);

}
//////////////////////////////////////////////

void AHBPlayerController::ReportLocalDungeonState()
{
    if (!IsLocalController() || !PlayerState || GetNetMode() != NM_Client) return;
    auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>();
    if (!Source || Source->GetDungeonSnapshot().GenerationId <= 0) return;
    for (TActorIterator<AHBDungeonRuntime> It(GetWorld()); It; ++It)
    {
        auto* Dungeon = *It;
        const int32 Generation = Source->GetDungeonSnapshot().GenerationId;
        if (ReportedDungeon.Get() == Dungeon && ReportedDungeonGeneration == Generation) return;
        const bool bReady = Dungeon->IsReady() && Dungeon->GetGenerationId() == Generation;
        if (!bReady && Dungeon->State != EHBDungeonRuntimeState::Failed) return;
        ReportedDungeon = Dungeon;
        ReportedDungeonGeneration = Generation;
        ServerReportDungeonLoaded(Source, Generation, bReady);
        return;
    }
}

void AHBPlayerController::ServerReportDungeonLoaded_Implementation(AHeavenBreachGameState* Source, int32 GenerationId, bool bSucceeded)
{
    if (!Source || Source != GetWorld()->GetGameState()) return;
    if (auto* Mode = GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>())
        Mode->ReportDungeonLoaded(this, GenerationId, bSucceeded);
}

void AHBPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
    ReportLocalDungeonState();

	if (PlayerInput == nullptr)
	{
		return;
	}

	auto CheckKey = [this](const FKey& Key, bool& bWasPressed, void (AHBPlayerController::* UseFunc)())
		{
			const bool bNow = PlayerInput->IsPressed(Key);
			if (bNow && !bWasPressed)
			{
				(this->*UseFunc)();
			}
			bWasPressed = bNow;
		};

	CheckKey(EKeys::One, bWasQuickSlot1Pressed, &AHBPlayerController::UseQuickSlot1);
	CheckKey(EKeys::Two, bWasQuickSlot2Pressed, &AHBPlayerController::UseQuickSlot2);
	CheckKey(EKeys::Three, bWasQuickSlot3Pressed, &AHBPlayerController::UseQuickSlot3);
	CheckKey(EKeys::Four, bWasQuickSlot4Pressed, &AHBPlayerController::UseQuickSlot4);
}

void AHBPlayerController::HandleMove(const FInputActionValue& Value)
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		const FVector2D MovementInput = Value.Get<FVector2D>();
		PlayerCharacter->MoveRight(MovementInput.X);
		PlayerCharacter->MoveForward(MovementInput.Y);
	}
}

void AHBPlayerController::HandleLook(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();
	const UHBSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UHBSettingsSubsystem>();
	const float Sensitivity = Settings ? Settings->GetMouseSensitivity() : 1.0f;
	AddYawInput(LookInput.X * Sensitivity);
	AddPitchInput(LookInput.Y * Sensitivity);
}

void AHBPlayerController::StartJump()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->Jump();
	}
}

void AHBPlayerController::StopJump()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StopJumping();
	}
}

void AHBPlayerController::StartCrouch()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->Crouch();
	}
}

void AHBPlayerController::StopCrouch()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->UnCrouch();
	}
}

void AHBPlayerController::Attack()
{
	if (bSpectatingTeammates)
	{
		ServerSpectateNext();
		return;
	}

	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->Attack();
	}
}

void AHBPlayerController::StopAttack()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StopAttack();
	}
}

// 함수 이름과 입력 액션 애셋(BlockAction)은 그대로 둔다.
// 개명하면 입력 매핑 애셋과 BP 참조가 깨진다. 하는 일만 패링으로 바뀌었다.
void AHBPlayerController::Block()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->TryParry();
	}
}

void AHBPlayerController::StopBlock()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->ReleaseParryInput();
	}
}

void AHBPlayerController::ToggleInventory()
{
	if (HUDLayer)
	{
		HUDLayer->ToggleInventory();
	}
}

void AHBPlayerController::RequestCloseInventory()
{
	if (HUDLayer)
	{
		HUDLayer->CloseInventory();
	}
}

void AHBPlayerController::TravelToLevel(const FString& InLevelPath)
{
	ClientTravel(InLevelPath, ETravelType::TRAVEL_Absolute);
}

void AHBPlayerController::StartSprint()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StartSprint();
	}
}

void AHBPlayerController::StopSprint()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StopSprint();
	}
}

void AHBPlayerController::ActivateSkill1()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->ActivateSkill1();
	}
}

void AHBPlayerController::ActivateSkill2()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->ActivateSkill2();
	}
}

void AHBPlayerController::ClientPlayCameraShake_Implementation(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	const UHBSettingsSubsystem* Settings = GetGameInstance()->GetSubsystem<UHBSettingsSubsystem>();
	if (Settings && !Settings->IsCombatCameraShakeEnabled())
	{
		return;
	}
	if (ShakeClass && PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraShake(ShakeClass, FMath::Max(0.f, Scale));
	}
}

void AHBPlayerController::ClientSetPostMatchReason_Implementation(EHBPostMatchReason Reason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UHBMatchResultSubsystem* ResultSubsystem = GI->GetSubsystem<UHBMatchResultSubsystem>())
		{
			ResultSubsystem->SetPostMatchReason(Reason);
		}
	}
}

void AHBPlayerController::UseQuickSlot1()
{
	if (AHB_PlayerState* HBPlayerState = GetPlayerState<AHB_PlayerState>())
	{
		if (UQuickSlotComponent* QuickSlotComp = HBPlayerState->GetQuickSlot())
		{
			QuickSlotComp->Server_UseSlot(0);
		}
	}
}

void AHBPlayerController::UseQuickSlot2()
{
	if (AHB_PlayerState* HBPlayerState = GetPlayerState<AHB_PlayerState>())
	{
		if (UQuickSlotComponent* QuickSlotComp = HBPlayerState->GetQuickSlot())
		{
			QuickSlotComp->Server_UseSlot(1);
		}
	}
}

void AHBPlayerController::UseQuickSlot3()
{
	if (AHB_PlayerState* HBPlayerState = GetPlayerState<AHB_PlayerState>())
	{
		if (UQuickSlotComponent* QuickSlotComp = HBPlayerState->GetQuickSlot())
		{
			QuickSlotComp->Server_UseSlot(2);
		}
	}
}

void AHBPlayerController::UseQuickSlot4()
{
	if (AHB_PlayerState* HBPlayerState = GetPlayerState<AHB_PlayerState>())
	{
		if (UQuickSlotComponent* QuickSlotComp = HBPlayerState->GetQuickSlot())
		{
			QuickSlotComp->Server_UseSlot(3);
		}
	}
}

void AHBPlayerController::ToggleSkillAugment()
{
	if (SkillAugmentScreen != nullptr)
	{
		SkillAugmentScreen->DeactivateWidget();
		SkillAugmentScreen = nullptr;
		return;
	}

	// 남은 선택권이 없으면 열지 않는다. 후보는 레벨업 때만 생기므로
	// 가드가 없으면 카드 0장짜리 빈 화면이 뜬다.
	const AHB_PlayerState* HBPS = GetPlayerState<AHB_PlayerState>();
	if (HBPS == nullptr || HBPS->GetPendingUpgradeChoices() <= 0)
	{
		return;
	}

	SkillAugmentScreen = UHB_SkillAugment::OpenOn(this, SkillAugmentScreenClass);
	if (SkillAugmentScreen != nullptr)
	{
		SkillAugmentScreen->OnDeactivated().AddWeakLambda(this, [this]()
			{
				SkillAugmentScreen = nullptr;
			});
	}
}

void AHBPlayerController::StartSpectatingTeammates()
{
	if (!HasAuthority())
	{
		return;
	}

	// 빙의가 바뀔 때 카메라를 컨트롤러로 되돌리지 않게 한다.
	bAutoManageActiveCameraTarget = false;
	// 폰 없는 클라는 카메라 위치를 안 보내 관련성 기준이 시체에 머문다. 서버가 뷰타깃으로 직접 계산한다.
	if (PlayerCameraManager)
	{
		PlayerCameraManager->bUseClientSideCameraUpdates = false;
	}

	// 바로 넘어가지 않고 내 시체가 쓰러지는 모습을 먼저 보여 준다.
	// 시점은 관전과 같은 복제 변수(SpectateTarget)로 보내서, 클라에서 컨트롤러로 되돌아가는 문제를 피한다.
	APawn* Corpse = LastPossessedPawn.Get();
	if (Corpse && DeathViewDuration > 0.f)
	{
		SetSpectateTarget(Corpse);
		GetWorldTimerManager().SetTimer(DeathViewTimer, this, &AHBPlayerController::OfferSpectate, DeathViewDuration, false);
		return;
	}

	OfferSpectate();
}

void AHBPlayerController::OfferSpectate()
{
	if (!HasAuthority())
	{
		return;
	}

	// 사망 결과 화면을 이 플레이어에게 보여 주기로 했다고 게임모드에 알린다.
	// 매치가 끝날 때 이미 결과를 본 사람은 결과 맵을 건너뛰고 바로 허브로 보낸다.
	if (AHeavenBreachGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr)
	{
		GameMode->MarkDeathResultShown(PlayerState);
	}

	ClientShowDeathResult();
}

void AHBPlayerController::ClientShowDeathResult_Implementation()
{
	if (!IsLocalController() || DeathResultWidget)
	{
		return;
	}

	CloseContainer();

	// 결과 위젯이 지정돼 있지 않으면 예전처럼 바로 관전으로 넘어간다.
	if (!DeathResultWidgetClass)
	{
		ServerBeginSpectate();
		return;
	}

	DeathResultWidget = CreateWidget<UUserWidget>(this, DeathResultWidgetClass);
	if (!DeathResultWidget)
	{
		ServerBeginSpectate();
		return;
	}

	// 사망 결과 화면 뒤에 평소 HUD 가 비치지 않게 한다.
	if (HUDLayer)
	{
		HUDLayer->SetVisibility(ESlateVisibility::Collapsed);
	}

	DeathResultWidget->AddToViewport();

	// WBP 에 만들어 둔 "관전" 버튼을 이름으로 찾아 연결한다. (WBP 가 C++ 부모 클래스를 쓰지 않아서 이름으로 찾는다)
	if (UButton* SpectateButton = Cast<UButton>(DeathResultWidget->GetWidgetFromName(TEXT("Button_Spectate"))))
	{
		SpectateButton->OnClicked.AddUniqueDynamic(this, &AHBPlayerController::HandleDeathSpectateClicked);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HBPlayerController | DeathResult 위젯에 Button_Spectate 가 없다. 관전으로 넘어갈 수 없다."));
	}

	// 버튼을 누를 수 있게 커서를 켠다.
	ApplySpectateInputMode();
}

void AHBPlayerController::HandleDeathSpectateClicked()
{
	ServerBeginSpectate();
}

void AHBPlayerController::ServerBeginSpectate_Implementation()
{
	// 사망 결과를 본 사람만 관전으로 넘어갈 수 있다. 이미 관전 중이면 무시한다.
	if (!bSpectatingTeammates)
	{
		BeginTeammateSpectate();
	}
}

void AHBPlayerController::ClientReturnToHub_Implementation()
{
	ReturnToHub();
}

void AHBPlayerController::ApplySpectateInputMode()
{
	// 사망 결과 화면과 관전 UI 의 버튼을 누를 수 있는 상태: 커서를 켜고 게임 입력도 받는다.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AHBPlayerController::HandleEscapePressed()
{
	if (!IsLocalController())
	{
		return;
	}

	// 결과 맵은 매치와 연결이 끊긴 화면이라 나가기 팝업이 필요 없다. 인게임 UI 가 없는 맵도 마찬가지다.
	const UWorld* World = GetWorld();
	if (!World || World->GetMapName().Contains(TEXT("ExtractionResult")) || UILayout == nullptr)
	{
		return;
	}

	// 이미 떠 있으면 다시 열지 않는다. 닫기는 팝업이 ESC 를 직접 받아서 처리한다.
	if (LeaveMatchPopup != nullptr)
	{
		return;
	}

	OpenLeaveMatchPopup();
}

void AHBPlayerController::OpenLeaveMatchPopup()
{
	if (!LeaveMatchPopupClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("HBPlayerController | LeaveMatchPopupClass 가 지정되지 않아 나가기 팝업을 열 수 없다."));
		return;
	}

	// 살아서 플레이 중인지. 죽었거나 관전 중이면 이미 아이템 처리가 끝난 상태다.
	const AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn());
	const bool bAlive = PlayerCharacter && !PlayerCharacter->IsCorpse() && !bSpectatingTeammates && !DeathResultWidget;

	const FText Title = NSLOCTEXT("LeaveMatch", "Title", "매치 나가기");
	// 질문은 항상 같고, 아이템을 잃는다는 경고는 살아 있을 때만 별도 칸(Text_Warning)에 띄운다.
	const FText Message = NSLOCTEXT("LeaveMatch", "Message", "로비로 나가시겠습니까?");
	const FText Warning = bAlive
		? NSLOCTEXT("LeaveMatch", "WarningAlive", "지금 나가면 사망 처리되어 가져온 아이템을 모두 잃습니다.")
		: FText::GetEmpty();

	LeaveMatchPopup = UHB_ConfirmPopup::OpenOn(
		this,
		LeaveMatchPopupClass,
		Title,
		Message,
		Warning,
		NSLOCTEXT("LeaveMatch", "Confirm", "나가기"),
		NSLOCTEXT("LeaveMatch", "Cancel", "취소"));

	if (LeaveMatchPopup == nullptr)
	{
		return;
	}

	LeaveMatchPopup->OnConfirmed.AddUniqueDynamic(this, &AHBPlayerController::HandleLeaveMatchConfirmed);

	// 닫히면 포인터를 비우고, 관전/사망 결과 화면 중이었다면 버튼을 누를 수 있는 입력 상태로 되돌린다.
	// 스택은 팝업이 닫힐 때 평소 HUD 설정(커서 숨김)으로 되돌리기 때문이다.
	LeaveMatchPopup->OnDeactivated().RemoveAll(this);
	LeaveMatchPopup->OnDeactivated().AddWeakLambda(this, [this]()
		{
			LeaveMatchPopup = nullptr;

			if (bSpectatingTeammates || DeathResultWidget)
			{
				ApplySpectateInputMode();
			}
		});
}

void AHBPlayerController::HandleLeaveMatchConfirmed()
{
	// 살아 있는 채로 나가면 연결이 끊기면서 서버가 사망 처리한다. 이미 죽은 사람은 그대로 나간다.
	ReturnToHub();
}

void AHBPlayerController::BeginTeammateSpectate()
{
	if (!HasAuthority())
	{
		return;
	}

	bSpectatingTeammates = true;

	// 살아 있는 팀원이 없으면 null 이라 SetSpectateTarget 이 무시하고, 시체 화면에 그대로 머문다.
	SetSpectateTarget(FindNextSpectateTarget(nullptr));

	GetWorldTimerManager().SetTimer(SpectateCheckTimer, this, &AHBPlayerController::CheckSpectateTarget, 0.5f, true);
	OnRep_SpectatingTeammates();
	ForceNetUpdate();
}

void AHBPlayerController::OnUnPossess()
{
	LastPossessedPawn = GetPawn();
	Super::OnUnPossess();
}

void AHBPlayerController::PawnLeavingGame()
{
	if (HasAuthority())
	{
		AHeavenBreachGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr;
		// 시체로 남겼으면 Super 가 폰(=시체)을 지우지 않게 여기서 끝낸다.
		if (GameMode && GameMode->HandleAlivePlayerLeaving(this))
		{
			return;
		}
	}

	Super::PawnLeavingGame();
}

void AHBPlayerController::ServerSpectateNext_Implementation()
{
	if (!bSpectatingTeammates)
	{
		return;
	}

	APawn* Target = FindNextSpectateTarget(SpectateTarget);
	SetSpectateTarget(Target);
}

void AHBPlayerController::ServerSpectatePrevious_Implementation()
{
	if (!bSpectatingTeammates)
	{
		return;
	}

	SetSpectateTarget(FindNextSpectateTarget(SpectateTarget, -1));
}

void AHBPlayerController::RequestSpectateNext()
{
	if (bSpectatingTeammates)
	{
		ServerSpectateNext();
	}
}

void AHBPlayerController::RequestSpectatePrevious()
{
	if (bSpectatingTeammates)
	{
		ServerSpectatePrevious();
	}
}

void AHBPlayerController::GetSpectateCandidates(TArray<APawn*>& OutCandidates) const
{
	OutCandidates.Reset();

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (const APlayerState* PS : GameState->PlayerArray)
	{
		AHBPlayerCharacter* Teammate = PS ? Cast<AHBPlayerCharacter>(PS->GetPawn()) : nullptr;
		// 탈출한 폰은 숨겨진 채 남아 있어 건너뛴다.
		if (Teammate && !Teammate->IsCorpse() && !Teammate->IsHidden() && PS != PlayerState)
		{
			OutCandidates.Add(Teammate);
		}
	}
}

APawn* AHBPlayerController::FindNextSpectateTarget(const AActor* Current, int32 Step) const
{
	TArray<APawn*> Alive;
	GetSpectateCandidates(Alive);

	if (Alive.IsEmpty())
	{
		return nullptr;
	}

	const int32 CurrentIndex = Alive.IndexOfByKey(Current);
	if (CurrentIndex == INDEX_NONE)
	{
		// 보던 대상이 이미 목록에 없다(죽음 등). 다음이면 처음 사람, 이전이면 마지막 사람.
		return Step >= 0 ? Alive[0] : Alive.Last();
	}

	const int32 Count = Alive.Num();
	return Alive[((CurrentIndex + Step) % Count + Count) % Count];
}

void AHBPlayerController::CheckSpectateTarget()
{
	const AHBPlayerCharacter* Current = Cast<AHBPlayerCharacter>(SpectateTarget);
	if (Current && !Current->IsCorpse() && !Current->IsHidden())
	{
		return;
	}

	// 생존자가 없으면 지금 화면을 그대로 둔다. 매치가 끝나면 GameMode 가 결과 맵으로 보낸다.
	SetSpectateTarget(FindNextSpectateTarget(SpectateTarget));
}

void AHBPlayerController::SetSpectateTarget(APawn* NewTarget)
{
	if (!HasAuthority() || NewTarget == nullptr)
	{
		return;
	}

	SpectateTarget = NewTarget;
	SetViewTargetWithBlend(NewTarget, 0.f);
	ForceNetUpdate();
}

void AHBPlayerController::OnRep_SpectateTarget()
{
	if (SpectateTarget == nullptr)
	{
		return;
	}

	bAutoManageActiveCameraTarget = false;
	SetViewTarget(SpectateTarget);
}

void AHBPlayerController::OnRep_SpectatingTeammates()
{
	if (!bSpectatingTeammates)
	{
		return;
	}

	// 서버에서 바꾼 플래그는 복제되지 않으므로, 클라도 폰이 없어질 때 카메라를 컨트롤러로 되돌리지 않게 직접 끈다.
	bAutoManageActiveCameraTarget = false;

	if (!IsLocalController() || SpectateWidget)
	{
		return;
	}

	CloseContainer();

	// 사망 결과 화면은 관전으로 넘어가는 순간 닫는다.
	if (DeathResultWidget)
	{
		DeathResultWidget->RemoveFromParent();
		DeathResultWidget = nullptr;
	}

	// 관전 중에는 내 체력/스킬 같은 평소 HUD 를 숨긴다. 관전 UI 만 보이게 한다.
	// 숨기기만 하고 스택에서 빼지 않는다. 입력 모드는 이 레이어의 설정을 따르기 때문이다.
	if (HUDLayer)
	{
		HUDLayer->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 디자인 WBP 가 지정돼 있으면 그것을, 없으면 C++ 로 만든 기본 UI 를 쓴다.
	const TSubclassOf<UHB_SpectateWidget> WidgetClass = SpectateWidgetClass ? SpectateWidgetClass : TSubclassOf<UHB_SpectateWidget>(UHB_SpectateWidget::StaticClass());
	SpectateWidget = CreateWidget<UHB_SpectateWidget>(this, WidgetClass);
	if (SpectateWidget)
	{
		SpectateWidget->AddToViewport();

		// 관전 UI 의 이전/다음 버튼을 누를 수 있게 커서를 켠다. 관전 중에는 팀원의 카메라로 보기 때문에
		// 마우스로 시점을 돌릴 일이 없다. 빈 화면 좌클릭은 그대로 게임으로 가서 "다음 팀원"이 된다.
		ApplySpectateInputMode();
	}
}

void AHBPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHBPlayerController, bSpectatingTeammates, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AHBPlayerController, SpectateTarget, COND_OwnerOnly);
}

void AHBPlayerController::ShowHudNotification(const FText& Body)
{
	if (!IsLocalController() || !HUDLayer)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!HBNotify::ShouldShow(LastHudNotificationText, LastHudNotificationTime, Body, Now, HudNotificationRepeatInterval))
	{
		return;
	}

	LastHudNotificationText = Body;
	LastHudNotificationTime = Now;
	HUDLayer->ShowNotification(FText::GetEmpty(), Body);
}

void AHBPlayerController::ClientShowHudNotification_Implementation(const FText& Body)
{
	ShowHudNotification(Body);
}

void AHBPlayerController::HBGiveArrows(const int32 Count)
{
	ServerGiveArrows(Count);
}

void AHBPlayerController::HBGiveItem(FName RowName, int32 Count)
{
	ServerGiveItem(RowName, FMath::Max(1, Count));
}

void AHBPlayerController::ServerGiveItem_Implementation(FName RowName, int32 Count)
{
#if !UE_BUILD_SHIPPING
	const AHBPlayerCharacter* Avatar = GetPawn<AHBPlayerCharacter>();
	UInventoryComponent* Inventory = Avatar ? Avatar->GetInventory() : nullptr;
	if (Inventory)
	{
		Inventory->DebugGiveItemByRow(RowName, FMath::Max(1, Count));
	}
#endif
}

void AHBPlayerController::ServerGiveArrows_Implementation(const int32 Count)
{
#if !UE_BUILD_SHIPPING
	AHBPlayerCharacter* Avatar = GetPawn<AHBPlayerCharacter>();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Avatar);
	UInventoryComponent* Inventory = Avatar ? Avatar->GetInventory() : nullptr;
	if (!ASC || !Inventory || Count <= 0)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UGA_PlayerBowAttack* Bow = Cast<UGA_PlayerBowAttack>(Spec.Ability);
		if (Bow && !Bow->GetArrowItemRow().IsNull())
		{
			const bool bAdded = Inventory->AddGold(Bow->GetArrowItemRow(), Count);
			UE_LOG(LogTemp, Warning, TEXT("[Bow] 테스트 화살 %d개 지급 %s"), Count, bAdded ? TEXT("성공") : TEXT("실패(가방 공간)"));
			return;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[Bow] 테스트 화살 지급 실패 — 화살 행이 지정된 활 GA 가 없음"));
#endif
}
