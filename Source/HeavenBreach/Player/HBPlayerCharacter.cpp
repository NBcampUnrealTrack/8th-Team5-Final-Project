#include "Player/HBPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Item.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/Attack/GA_PlayerAttack.h"
#include "Combat/Block/GA_PlayerBlock.h"
#include "Components/CapsuleComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Core/Tag/Team/HB_TeamTag.h"
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
	Equipment = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
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
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByClass(UGA_PlayerAttack::StaticClass());
	}
}

void AHBPlayerCharacter::StartBlocking()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByClass(UGA_PlayerBlock::StaticClass());
	}
}

void AHBPlayerCharacter::StopBlocking()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		FGameplayTagContainer BlockTag;
		BlockTag.AddTag(StateTag::TAG_State_Blocking);
		ASC->CancelAbilities(&BlockTag,nullptr,nullptr);
	}
}

UAbilitySystemComponent* AHBPlayerCharacter::GetAbilitySystemComponent() const
{
	return HB_PlayerState ? HB_PlayerState->GetAbilitySystemComponent() : nullptr;
}

void AHBPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	HB_PlayerState = GetPlayerState<AHB_PlayerState>();
	
	if (HB_PlayerState)
	{
		UAbilitySystemComponent* ASC = HB_PlayerState->GetAbilitySystemComponent();
		ASC->InitAbilityActorInfo(HB_PlayerState, this);
		
		if (HasAuthority())
		{
			ASC->AddLooseGameplayTag(TeamTag::TAG_Team_Player);
			
			for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
			{
				if (AbilityClass)
				{
					ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass,1,INDEX_NONE,this));
				}
			}
		}
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

bool AHBPlayerCharacter::CanBeDamaged_Implementation(AActor* InstigatorActor) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	
	return ASC && !ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
}

void AHBPlayerCharacter::ApplyMeleeHit_Implementation(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit)
{
	//Player Attribute 데이터 필요
}

float AHBPlayerCharacter::GetAttackPower() const
{
	if (Equipment)
	{
		FItemInstance WeaponItem;
		if (Equipment->GetItemInSlot(EEquipSlotType::PrimaryWeapon,WeaponItem))
		{
			if (const FWeaponData* WeaponData = WeaponItem.ItemRow.GetRow<FWeaponData>(TEXT("GetAttackPower")))
			{
				return WeaponData->AttackPower;
			}
		}
	}
	
	return BaseAttackPower;
}

float AHBPlayerCharacter::GetAttackRange() const
{
	if (Equipment)
	{
		FItemInstance WeaponItem;
		if (Equipment->GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
		{
			if (const FWeaponData* WeaponData = WeaponItem.ItemRow.GetRow<FWeaponData>(TEXT("GetAttackRange")))
			{
				return WeaponData->AttackRange;
			}
		}
	}
	return BaseAttackRange;
}

float AHBPlayerCharacter::GetBlockDamageReduction() const
{
	if (Equipment && Equipment->IsSlotOccupied(EEquipSlotType::SecondaryEquip))
	{
		FItemInstance ShieldItem;
		if (Equipment->GetItemInSlot(EEquipSlotType::SecondaryEquip, ShieldItem))
		{
			if (const FShieldData* ShieldData = ShieldItem.ItemRow.GetRow<FShieldData>(TEXT("GetBlockDamageReduction")))
			{
				return ShieldData->DamageReduction;
			}
		}
	}
	
	return BaseBlockDamageReduction;
}

float AHBPlayerCharacter::GetBlockAngle() const
{
	if (Equipment && Equipment->IsSlotOccupied(EEquipSlotType::SecondaryEquip))
	{
		FItemInstance ShieldItem;
		if (Equipment->GetItemInSlot(EEquipSlotType::SecondaryEquip, ShieldItem))
		{
			if (const FShieldData* ShieldData = ShieldItem.ItemRow.GetRow<FShieldData>(TEXT("GetBlockAngle")))
			{
				return ShieldData->BlockAngle;
			}
		}
	}
	
	return BaseBlockingAngle;
}
