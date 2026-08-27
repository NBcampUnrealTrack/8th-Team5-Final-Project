#include "Player/HBPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "Item/Item.h"
#include "HBPlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/Attack/GA_PlayerAttack.h"
#include "Combat/Block/GA_PlayerBlock.h"
#include "Player/Abilities/GA_ClassBasicAttacks.h"
#include "Components/CapsuleComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Core/Tag/Team/HB_TeamTag.h"
#include "Core/Tag/Job/HB_JobTag.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
#include "Core/Tag/Mage/HB_MageTag.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerState/HB_PlayerState.h"
#include "Inventory/QuickSlotComponent.h"

#include "Interaction/InteractionComponent.h"
#include "Interaction/InteractionPromptWidget.h"
#include "Net/UnrealNetwork.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/Job/HBJobData.h"
#include "Player/Job/HBJobAttributeEffects.h"
#include "Player/Warrior/Effects/HBWarriorEffectLibrary.h"
#include "Player/Warrior/Effects/HBWarriorGameplayEffects.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Core/Tag/Event/HB_EventTag.h"

namespace
{
	FGameplayTag GetSkillCooldownTag(const FName SkillId)
	{
		if (SkillId == TEXT("Warrior.ChargeSlash")) return WarriorTag::TAG_Cooldown_ChargeSlash;
		if (SkillId == TEXT("Warrior.CombatRoll")) return WarriorTag::TAG_Cooldown_CombatRoll;
		if (SkillId == TEXT("Warrior.Execute")) return WarriorTag::TAG_Cooldown_Execute;
		if (SkillId == TEXT("Warrior.BattleCry")) return WarriorTag::TAG_Cooldown_BattleCry;
		if (SkillId == TEXT("Warrior.ChainPull")) return WarriorTag::TAG_Cooldown_ChainPull;
		if (SkillId == TEXT("Warrior.BloodThirst")) return WarriorTag::TAG_Cooldown_BloodThirst;
		if (SkillId == TEXT("Archer.EvasiveShot")) return ArcherTag::TAG_Cooldown_EvasiveShot;
		if (SkillId == TEXT("Archer.SwiftMovement")) return ArcherTag::TAG_Cooldown_SwiftMovement;
		if (SkillId == TEXT("Archer.PiercingArrow")) return ArcherTag::TAG_Cooldown_PiercingArrow;
		if (SkillId == TEXT("Archer.BindingTrap")) return ArcherTag::TAG_Cooldown_BindingTrap;
		if (SkillId == TEXT("Archer.MultiShot")) return ArcherTag::TAG_Cooldown_MultiShot;
		if (SkillId == TEXT("Archer.TrackingMark")) return ArcherTag::TAG_Cooldown_TrackingMark;
		if (SkillId == TEXT("Archer.ExplosiveArrow")) return ArcherTag::TAG_Cooldown_ExplosiveArrow;
		if (SkillId == TEXT("Archer.RapidFire")) return ArcherTag::TAG_Cooldown_RapidFire;
		if (SkillId == TEXT("Archer.SmokeArrow")) return ArcherTag::TAG_Cooldown_SmokeArrow;
		if (SkillId == TEXT("Mage.MagicBolt")) return MageTag::TAG_Cooldown_MagicBolt;
		if (SkillId == TEXT("Mage.Teleport")) return MageTag::TAG_Cooldown_Teleport;
		if (SkillId == TEXT("Mage.Fireball")) return MageTag::TAG_Cooldown_Fireball;
		if (SkillId == TEXT("Mage.IceLance")) return MageTag::TAG_Cooldown_IceLance;
		if (SkillId == TEXT("Mage.ChainLightning")) return MageTag::TAG_Cooldown_ChainLightning;
		if (SkillId == TEXT("Mage.IceWall")) return MageTag::TAG_Cooldown_IceWall;
		if (SkillId == TEXT("Mage.FireZone")) return MageTag::TAG_Cooldown_FireZone;
		if (SkillId == TEXT("Mage.MagicShield")) return MageTag::TAG_Cooldown_MagicShield;
		if (SkillId == TEXT("Mage.GravityField")) return MageTag::TAG_Cooldown_GravityField;
		if (SkillId == TEXT("Mage.ManaExplosion")) return MageTag::TAG_Cooldown_ManaExplosion;
		return FGameplayTag();
	}
}

