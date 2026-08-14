#include "Player/HBPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Item/Item.h"
#include "HBPlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/Attack/GA_PlayerAttack.h"
#include "Combat/Block/GA_PlayerBlock.h"
#include "Components/CapsuleComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Data/HB_DataTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Core/Tag/Team/HB_TeamTag.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerState/HB_PlayerState.h"

#include "Interaction/InteractionComponent.h"
#include "Interaction/InteractionPromptWidget.h"
#include "Net/UnrealNetwork.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

AHBPlayerCharacter::AHBPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 300.f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Equipment = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	
#pragma region Weapon
	WeaponMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComp"));
	WeaponMeshComp->SetupAttachment(GetMesh(),WeaponSocketName);
	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ShieldMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMeshComp"));
	ShieldMeshComp->SetupAttachment(GetMesh(),ShieldSocketName);
	ShieldMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	HelmetMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HelmetMeshComp"));
	HelmetMeshComp->SetupAttachment(GetMesh(),HelmetSocketName);
	HelmetMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ChestMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMeshComp"));
	ChestMeshComp->SetupAttachment(GetMesh());
	ChestMeshComp->SetLeaderPoseComponent(GetMesh());
	ChestMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
#pragma endregion
}

void AHBPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Inventory)
	{
		Inventory->OnItemUsed.AddDynamic(this, &AHBPlayerCharacter::HandleItemUsed);
	}
	
	if (Equipment)
	{
		Equipment->OnEquipmentChanged.AddDynamic(this, &AHBPlayerCharacter::RefreshEquippedVisuals);
	}
	
	RefreshEquippedVisuals();
}

void AHBPlayerCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass,bIsSprinting);
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
		ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(StateTag::TAG_State_Attacking));
	}
}

void AHBPlayerCharacter::StartBlocking()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(StateTag::TAG_State_Blocking));
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

void AHBPlayerCharacter::ToggleBlocking()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking))
	{
		StopBlocking();
	}
	else
	{
		StartBlocking();
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
	
	HB_PlayerState = GetPlayerState<AHB_PlayerState>();
	
	if (HB_PlayerState)
	{
		HB_PlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(HB_PlayerState,this);

		if (IsLocallyControlled())
		{
			BP_OnPlayerStateReady();
		}
	}
}

bool AHBPlayerCharacter::CanBeDamaged_Implementation(AActor* InstigatorActor) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	
	return ASC && !ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
}

void AHBPlayerCharacter::ApplyMeleeHit_Implementation(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !GE_PlayerDamage)
	{
		return;
	}
	
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_PlayerDamage,1.f,ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(DataTag::TAG_Data_Damage, -RawDamage);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	
	if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetController()))
	{
		PC->ClientPlayCameraShake(CS_HitReact);
	}
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

void AHBPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (!IsLocallyControlled())
	{
		return;
	}

	SetupInteractionPrompt();
}

void AHBPlayerCharacter::SetupInteractionPrompt()
{
	if (InteractionPromptWidgetInstance != nullptr || InteractionPromptClass == nullptr)
	{
		return;
	}

	UInteractionComponent* InteractionComp = FindComponentByClass<UInteractionComponent>();
	if (InteractionComp == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHBPlayerCharacter: UInteractionComponent not found (BP에 추가됐는지 확인)"));
		return;
	}

	InteractionPromptWidgetInstance = CreateWidget<UInteractionPromptWidget>(GetWorld(), InteractionPromptClass);
	if (InteractionPromptWidgetInstance == nullptr)
	{
		return;
	}

	InteractionPromptWidgetInstance->AddToViewport();
	InteractionPromptWidgetInstance->HidePrompt();

	InteractionComp->OnFocusChanged.AddDynamic(this, &AHBPlayerCharacter::HandleInteractableFocusChanged);
}

void AHBPlayerCharacter::HandleInteractableFocusChanged(AActor* NewInteractable)
{
	if (InteractionPromptWidgetInstance == nullptr)
	{
		return;
	}

	if (NewInteractable == nullptr)
	{
		InteractionPromptWidgetInstance->HidePrompt();
		return;
	}

	const UInteractionComponent* InteractionComp = FindComponentByClass<UInteractionComponent>();
	const FText Prompt = InteractionComp ? InteractionComp->GetCurrentPrompt() : FText::GetEmpty();
	InteractionPromptWidgetInstance->ShowPrompt(Prompt);
}

void AHBPlayerCharacter::TestDamage(float Amount)
{
	if (AHB_PlayerState* PS = GetPlayerCurrentState())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			if (UHB_PlayerAttributeSet* AttribSet = PS->GetPlayerAttributeSet())
			{
				float NewHP = AttribSet->GetPlayer_HP() - Amount;
				AttribSet->SetPlayer_HP(FMath::Clamp(NewHP, 0.0f, AttribSet->GetPlayer_MaxHP()));
			}
		}
	}
}

