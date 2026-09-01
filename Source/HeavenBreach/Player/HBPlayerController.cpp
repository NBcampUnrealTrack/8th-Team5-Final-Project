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
#include "CommonActivatableWidget.h"
#include "Core/Tag/UI/HB_UITag.h"
#include "UI/HB_UILayout.h"
#include "UI/HUD/HB_HUDLayer.h"
#include "UI/HB_ContainerScreen.h"

AHBPlayerController::AHBPlayerController()
{
	InventoryUI = CreateDefaultSubobject<UInventoryUIComponent>(TEXT("InventoryUI"));
}

void AHBPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
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

void AHBPlayerController::ClientPlayHitEffect_Implementation(float NewHP, float NewMaxHP)
{
	if (HUDLayer)
	{
		HUDLayer->UpdateHPBar(NewHP, NewMaxHP);
		HUDLayer->PlayHitEffect();
	}
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

void AHBPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

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
	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
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
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->Attack();
	}
}

void AHBPlayerController::Block()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StartBlocking();
	}
}

void AHBPlayerController::StopBlock()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StopBlocking();
	}
}

void AHBPlayerController::ToggleInventory()
{
	if (InventoryUI)
	{
		InventoryUI->ToggleInventory();
	}
}

void AHBPlayerController::RequestCloseInventory()
{
	if (InventoryUI)
	{
		InventoryUI->CloseInventory();
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

void AHBPlayerController::ClientPlayCameraShake_Implementation(TSubclassOf<UCameraShakeBase> ShakeClass)
{
	if (ShakeClass && PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraShake(ShakeClass);
	}
}

void AHBPlayerController::UseQuickSlot1()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		if (UQuickSlotComponent* QuickSlotComp = PlayerCharacter->FindComponentByClass<UQuickSlotComponent>())
		{
			QuickSlotComp->Server_UseSlot(0);
		}
	}
}

void AHBPlayerController::UseQuickSlot2()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		if (UQuickSlotComponent* QuickSlotComp = PlayerCharacter->FindComponentByClass<UQuickSlotComponent>())
		{
			QuickSlotComp->Server_UseSlot(1);
		}
	}
}

void AHBPlayerController::UseQuickSlot3()
{
	APawn* CurrentPawn = GetPawn();
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(CurrentPawn))
	{
		if (UQuickSlotComponent* QuickSlotComp = PlayerCharacter->FindComponentByClass<UQuickSlotComponent>())
		{
			QuickSlotComp->Server_UseSlot(2);
		}
	}
}

void AHBPlayerController::UseQuickSlot4()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		if (UQuickSlotComponent* QuickSlotComp = PlayerCharacter->FindComponentByClass<UQuickSlotComponent>())
		{
			QuickSlotComp->Server_UseSlot(3);
		}
	}
}