AHBPlayerCharacter::AHBPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Equipment = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
	QuickSlot = CreateDefaultSubobject<UQuickSlotComponent>(TEXT("QuickSlot"));
	FirstPersonCamera->SetupAttachment(GetMesh(), FirstPersonCameraSocketName);
	FirstPersonCamera->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	FirstPersonCamera->SetAbsolute(false, true, true);
	FirstPersonCamera->bUsePawnControlRotation = true;


#pragma region EquipmentVisual
	WeaponMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComp"));
	WeaponMeshComp->SetupAttachment(GetMesh(),WeaponSocketName);
	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ShieldMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMeshComp"));
	ShieldMeshComp->SetupAttachment(GetMesh(),ShieldSocketName);
	ShieldMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	#pragma region ModularEquipmentMesh
	HelmetMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HelmetMeshComp"));
	HelmetMeshComp->SetupAttachment(GetMesh());
	HelmetMeshComp->SetLeaderPoseComponent(GetMesh());
	HelmetMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ChestMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMeshComp"));
	ChestMeshComp->SetupAttachment(GetMesh());
	ChestMeshComp->SetLeaderPoseComponent(GetMesh());
	ChestMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LegsMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegsMeshComp"));
	LegsMeshComp->SetupAttachment(GetMesh());
	LegsMeshComp->SetLeaderPoseComponent(GetMesh());
	LegsMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ShoesMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ShoesMeshComp"));
	ShoesMeshComp->SetupAttachment(GetMesh());
	ShoesMeshComp->SetLeaderPoseComponent(GetMesh());
	ShoesMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	#pragma endregion
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

	#pragma region TemporaryEquipmentVisualTest
	ApplyTemporaryEquipmentVisualTest();
	#pragma endregion
}

void AHBPlayerCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass,bIsSprinting);
	DOREPLIFETIME(ThisClass,MovementSpeedMultiplier);
	DOREPLIFETIME(ThisClass,SelectedSkillQId);
	DOREPLIFETIME(ThisClass,SelectedSkillEId);
	DOREPLIFETIME(ThisClass,SelectedPassiveSkillIds);
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
		if (ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead)
			|| ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking))
		{
			return;
		}

		if (bDrawBasicAttackDebug && GetWorld())
		{
			float DebugRange = GetAttackRange();
			float DebugRadius = 50.f;
			FColor DebugColor = FColor::Red;
			if (JobData && JobData->JobId == TEXT("Archer"))
			{
				DebugRange = 1800.f;
				DebugRadius = 35.f;
				DebugColor = FColor::Green;
			}
			else if (JobData && JobData->JobId == TEXT("Mage"))
			{
				DebugRange = 1500.f;
				DebugRadius = 55.f;
				DebugColor = FColor::Blue;
			}

			const FVector DebugStart = GetPawnViewLocation();
			const FVector DebugEnd = DebugStart + GetBaseAimRotation().Vector() * DebugRange;
			DrawDebugLine(GetWorld(), DebugStart, DebugEnd, DebugColor, false, BasicAttackDebugDuration, 0, 3.f);
			DrawDebugSphere(GetWorld(), DebugEnd, DebugRadius, 12, DebugColor, false, BasicAttackDebugDuration, 0, 2.f);
		}

		if (JobData && JobData->JobId == TEXT("Archer"))
		{
			ASC->TryActivateAbilityByClass(UGA_ArcherBasicAttack::StaticClass());
			return;
		}
		if (JobData && JobData->JobId == TEXT("Mage"))
		{
			ASC->TryActivateAbilityByClass(UGA_MageBasicAttack::StaticClass());
			return;
		}
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
		FGameplayEventData Payload;
		Payload.Instigator = this;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventTag::TAG_Event_Block_Release, Payload);
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

void AHBPlayerCharacter::ActivateSkill1()
{
	const FHBJobSkillData* Skill = JobData ? JobData->FindActiveSkill(SelectedSkillQId) : nullptr;
	if (!Skill || !Skill->AbilityClass)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByClass(Skill->AbilityClass);
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
					GrantAbilityIfMissing(ASC, AbilityClass);
				}
			}
		}

		OnAbilitySystemInitialized(ASC);
		ApplyJobAppearance();
	}
}

void AHBPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	HB_PlayerState = GetPlayerState<AHB_PlayerState>();
	
	if (HB_PlayerState)
	{
		UAbilitySystemComponent* ASC = HB_PlayerState->GetAbilitySystemComponent();
		ASC->InitAbilityActorInfo(HB_PlayerState,this);
		OnAbilitySystemInitialized(ASC);
		ApplyJobAppearance();

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
	if (ASC->HasMatchingGameplayTag(MageTag::TAG_State_MagicShield))
	{
		if (UHB_PlayerAttributeSet* Attributes = HB_PlayerState ? HB_PlayerState->GetPlayerAttributeSet() : nullptr)
		{
			const float ManaSpent = FMath::Min(10.f, Attributes->GetPlayer_Mana());
			Attributes->SetPlayer_Mana(Attributes->GetPlayer_Mana() - ManaSpent);
			if (ManaSpent > 0.f) RawDamage *= 0.6f;
		}
	}

	LastDamageInstigator = InstigatorActor;
	LastDamageReceivedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_PlayerDamage,1.f,ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		const float HPBefore = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute());
		Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Damage, -RawDamage);
		const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		const float HPAfter = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute());

		UE_LOG(LogTemp, Warning,
			TEXT("PlayerDamage | Target=%s RawDamage=%.2f HPBefore=%.2f HPAfter=%.2f Applied=%s"),
			*GetNameSafe(this), RawDamage, HPBefore, HPAfter,
			AppliedHandle.WasSuccessfullyApplied() ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("PlayerDamage | Failed to create GameplayEffectSpec | Target=%s Effect=%s"),
			*GetNameSafe(this), *GetNameSafe(GE_PlayerDamage));
	}
	
	if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetController()))
	{
		PC->ClientPlayCameraShake(CS_HitReact);
	}
}

float AHBPlayerCharacter::GetAttackPower() const
{
	float TotalAttackPower = BaseAttackPower;
	if (HB_PlayerState)
	{
		if (const UHB_PlayerAttributeSet* Attributes = HB_PlayerState->GetPlayerAttributeSet())
		{
			TotalAttackPower = Attributes->GetPlayer_AttackPower();
		}
	}

	if (Equipment)
	{
		FItemInstance WeaponItem;
		if (Equipment->GetItemInSlot(EEquipSlotType::PrimaryWeapon,WeaponItem))
		{
			if (const FWeaponData* WeaponData = WeaponItem.ItemRow.GetRow<FWeaponData>(TEXT("GetAttackPower")))
			{
				TotalAttackPower += WeaponData->AttackPower;
			}
		}
	}
	
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_BerserkerRage)
			&& HB_PlayerState && HB_PlayerState->GetPlayerAttributeSet())
		{
			const UHB_PlayerAttributeSet* Attributes = HB_PlayerState->GetPlayerAttributeSet();
			const float HealthRatio = Attributes->GetPlayer_MaxHP() > 0.f
				? Attributes->GetPlayer_HP() / Attributes->GetPlayer_MaxHP() : 1.f;
			TotalAttackPower *= 1.f + (1.f - HealthRatio) * 0.3f;
		}
	}

	return TotalAttackPower;
}

void AHBPlayerCharacter::ActivateSkill2()
{
	const FHBJobSkillData* Skill = JobData ? JobData->FindActiveSkill(SelectedSkillEId) : nullptr;
	if (!Skill || !Skill->AbilityClass)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->TryActivateAbilityByClass(Skill->AbilityClass);
	}
}

