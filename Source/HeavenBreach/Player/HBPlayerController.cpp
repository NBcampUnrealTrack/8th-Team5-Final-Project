#include "Player/HBPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/HBPlayerCharacter.h"

AHBPlayerController::AHBPlayerController() = default;

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
				BlockAction, ETriggerEvent::Started, this, &AHBPlayerController::StartBlocking);
			EnhancedInputComponent->BindAction(
				BlockAction, ETriggerEvent::Completed, this, &AHBPlayerController::StopBlocking);
		}
	}
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

void AHBPlayerController::StartBlocking()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StartBlocking();
	}
}

void AHBPlayerController::StopBlocking()
{
	if (AHBPlayerCharacter* PlayerCharacter = Cast<AHBPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->StopBlocking();
	}
}
