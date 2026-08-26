#include "Player/HBPlayerController.h"

#include "Inventory/InventoryUIComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/HBPlayerCharacter.h"
#include "Inventory/QuickSlotComponent.h"

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

		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
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
	}
}

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
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		const FVector2D MovementInput = Value.Get<FVector2D>();
		PlayerCharacter->MoveRight(MovementInput.X);
		PlayerCharacter->MoveForward(MovementInput.Y);
	}
}

void AHBPlayerController::HandleLook(const FInputActionValue& Value)
{
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

	const FVector2D LookInput = Value.Get<FVector2D>();
	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
}

void AHBPlayerController::StartJump()
{
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

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
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

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
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->Attack();
	}
}

void AHBPlayerController::Block()
{
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StartBlocking();
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

void AHBPlayerController::StartSprint()
{
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

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

void AHBPlayerController::ClientPlayCameraShake_Implementation(TSubclassOf<UCameraShakeBase> ShakeClass)
{
	if (ShakeClass && PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraShake(ShakeClass);
	}
}

void AHBPlayerController::UseQuickSlot1()
{
	UE_LOG(LogTemp, Warning, TEXT("[PC] UseQuickSlot1 called, InventoryOpen=%d"),
		InventoryUI ? InventoryUI->IsInventoryOpen() : -1);
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

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
	UE_LOG(LogTemp, Warning, TEXT("[PC] UseQuickSlot2 called, InventoryOpen=%d"),
		InventoryUI ? InventoryUI->IsInventoryOpen() : -1);
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

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
	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

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
	UE_LOG(LogTemp, Warning, TEXT("[PC] UseQuickSlot4 called, InventoryOpen=%d"),
		InventoryUI ? InventoryUI->IsInventoryOpen() : -1);

	if (InventoryUI && InventoryUI->IsInventoryOpen())
	{
		return;
	}

	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		if (UQuickSlotComponent* QuickSlotComp = PlayerCharacter->FindComponentByClass<UQuickSlotComponent>())
		{
			QuickSlotComp->Server_UseSlot(3);
		}
	}
}