float AHBPlayerCharacter::ModifyOutgoingDamage(AActor* Target, float Damage)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !Target)
	{
		return Damage;
	}

	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_ConsecutiveAttack))
	{
		if (ConsecutiveTarget.Get() == Target)
		{
			ConsecutiveHitCount = FMath::Min(ConsecutiveHitCount + 1, 5);
		}
		else
		{
			ConsecutiveTarget = Target;
			ConsecutiveHitCount = 1;
		}
		Damage *= 1.f + (ConsecutiveHitCount - 1) * 0.05f;
	}

	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_WeaknessDetection))
	{
		if (const UAbilitySystemComponent* TargetASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
		{
			float HP = 0.f;
			float MaxHP = 0.f;
			if (TargetASC->HasAttributeSetForAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute()))
			{
				HP = TargetASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute());
				MaxHP = TargetASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute());
			}
			else if (TargetASC->HasAttributeSetForAttribute(UEnemyAttributeSetBase::GetHPAttribute()))
			{
				HP = TargetASC->GetNumericAttribute(UEnemyAttributeSetBase::GetHPAttribute());
				MaxHP = TargetASC->GetNumericAttribute(UEnemyAttributeSetBase::GetMaxHPAttribute());
			}
			if (MaxHP > 0.f && HP / MaxHP <= 0.3f)
			{
				Damage *= 1.25f;
			}
		}
	}

	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_Revenge)
		&& LastDamageInstigator.Get() == Target && GetWorld()
		&& GetWorld()->GetTimeSeconds() - LastDamageReceivedTime <= 5.0)
	{
		Damage *= 1.2f;
	}

	const float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (ASC->HasMatchingGameplayTag(ArcherTag::TAG_Passive_EagleEye) && Distance >= 800.f)
		Damage *= 1.15f;
	if (ASC->HasMatchingGameplayTag(ArcherTag::TAG_Passive_LongRangeShot))
		Damage *= 1.f + FMath::Clamp(Distance / 2000.f, 0.f, 1.f) * 0.3f;
	if (ASC->HasMatchingGameplayTag(MageTag::TAG_Passive_ManaAmplification) && HB_PlayerState)
	{
		if (const UHB_PlayerAttributeSet* Attributes = HB_PlayerState->GetPlayerAttributeSet())
		{
			const float Ratio = Attributes->GetPlayer_MaxMana() > 0.f ? Attributes->GetPlayer_Mana() / Attributes->GetPlayer_MaxMana() : 0.f;
			Damage *= 1.f + Ratio * 0.25f;
		}
	}
	if (ASC->HasMatchingGameplayTag(MageTag::TAG_Passive_GlassCannon))
		Damage *= 1.3f;

	return Damage;
}

void AHBPlayerCharacter::NotifyDamageDealt(AActor* Target, float Damage, bool bTargetKilled)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	UHB_PlayerAttributeSet* Attributes = HB_PlayerState ? HB_PlayerState->GetPlayerAttributeSet() : nullptr;
	if (!ASC || !Attributes)
	{
		return;
	}
	if (ASC->HasMatchingGameplayTag(ArcherTag::TAG_Passive_EagleEye))
	{
		Attributes->SetPlayer_Focus(FMath::Min(Attributes->GetPlayer_Focus() + 10.f, Attributes->GetPlayer_MaxFocus()));
	}

	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_State_BloodThirst))
	{
		Attributes->SetPlayer_HP(FMath::Min(
			Attributes->GetPlayer_HP() + Damage * 0.2f,
			Attributes->GetPlayer_MaxHP()));
	}

	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_CombatInstinct))
	{
		HBWarriorEffects::ApplyEffect(ASC, UGE_WarriorCombatInstinctStack::StaticClass());
		GetWorldTimerManager().SetTimer(
			CombatInstinctTimer, this, &AHBPlayerCharacter::ResetCombatInstinct, 4.f, false);
	}

	if (!bTargetKilled)
	{
		return;
	}

	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_CombatRecovery))
	{
		Attributes->SetPlayer_HP(FMath::Min(
			Attributes->GetPlayer_HP() + Attributes->GetPlayer_MaxHP() * 0.1f,
			Attributes->GetPlayer_MaxHP()));
	}

	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_KillingMomentum))
	{
		if (KillingMomentumEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(KillingMomentumEffectHandle);
		}
		KillingMomentumEffectHandle = HBWarriorEffects::ApplyEffect(
			ASC, UGE_WarriorKillingMomentumBuff::StaticClass());
	}
	if (ASC->HasMatchingGameplayTag(MageTag::TAG_Passive_ManaAbsorption))
	{
		Attributes->SetPlayer_Mana(FMath::Min(Attributes->GetPlayer_Mana() + 15.f, Attributes->GetPlayer_MaxMana()));
	}
}