void AHBPlayerCharacter::HandleItemUsed(FName ItemId, float HealAmount)
{
	if (AHB_PlayerState* PS = GetPlayerCurrentState())
	{
		if (UHB_PlayerAttributeSet* AttribSet = PS->GetPlayerAttributeSet())
		{
			const float NewHP = AttribSet->GetPlayer_HP() + HealAmount;
			AttribSet->SetPlayer_HP(FMath::Clamp(NewHP, 0.0f, AttribSet->GetPlayer_MaxHP()));
		}
	}
}

void AHBPlayerCharacter::OnAbilitySystemInitialized(UAbilitySystemComponent* AbilitySystem)
{
}

void AHBPlayerCharacter::StartSprint()
{
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	
	if (!HasAuthority())
	{
		ServerSetSprinting(true);
	}
}

void AHBPlayerCharacter::StopSprint()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	
	if (!HasAuthority())
	{
		ServerSetSprinting(false);
	}
}

void AHBPlayerCharacter::OnRep_IsSprinting()
{
	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
}

void AHBPlayerCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	GetCharacterMovement()->MaxWalkSpeed = bNewSprinting ? SprintSpeed : WalkSpeed;
}

void AHBPlayerCharacter::RefreshEquippedVisuals()
{
	RefreshWeaponVisual();
	RefreshShieldVisual();
	RefreshHelmetVisual();
	RefreshChestVisual();
}

void AHBPlayerCharacter::RefreshWeaponVisual()
{
	if (!Equipment || !WeaponMeshComp)
	{
		return;
	}
	
	FItemInstance WeaponItem;
	if (Equipment->GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
	{
		if (const FWeaponData* WeaponData = WeaponItem.ItemRow.GetRow<FWeaponData>(TEXT("RefreshWeaponVisual")))
		{
			if (UStaticMesh* WeaponMesh = LoadObject<UStaticMesh>(nullptr,*WeaponData->EquipMeshPath))
			{
				WeaponMeshComp->SetStaticMesh(WeaponMesh);
				return;
			}
		}
	}
	
	WeaponMeshComp->SetStaticMesh(nullptr);
}

void AHBPlayerCharacter::RefreshShieldVisual()
{
	if (!Equipment || !ShieldMeshComp)
	{
		return;
	}
	
	FItemInstance ShieldItem;
	if (Equipment->GetItemInSlot(EEquipSlotType::SecondaryEquip, ShieldItem))
	{
		if (const FShieldData* ShieldData = ShieldItem.ItemRow.GetRow<FShieldData>(TEXT("RefreshShieldVisual")))
		{
			if (UStaticMesh* WeaponMesh = LoadObject<UStaticMesh>(nullptr,*ShieldData->EquipMeshPath))
			{
				ShieldMeshComp->SetStaticMesh(WeaponMesh);
				return;
			}
		}
	}
	
	ShieldMeshComp->SetStaticMesh(nullptr);
}

void AHBPlayerCharacter::RefreshHelmetVisual()
{
	if (!Equipment || !HelmetMeshComp)
	{
		return;
	}
	
	FItemInstance HelmetItem;
	if (Equipment->GetItemInSlot(EEquipSlotType::Helmet, HelmetItem))
	{
		if (const FItemData* HelmetData = HelmetItem.ItemRow.GetRow<FItemData>(TEXT("RefreshHelmetVisual")))
		{
			if (UStaticMesh* HelmetMesh = LoadObject<UStaticMesh>(nullptr, *HelmetData->WorldMeshPath))
			{
				HelmetMeshComp->SetStaticMesh(HelmetMesh);
				return;
			}
		}
	}
	
	HelmetMeshComp->SetStaticMesh(nullptr);
}

void AHBPlayerCharacter::RefreshChestVisual()
{
	if (!Equipment || !ChestMeshComp)
	{
		return;
	}

	FItemInstance ChestItem;
	if (Equipment->GetItemInSlot(EEquipSlotType::Chest, ChestItem))
	{
		if (const FItemData* ChestData = ChestItem.ItemRow.GetRow<FItemData>(TEXT("RefreshChestVisual")))
		{
			if (USkeletalMesh* ChestMesh = LoadObject<USkeletalMesh>(nullptr, *ChestData->WorldMeshPath))
			{
				ChestMeshComp->SetSkeletalMesh(ChestMesh);
				return;
			}
		}
	}

	ChestMeshComp->SetSkeletalMesh(nullptr);
}
