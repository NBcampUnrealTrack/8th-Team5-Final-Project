#include "Player/HBPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerState/HB_PlayerState.h"

AHBPlayerCharacter::AHBPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void AHBPlayerCharacter::MoveForward(const float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AHBPlayerCharacter::MoveRight(const float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AHBPlayerCharacter::Attack()
{
}

void AHBPlayerCharacter::StartBlocking()
{
}

void AHBPlayerCharacter::StopBlocking()
{
}

void AHBPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	HB_PlayerState = GetPlayerState<AHB_PlayerState>();
	
	if (HB_PlayerState)
	{
		HB_PlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(HB_PlayerState,this);	
	}
}

void AHBPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	if (HB_PlayerState)
	{
		HB_PlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(HB_PlayerState,this);
	}
}