float AHBPlayerCharacter::GetSkillCooldownRemaining(const FName SkillId) const
{
	return HBWarriorEffects::GetCooldownRemaining(GetAbilitySystemComponent(), GetSkillCooldownTag(SkillId));
}

bool AHBPlayerCharacter::IsSkillOnCooldown(const FName SkillId) const
{
	return HBWarriorEffects::IsCooldownActive(GetAbilitySystemComponent(), GetSkillCooldownTag(SkillId));
}

void AHBPlayerCharacter::ResetCombatInstinct()
{
	ConsecutiveTarget.Reset();
	ConsecutiveHitCount = 0;
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
	InitializeJob(AbilitySystem);
}

void AHBPlayerCharacter::InitializeJob(UAbilitySystemComponent* AbilitySystem)
{
	if (!HasAuthority() || bJobInitialized || !AbilitySystem || !JobData)
	{
		return;
	}

	bJobInitialized = true;

	AbilitySystem->RemoveLooseGameplayTag(JobTag::TAG_Job_Warrior);
	AbilitySystem->RemoveLooseGameplayTag(JobTag::TAG_Job_Archer);
	AbilitySystem->RemoveLooseGameplayTag(JobTag::TAG_Job_Mage);
	if (JobData->JobId == TEXT("Warrior")) AbilitySystem->AddLooseGameplayTag(JobTag::TAG_Job_Warrior);
	else if (JobData->JobId == TEXT("Archer"))
	{
		AbilitySystem->AddLooseGameplayTag(JobTag::TAG_Job_Archer);
		GrantAbilityIfMissing(AbilitySystem, UGA_ArcherBasicAttack::StaticClass());
	}
	else if (JobData->JobId == TEXT("Mage"))
	{
		AbilitySystem->AddLooseGameplayTag(JobTag::TAG_Job_Mage);
		GrantAbilityIfMissing(AbilitySystem, UGA_MageBasicAttack::StaticClass());
	}

	TSubclassOf<UGameplayEffect> AttributesEffect = JobData->DefaultAttributesEffect;
	if (!AttributesEffect)
	{
		if (JobData->JobId == TEXT("Warrior")) AttributesEffect = UGE_DefaultAttributes_Warrior::StaticClass();
		else if (JobData->JobId == TEXT("Archer")) AttributesEffect = UGE_DefaultAttributes_Archer::StaticClass();
		else if (JobData->JobId == TEXT("Mage")) AttributesEffect = UGE_DefaultAttributes_Mage::StaticClass();
	}

	if (AttributesEffect)
	{
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
			AttributesEffect, 1.f, AbilitySystem->MakeEffectContext());
		if (Spec.IsValid())
		{
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// ActiveSkills/PassiveSkills are catalogs. Only entries listed in BasicSkills
	// are equipped as the initial loadout.
	SelectedSkillQId = NAME_None;
	SelectedSkillEId = NAME_None;
	SelectedPassiveSkillIds.Reset();
	for (const FHBJobSkillData& BaseSkill : JobData->GetBasicSkillCatalog())
	{
		if (JobData->FindActiveSkill(BaseSkill.SkillId))
		{
			if (SelectedSkillQId.IsNone()) SelectedSkillQId = BaseSkill.SkillId;
			else if (SelectedSkillEId.IsNone()) SelectedSkillEId = BaseSkill.SkillId;
		}
		else if (JobData->FindPassiveSkill(BaseSkill.SkillId))
		{
			SelectedPassiveSkillIds.AddUnique(BaseSkill.SkillId);
		}
		else
		{
			// Compatibility for a mandatory utility ability that is not selectable.
			GrantAbilityIfMissing(AbilitySystem, BaseSkill.AbilityClass);
		}
	}
	ApplyLoadout(SelectedSkillQId, SelectedSkillEId, SelectedPassiveSkillIds);

	GrantAbilityIfMissing(AbilitySystem,
		JobData->UltimateSkill.AbilityClass ? JobData->UltimateSkill.AbilityClass : JobData->UltimateAbility);
}

void AHBPlayerCharacter::ApplyJobAppearance()
{
	if (!JobData)
	{
		return;
	}

	if (USkeletalMesh* JobMesh = JobData->CharacterMesh.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMeshAsset(JobMesh);
	}
	if (JobData->AnimInstanceClass)
	{
		GetMesh()->SetAnimInstanceClass(JobData->AnimInstanceClass);
	}
}

void AHBPlayerCharacter::RequestApplyLoadout(
	const FName SkillQId, const FName SkillEId, const TArray<FName>& PassiveSkillIds)
{
	if (!JobData) return;
	SelectedSkillQId = JobData->FindActiveSkill(SkillQId) ? SkillQId : NAME_None;
	SelectedSkillEId = JobData->FindActiveSkill(SkillEId) ? SkillEId : NAME_None;
	SelectedPassiveSkillIds = PassiveSkillIds;
	if (HasAuthority()) ApplyLoadout(SkillQId, SkillEId, PassiveSkillIds);
	else ServerApplyLoadout(SkillQId, SkillEId, PassiveSkillIds);
}

void AHBPlayerCharacter::ServerApplyLoadout_Implementation(
	const FName SkillQId, const FName SkillEId, const TArray<FName>& PassiveSkillIds)
{
	ApplyLoadout(SkillQId, SkillEId, PassiveSkillIds);
}

void AHBPlayerCharacter::ApplyLoadout(
	const FName SkillQId, const FName SkillEId, TArray<FName> PassiveSkillIds)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	if (!HasAuthority() || !AbilitySystem || !JobData) return;

	for (const FGameplayAbilitySpecHandle& Handle : LoadoutAbilityHandles)
	{
		AbilitySystem->CancelAbilityHandle(Handle);
		AbilitySystem->ClearAbility(Handle);
	}
	LoadoutAbilityHandles.Reset();

	SelectedSkillQId = JobData->FindActiveSkill(SkillQId) ? SkillQId : NAME_None;
	SelectedSkillEId = JobData->FindActiveSkill(SkillEId) ? SkillEId : NAME_None;
	SelectedPassiveSkillIds.Reset();

	auto GrantLoadoutAbility = [this, AbilitySystem](TSubclassOf<UGameplayAbility> AbilityClass, bool bActivate)
	{
		if (!AbilityClass) return;
		const FGameplayAbilitySpecHandle Handle = AbilitySystem->GiveAbility(
			FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		LoadoutAbilityHandles.Add(Handle);
		if (bActivate) AbilitySystem->TryActivateAbility(Handle);
	};

	if (const FHBJobSkillData* Skill = JobData->FindActiveSkill(SelectedSkillQId))
		GrantLoadoutAbility(Skill->AbilityClass, false);
	if (SelectedSkillEId != SelectedSkillQId)
	{
		if (const FHBJobSkillData* Skill = JobData->FindActiveSkill(SelectedSkillEId))
			GrantLoadoutAbility(Skill->AbilityClass, false);
	}

	for (const FName PassiveSkillId : PassiveSkillIds)
	{
		if (const FHBJobSkillData* Skill = JobData->FindPassiveSkill(PassiveSkillId))
		{
			if (!SelectedPassiveSkillIds.Contains(PassiveSkillId))
			{
				SelectedPassiveSkillIds.Add(PassiveSkillId);
				GrantLoadoutAbility(Skill->AbilityClass, true);
			}
		}
	}
}

FGameplayAbilitySpecHandle AHBPlayerCharacter::GrantAbilityIfMissing(
	UAbilitySystemComponent* AbilitySystem,
	TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilitySystem || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}

	if (const FGameplayAbilitySpec* ExistingSpec = AbilitySystem->FindAbilitySpecFromClass(AbilityClass))
	{
		return ExistingSpec->Handle;
	}

	return AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
}

void AHBPlayerCharacter::StartSprint()
{
	bIsSprinting = true;
	RefreshMovementSpeed();
	
	if (!HasAuthority())
	{
		ServerSetSprinting(true);
	}
}

void AHBPlayerCharacter::StopSprint()
{
	bIsSprinting = false;
	RefreshMovementSpeed();
	
	if (!HasAuthority())
	{
		ServerSetSprinting(false);
	}
}

void AHBPlayerCharacter::OnRep_IsSprinting()
{
	RefreshMovementSpeed();
}

void AHBPlayerCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	RefreshMovementSpeed();
}

void AHBPlayerCharacter::SetMovementSpeedMultiplier(const float NewMultiplier)
{
	MovementSpeedMultiplier = FMath::Max(NewMultiplier, 0.1f);
	RefreshMovementSpeed();
}

void AHBPlayerCharacter::OnRep_MovementSpeedMultiplier()
{
	RefreshMovementSpeed();
}

void AHBPlayerCharacter::RefreshMovementSpeed()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		float GASMultiplier = MovementSpeedMultiplier;
		if (HB_PlayerState && HB_PlayerState->GetPlayerAttributeSet())
		{
			GASMultiplier = HB_PlayerState->GetPlayerAttributeSet()->GetPlayer_MoveSpeedMultiplier();
		}
		const float BaseSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
		Movement->MaxWalkSpeed = BaseSpeed * GASMultiplier;
		Movement->MaxWalkSpeedCrouched = CrouchSpeed * GASMultiplier;
	}
}

void AHBPlayerCharacter::RefreshMovementSpeedFromAttributes()
{
	RefreshMovementSpeed();
}

void AHBPlayerCharacter::RefreshEquippedVisuals()
{
	if (bEnableEquipmentVisualTest)
	{
		ApplyTemporaryEquipmentVisualTest();
		return;
	}

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
			if (USkeletalMesh* HelmetMesh = LoadObject<USkeletalMesh>(nullptr, *HelmetData->WorldMeshPath))
			{
				HelmetMeshComp->SetSkeletalMesh(HelmetMesh);
				SetHairVisibleForHelmet(false);
				return;
			}
		}
	}
	
	HelmetMeshComp->SetSkeletalMesh(nullptr);
	SetHairVisibleForHelmet(true);
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

#pragma region ModularEquipmentMesh
void AHBPlayerCharacter::SetHairVisibleForHelmet(const bool bVisible)
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	USkeletalMesh* SkeletalMesh = CharacterMesh ? CharacterMesh->GetSkeletalMeshAsset() : nullptr;
	if (!CharacterMesh || !SkeletalMesh)
	{
		return;
	}

	const int32 HairMaterialIndex = CharacterMesh->GetMaterialIndex(HairMaterialSlotName);
	const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	if (HairMaterialIndex == INDEX_NONE || !RenderData)
	{
		return;
	}

	for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
	{
		const FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
		for (int32 SectionIndex = 0; SectionIndex < LODData.RenderSections.Num(); ++SectionIndex)
		{
			if (LODData.RenderSections[SectionIndex].MaterialIndex == HairMaterialIndex)
			{
				CharacterMesh->ShowMaterialSection(HairMaterialIndex, SectionIndex, bVisible, LODIndex);
			}
		}
	}
}
#pragma endregion

#pragma region TemporaryEquipmentVisualTest
void AHBPlayerCharacter::ApplyTemporaryEquipmentVisualTest()
{
	if (!bEnableEquipmentVisualTest)
	{
		return;
	}

	if (HelmetMeshComp)
	{
		USkeletalMesh* HelmetMesh = TestHelmetMesh.LoadSynchronous();
		HelmetMeshComp->SetSkeletalMesh(HelmetMesh);
		SetHairVisibleForHelmet(HelmetMesh == nullptr);
	}

	if (ChestMeshComp)
	{
		ChestMeshComp->SetSkeletalMesh(TestChestMesh.LoadSynchronous());
	}

	if (LegsMeshComp)
	{
		LegsMeshComp->SetSkeletalMesh(TestLegsMesh.LoadSynchronous());
	}

	if (ShoesMeshComp)
	{
		ShoesMeshComp->SetSkeletalMesh(TestShoesMesh.LoadSynchronous());
	}

	if (WeaponMeshComp)
	{
		WeaponMeshComp->SetStaticMesh(TestWeaponMesh.LoadSynchronous());
	}

	if (ShieldMeshComp)
	{
		ShieldMeshComp->SetStaticMesh(TestShieldMesh.LoadSynchronous());
	}
}
#pragma endregion
