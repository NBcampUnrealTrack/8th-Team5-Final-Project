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
#include "Combat/Damage/HBDamageCalculation.h"
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
#include "Combat/Effect/HBEffectLibrary.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "AI/MeleeTraceComponent.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "Combat/Ability/HBPlayerPassiveGABase.h"

namespace
{
	// GetSkillCooldownRemaining로직 변경으로 필요없어짐
	// 이제 skillid가 곧 태그임
	//FGameplayTag GetSkillCooldownTag(const FName SkillId)
	//{
	//	if (SkillId == TEXT("Warrior.ChargeSlash")) return WarriorTag::TAG_Cooldown_ChargeSlash;
	//	if (SkillId == TEXT("Warrior.CombatRoll")) return WarriorTag::TAG_Cooldown_CombatRoll;
	//	if (SkillId == TEXT("Warrior.Execute")) return WarriorTag::TAG_Cooldown_Execute;
	//	if (SkillId == TEXT("Warrior.BattleCry")) return WarriorTag::TAG_Cooldown_BattleCry;
	//	if (SkillId == TEXT("Warrior.ChainPull")) return WarriorTag::TAG_Cooldown_ChainPull;
	//	if (SkillId == TEXT("Warrior.BloodThirst")) return WarriorTag::TAG_Cooldown_BloodThirst;
	//	if (SkillId == TEXT("Archer.EvasiveShot")) return ArcherTag::TAG_Cooldown_EvasiveShot;
	//	if (SkillId == TEXT("Archer.SwiftMovement")) return ArcherTag::TAG_Cooldown_SwiftMovement;
	//	if (SkillId == TEXT("Archer.PiercingArrow")) return ArcherTag::TAG_Cooldown_PiercingArrow;
	//	if (SkillId == TEXT("Archer.BindingTrap")) return ArcherTag::TAG_Cooldown_BindingTrap;
	//	if (SkillId == TEXT("Archer.MultiShot")) return ArcherTag::TAG_Cooldown_MultiShot;
	//	if (SkillId == TEXT("Archer.TrackingMark")) return ArcherTag::TAG_Cooldown_TrackingMark;
	//	if (SkillId == TEXT("Archer.ExplosiveArrow")) return ArcherTag::TAG_Cooldown_ExplosiveArrow;
	//	if (SkillId == TEXT("Archer.RapidFire")) return ArcherTag::TAG_Cooldown_RapidFire;
	//	if (SkillId == TEXT("Archer.SmokeArrow")) return ArcherTag::TAG_Cooldown_SmokeArrow;
	//	if (SkillId == TEXT("Mage.MagicBolt")) return MageTag::TAG_Cooldown_MagicBolt;
	//	if (SkillId == TEXT("Mage.Teleport")) return MageTag::TAG_Cooldown_Teleport;
	//	if (SkillId == TEXT("Mage.Fireball")) return MageTag::TAG_Cooldown_Fireball;
	//	if (SkillId == TEXT("Mage.IceLance")) return MageTag::TAG_Cooldown_IceLance;
	//	if (SkillId == TEXT("Mage.ChainLightning")) return MageTag::TAG_Cooldown_ChainLightning;
	//	if (SkillId == TEXT("Mage.IceWall")) return MageTag::TAG_Cooldown_IceWall;
	//	if (SkillId == TEXT("Mage.FireZone")) return MageTag::TAG_Cooldown_FireZone;
	//	if (SkillId == TEXT("Mage.MagicShield")) return MageTag::TAG_Cooldown_MagicShield;
	//	if (SkillId == TEXT("Mage.GravityField")) return MageTag::TAG_Cooldown_GravityField;
	//	if (SkillId == TEXT("Mage.ManaExplosion")) return MageTag::TAG_Cooldown_ManaExplosion;
	//	return FGameplayTag();
	//}
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
	FirstPersonCamera->SetupAttachment(GetMesh(), FirstPersonCameraSocketName);
	FirstPersonCamera->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	FirstPersonCamera->SetAbsolute(false, true, true);
	FirstPersonCamera->bUsePawnControlRotation = true;


#pragma region EquipmentVisual
	WeaponMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComp"));
	WeaponMeshComp->SetupAttachment(GetMesh(),WeaponSocketName);
	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComp->ComponentTags.Add(TEXT("Weapon"));
	
	ShieldMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMeshComp"));
	ShieldMeshComp->SetupAttachment(GetMesh(),ShieldSocketName);
	ShieldMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldMeshComp->ComponentTags.Add(TEXT("Shield"));
	
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

	MeleeTraceComp = CreateDefaultSubobject<UMeleeTraceComponent>(TEXT("MeleeTraceComp"));
}

void AHBPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetUnderwearVisible(bUnderwearVisible);
	
	BindToPlayerStateComponents();

	if (HasAuthority() && GetWorld())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		LastManaActivityTime = GetWorld()->GetTimeSeconds();

		GetWorldTimerManager().SetTimer(
			ManaResourceTimer,
			this,
			&AHBPlayerCharacter::UpdateManaResource,
			ManaResourceTickInterval,
			true
		);
	}

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
	//if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	//{
	//	if (ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead)
	//		|| ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking))
	//	{
	//		return;
	//	}
	//
	//	if (bDrawBasicAttackDebug && GetWorld())
	//	{
	//		float DebugRange = GetAttackRange();
	//		float DebugRadius = 50.f;
	//		FColor DebugColor = FColor::Red;
	//		if (JobData && JobData->JobId == TEXT("Archer"))
	//		{
	//			DebugRange = 1800.f;
	//			DebugRadius = 35.f;
	//			DebugColor = FColor::Green;
	//		}
	//		else if (JobData && JobData->JobId == TEXT("Mage"))
	//		{
	//			DebugRange = 1500.f;
	//			DebugRadius = 55.f;
	//			DebugColor = FColor::Blue;
	//		}
	//
	//		const FVector DebugStart = GetPawnViewLocation();
	//		const FVector DebugEnd = DebugStart + GetBaseAimRotation().Vector() * DebugRange;
	//		DrawDebugLine(GetWorld(), DebugStart, DebugEnd, DebugColor, false, BasicAttackDebugDuration, 0, 3.f);
	//		DrawDebugSphere(GetWorld(), DebugEnd, DebugRadius, 12, DebugColor, false, BasicAttackDebugDuration, 0, 2.f);
	//	}
	//
	//	if (JobData && JobData->JobId == TEXT("Archer"))
	//	{
	//		ASC->TryActivateAbilityByClass(UGA_ArcherBasicAttack::StaticClass());
	//		return;
	//	}
	//	if (JobData && JobData->JobId == TEXT("Mage"))
	//	{
	//		ASC->TryActivateAbilityByClass(UGA_MageBasicAttack::StaticClass());
	//		return;
	//	}
	//	ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(StateTag::TAG_State_Attacking));
	//}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) 
		return;

	if (ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead) || ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking)) 
		return;

	const FGameplayTag WeaponTag = GetEquippedWeaponTag();

	// 달리기/방어와 같은 규칙. 마나가 부족하면 몽타주도 안 나간다.
	if (!HasEnoughMana(ModifyManaCost(WeaponTag, AttackManaCost))) 
		return;

	ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(WeaponTag));
}

FGameplayTag AHBPlayerCharacter::GetEquippedWeaponTag() const
{
	FItemInstance WeaponItem;

	// 무기를 들고있다는게 전제조건. 없다면 맨손
	if (GetEquipment() && GetEquipment()->GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
	{
		if (const FWeaponData* WeaponData = WeaponItem.ItemRow.GetRow<FWeaponData>(TEXT("GetEquippedWeaponTag")))
		{
			// DT_Item 의 WeaponType을 태그로 간다.
			// 예시) Weapon.Sword
			const FName TagName(*FString::Printf(TEXT("Weapon.%s"), *WeaponData->WeaponType));

			// 해당 이름의 태그가 있는지 검색
			const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(TagName, false);
			if (WeaponTag.IsValid())
			{
				return WeaponTag;
			}

			UE_LOG(LogTemp, Warning, TEXT("[Attack] 등록되지 않은 무기 태그: %s"), *TagName.ToString());
		}
	}

	// 없다면 디폴트로 언암드 태그반환
	return ItemTag::TAG_Weapon_Unarmed;
}

void AHBPlayerCharacter::StartBlocking()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute()) <= 0.f)
		{
			return;
		}

		StopSprint();
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
	TryActivateJobSkill(SelectedSkillQId);
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
			//ASC->AddLooseGameplayTag(TeamTag::TAG_Team_Player);
			
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

	BindToPlayerStateComponents();
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

	BindToPlayerStateComponents();
}

void AHBPlayerCharacter::BindToPlayerStateComponents()
{
	if (bPlayerStateComponentsBound)
	{
		return;
	}

	UInventoryComponent* Inv = GetInventory();
	UEquipmentComponent* Equip = GetEquipment();
	if (!Inv || !Equip)
	{
		return;
	}

	Inv->OnItemUsed.AddUniqueDynamic(this, &AHBPlayerCharacter::HandleItemUsed);
	Equip->OnEquipmentChanged.AddUniqueDynamic(this, &AHBPlayerCharacter::RefreshEquippedVisuals);
	bPlayerStateComponentsBound = true;

	RefreshEquippedVisuals();
	BP_OnPlayerStateReady();
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
	float AdditionalMultiplier = 1.f;
	if (ASC->HasMatchingGameplayTag(MageTag::TAG_State_MagicShield))
	{
		if (UHB_PlayerAttributeSet* Attributes = HB_PlayerState ? HB_PlayerState->GetPlayerAttributeSet() : nullptr)
		{
			const float ManaSpent = FMath::Min(10.f, Attributes->GetPlayer_Mana());
			Attributes->SetPlayer_Mana(Attributes->GetPlayer_Mana() - ManaSpent);
			if (ManaSpent > 0.f) NotifyManaActivity();
			if (ManaSpent > 0.f) AdditionalMultiplier *= 0.6f;
		}
	}

	float BlockMultiplier = 1.f;
	if (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking) && InstigatorActor)
	{
		const FVector ToAttacker =
			(InstigatorActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		const float FacingDot = FMath::Clamp(
			FVector::DotProduct(GetActorForwardVector(), ToAttacker), -1.f, 1.f);
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FacingDot));
		if (AngleDeg <= GetBlockAngle() * 0.5f)
		{
			BlockMultiplier = 1.f - FMath::Clamp(GetBlockDamageReduction(), 0.f, 1.f);
		}
	}

	const UHB_PlayerAttributeSet* Attributes =
		HB_PlayerState ? HB_PlayerState->GetPlayerAttributeSet() : nullptr;
	const float TargetDefense = Attributes ? Attributes->GetPlayer_Defense() : 0.f;
	const float HitMultiplier = HBDamage::IsHeadshot(Hit) ? 2.f : 1.f;
	const float FinalDamage = HBDamage::CalculateFinalDamage(
		RawDamage, TargetDefense, HitMultiplier, BlockMultiplier, AdditionalMultiplier);

	LastDamageInstigator = InstigatorActor;
	LastDamageReceivedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_PlayerDamage,1.f,ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		const float HPBefore = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute());
		Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Damage, -FinalDamage);
		const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		const float HPAfter = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute());

		UE_LOG(LogTemp, Warning,
			TEXT("PlayerDamage | Target=%s Raw=%.2f Defense=%.2f HeadMult=%.2f BlockMult=%.2f Final=%.2f HPBefore=%.2f HPAfter=%.2f Applied=%s"),
			*GetNameSafe(this), RawDamage, TargetDefense, HitMultiplier, BlockMultiplier,
			FinalDamage, HPBefore, HPAfter,
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

		// 혈흔 연출과 HP 바도 같은 시점에 보낸다.
		// 복제를 기다리면 PlayerState 주기(1Hz) 때문에 쉐이크/사운드보다
		// 최대 1초 늦게 반응한다. 여기서 보내는 값이 데미지 적용 직후의 서버 값이다
		PC->ClientPlayHitEffect(
			ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute()),
			ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute()));
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

	if (GetEquipment())
	{
		FItemInstance WeaponItem;
		if (GetEquipment()->GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
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
	TryActivateJobSkill(SelectedSkillEId);
}

void AHBPlayerCharacter::TryActivateJobSkill(const FName SkillId)
{
	if (SkillId.IsNone())
	{
		return;
	}

	//if (!HasAuthority())
	//{
	//	ServerTryActivateJobSkill(SkillId);
	//	return;
	//}

	if (SkillId != SelectedSkillQId && SkillId != SelectedSkillEId)
	{
		return;
	}

	//const FHBJobSkillData* Skill = JobData ? JobData->FindActiveSkill(SkillId) : nullptr;
	//UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	//if (!Skill || !Skill->AbilityClass || !ASC)
	//{
	//	return;
	//}
	//
	//const float ManaCost = FMath::Max(0.f, Skill->ManaCost);
	//const float CurrentMana = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute());
	//if (CurrentMana + KINDA_SMALL_NUMBER < ManaCost)
	//{
	//	return;
	//}
	//
	//if (ASC->TryActivateAbilityByClass(Skill->AbilityClass))
	//{
	//	if (ManaCost > 0.f)
	//	{
	//		ASC->ApplyModToAttribute(
	//			UHB_PlayerAttributeSet::GetPlayer_ManaAttribute(), EGameplayModOp::Additive, -ManaCost);
	//	}
	//	NotifyManaActivity();
	//}

	const FHBJobSkillData* Skill = JobData ? JobData->FindActiveSkill(SkillId) : nullptr;
	if (!Skill || !Skill->AbilityClass)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// 마나와 쿨타임은 어빌리티의 CheckCost / ApplyCost 가 처리한다.
	ASC->TryActivateAbilityByClass(Skill->AbilityClass);
}

TArray<FName> AHBPlayerCharacter::ResolveStartingSkillIds() const
{
	if (!JobData)
	{
		return TArray<FName>();
	}

	// 1순위: 계정에 저장된 로드아웃.  미구현. 서버 권위로 만들어야 한다
	// 2순위: 로비에서 고른 값.        미구현. PlayerState 를 거쳐 들어온다
	// 3순위: DA 의 기본값.
	return JobData->InitialSkillIds;
}

void AHBPlayerCharacter::ServerTryActivateJobSkill_Implementation(const FName SkillId)
{
	TryActivateJobSkill(SkillId);
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
	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_State_BloodThirst))
	{
		const int32 UpgradeLevel = GetSkillUpgradeLevel(TEXT("Warrior.BloodThirst"));
		const float LifeStealRatio = UpgradeLevel >= 3 ? 0.3f : UpgradeLevel >= 1 ? 0.25f : 0.2f;
		Attributes->SetPlayer_HP(FMath::Min(
			Attributes->GetPlayer_HP() + Damage * LifeStealRatio,
			Attributes->GetPlayer_MaxHP()));
	}

	if (ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_CombatInstinct))
	{
		HBEffects::ApplyEffect(ASC, UGE_WarriorCombatInstinctStack::StaticClass());
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
		KillingMomentumEffectHandle = HBEffects::ApplyEffect(
			ASC, UGE_WarriorKillingMomentumBuff::StaticClass());
	}
	if (ASC->HasMatchingGameplayTag(MageTag::TAG_Passive_ManaAbsorption))
	{
		Attributes->SetPlayer_Mana(FMath::Min(Attributes->GetPlayer_Mana() + 15.f, Attributes->GetPlayer_MaxMana()));
	}
}

float AHBPlayerCharacter::ModifyManaCost(FGameplayTag SourceTag, float Cost) const
{
	if (Cost <= 0.f)
	{
		return 0.f;
	}
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return Cost;
	}

	// 장비,패시브,버프의 전역 보정은 전부 이 두 어트리뷰트에 모인다.
	const float Flat = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaCostFlatAttribute());
	const float Multiplier = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaCostMultiplierAttribute());

	// 연산 순서는 이 한 곳에만 있다. 정액 먼저, 정률 나중.
	// 반대로 하면 기본값이 작은 스킬에서 정액의 비중이 폭발한다.
	const float Final = (Cost + Flat) * Multiplier;

	// 특정 스킬에만 걸리는 증강은 어트리뷰트로 표현할 수 없다. SourceTag로 여기서 건다.

	// 마나소비는 최소0 이 정상이다
	return FMath::Max(0.f, Final);
}

float AHBPlayerCharacter::ModifyCooldown(FGameplayTag SourceTag, float Duration) const
{
	if (Duration <= 0.f)
	{
		return 0.f;
	}
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return Duration;
	}

	// 장비,패시브,버프의 전역 보정은 전부 이 두 어트리뷰트에 모인다.
	const float Flat = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_CooldownFlatAttribute());
	const float Multiplier = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_CooldownMultiplierAttribute());

	const float Final = (Duration + Flat) * Multiplier;

	// 특정 스킬에만 걸리는 증강은 어트리뷰트로 표현할 수 없다. SourceTag로 여기서 건다.

	// 쿨타임 감소는 최소0.1 이 정상이다
	return FMath::Max(0.1f, Final);
}

bool AHBPlayerCharacter::HasAugment(FGameplayTag AugmentTag) const
{
	if (!AugmentTag.IsValid())
	{
		return false;
	}

	// TODO: PlayerState 의 TakenAugments 를 조회하도록 채운다.

	return false;
}

float AHBPlayerCharacter::GetAugmentValue(FGameplayTag AugmentTag) const
{
	if (!AugmentTag.IsValid())
	{
		return 1.f;
	}

	// TODO: 증강 데이터의 Value 를 돌려주도록 채운다.

	return 1.f;
}

bool AHBPlayerCharacter::HasEnoughMana(float Cost) const
{
	if (Cost <= 0.f) return true;

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute()) >= Cost;
}

void AHBPlayerCharacter::ConsumeMana(float Cost)
{
	if (!HasAuthority() || Cost <= 0.f) 
		return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) 
		return;

	ASC->ApplyModToAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute(), EGameplayModOp::Additive, -Cost);

	// 빼먹으면 공격을 난사해도 3초 뒤부터 마나가 차오른다. 달리기/방어와 같은 규칙.
	NotifyManaActivity();
}

const FGameplayTagContainer* AHBPlayerCharacter::GetSkillCooldownTags(const FName SkillId) const
{
	if (!JobData)
	{
		return nullptr;
	}
	// 스킬 전체를 뒤진다.
	const FHBJobSkillData* Skill = JobData->FindSkillAnywhere(SkillId);
	if (!Skill)
	{
		return nullptr;
	}
	if (!Skill->AbilityClass)
	{
		return nullptr;
	}
	const UGameplayAbility* AbilityCDO = Skill->AbilityClass.GetDefaultObject();
	if (!AbilityCDO)
	{
		return nullptr;
	}
	// GA 의 CheckCooldown 이 읽는 것과 같은 값이다. 쿨다운 GE 애셋의 부여 태그가 유일한 진실.
	const FGameplayTagContainer* CooldownTags = AbilityCDO->GetCooldownTags();
	if (!CooldownTags)
	{
		return nullptr;
	}
	if (CooldownTags->IsEmpty())
	{
		return nullptr;
	}
	return CooldownTags;
}

float AHBPlayerCharacter::GetSkillCooldownRemaining(const FName SkillId) const
{
	const FGameplayTagContainer* CooldownTags = GetSkillCooldownTags(SkillId);
	if (!CooldownTags)
	{
		return 0.f;
	}
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return 0.f;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
	const TArray<float> Remaining = ASC->GetActiveEffectsTimeRemaining(Query);

	// 태그가 하나라도 남아 있으면 CheckCooldown 이 막는다. 그래서 가장 긴 것이 실제 시점이다.
	float Longest = 0.f;
	for (const float Time : Remaining)
	{
		if (Time > Longest)
		{
			Longest = Time;
		}
	}
	return Longest;
}

float AHBPlayerCharacter::GetSkillCooldownPercent(const FName SkillId) const
{
	const FGameplayTagContainer* CooldownTags = GetSkillCooldownTags(SkillId);
	if (!CooldownTags)
	{
		return 0.f;
	}
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return 0.f;
	}

	// 총 시간을 다시 계산하지 않고 GE 에서 직접 꺼낸다.
	// 쿨타임 도중에 장비를 바꿔도 게이지가 튀지 않는다.
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
	const TArray<TPair<float, float>> Pairs = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);

	float Remaining = 0.f;
	float Duration = 0.f;
	for (const TPair<float, float>& Pair : Pairs)
	{
		if (Pair.Key > Remaining)
		{
			Remaining = Pair.Key;
			Duration = Pair.Value;
		}
	}
	if (Duration <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(Remaining / Duration, 0.f, 1.f);
}

float AHBPlayerCharacter::GetSkillManaCost(const FName SkillId) const
{
	if (!JobData)
	{
		return 0.f;
	}
	const FHBJobSkillData* Skill = JobData->FindSkillAnywhere(SkillId);
	if (!Skill)
	{
		return 0.f;
	}
	if (!Skill->AbilityClass)
	{
		return 0.f;
	}
	// 구 GA 는 이 베이스가 아니라 0 이 나온다. 액티브 교체가 끝나면 해결된다.
	const UHBPlayerActiveGABase* ActiveCDO = Cast<UHBPlayerActiveGABase>(Skill->AbilityClass.GetDefaultObject());
	if (!ActiveCDO)
	{
		return 0.f;
	}

	// 발동 때와 같은 깔때기를 태운다. 카드 숫자와 실제 차감이 어긋날 수 없다.
	return ModifyManaCost(ActiveCDO->GetSourceTag(), ActiveCDO->GetBaseManaCost());
}

bool AHBPlayerCharacter::CanUseSkill(const FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return false;
	}
	if (IsSkillOnCooldown(SkillId))
	{
		return false;
	}
	return HasEnoughMana(GetSkillManaCost(SkillId));
}

bool AHBPlayerCharacter::IsSkillOnCooldown(const FName SkillId) const
{
	return GetSkillCooldownRemaining(SkillId) > 0.f;
}

int32 AHBPlayerCharacter::GetSkillUpgradeLevel(const FName SkillId) const
{
	return HB_PlayerState ? HB_PlayerState->GetSkillUpgradeLevel(SkillId) : 0;
}

void AHBPlayerCharacter::ResetCombatInstinct()
{
	ConsecutiveTarget.Reset();
	ConsecutiveHitCount = 0;
}

float AHBPlayerCharacter::GetAttackRange() const
{
	if (GetEquipment())
	{
		FItemInstance WeaponItem;
		if (GetEquipment()->GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
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
	if (GetEquipment() && GetEquipment()->IsSlotOccupied(EEquipSlotType::SecondaryEquip))
	{
		FItemInstance ShieldItem;
		if (GetEquipment()->GetItemInSlot(EEquipSlotType::SecondaryEquip, ShieldItem))
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
	if (GetEquipment() && GetEquipment()->IsSlotOccupied(EEquipSlotType::SecondaryEquip))
	{
		FItemInstance ShieldItem;
		if (GetEquipment()->GetItemInSlot(EEquipSlotType::SecondaryEquip, ShieldItem))
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
	// ActiveSkills / PassiveSkills 는 카탈로그다. 시작 로드아웃에 적힌 것만 장착된다.
	SelectedSkillQId = NAME_None;
	SelectedSkillEId = NAME_None;
	SelectedPassiveSkillIds.Reset();
	for (const FName SkillId : ResolveStartingSkillIds())
	{
		if (SkillId.IsNone())
		{
			continue;
		}
		// 직업 데이터에서 스킬을 찾는다.
		if (JobData->FindActiveSkill(SkillId))
		{
			// 1번째는 q
			if (SelectedSkillQId.IsNone())
			{
				SelectedSkillQId = SkillId;
			}
			// 2번짼 e
			else if (SelectedSkillEId.IsNone())
			{
				SelectedSkillEId = SkillId;
			}
			// 그 외에는 넘어가기
			continue;
		}
		if (JobData->FindPassiveSkill(SkillId))
		{
			SelectedPassiveSkillIds.AddUnique(SkillId);
			continue;
		}

		// 오타를 런타임에 조용히 넘기지 않는다. 이번 Q 미발동이 정확히 이 구멍이었다.
		UE_LOG(LogTemp, Error,
			TEXT("[Job] 시작 스킬 '%s' 가 ActiveSkills/PassiveSkills 어디에도 없다."),
			*SkillId.ToString());
	}
	ApplyLoadout(SelectedSkillQId, SelectedSkillEId, SelectedPassiveSkillIds);

	GrantAbilityIfMissing(AbilitySystem, JobData->UltimateSkill.AbilityClass);
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
	SetUnderwearVisible(bUnderwearVisible);
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

bool AHBPlayerCharacter::AddPassiveSkillFromUpgrade(const FName PassiveSkillId)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponent();
	const FHBJobSkillData* Skill = JobData ? JobData->FindPassiveSkill(PassiveSkillId) : nullptr;
	if (!HasAuthority() || !AbilitySystem || !Skill || !Skill->AbilityClass
		|| SelectedPassiveSkillIds.Contains(PassiveSkillId))
	{
		return false;
	}

	const FGameplayAbilitySpecHandle Handle = AbilitySystem->GiveAbility(
		FGameplayAbilitySpec(Skill->AbilityClass, 1, INDEX_NONE, this));
	LoadoutAbilityHandles.Add(Handle);
	SelectedPassiveSkillIds.Add(PassiveSkillId);
	AbilitySystem->TryActivateAbility(Handle);
	ForceNetUpdate();
	return true;
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
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC
		|| ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead)
		|| ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking)
		|| ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute()) <= 0.f)
	{
		return;
	}

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
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	bIsSprinting = bNewSprinting && ASC
		&& !ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead)
		&& !ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking)
		&& ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute()) > 0.f;
	if (bIsSprinting)
	{
		NotifyManaActivity();
	}
	RefreshMovementSpeed();
}

void AHBPlayerCharacter::NotifyManaActivity()
{
	if (HasAuthority() && GetWorld())
	{
		LastManaActivityTime = GetWorld()->GetTimeSeconds();
	}
}

void AHBPlayerCharacter::UpdateManaResource()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
	{
		return;
	}

	const bool bBlocking = ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking);
	const bool bUsingContinuousMana = bIsSprinting || bBlocking;
	const float CurrentMana = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute());

	if (bUsingContinuousMana)
	{
		NotifyManaActivity();
		const float CostPerSecond = (bIsSprinting ? ModifyManaCost(FGameplayTag(), SprintManaCostPerSecond) : 0.f) 
									+ (bBlocking ? ModifyManaCost(StateTag::TAG_State_Blocking, BlockManaCostPerSecond) : 0.f);
		const float ManaCost = FMath::Min(CurrentMana, CostPerSecond * ManaResourceTickInterval);
		if (ManaCost > 0.f)
		{
			ASC->ApplyModToAttribute(
				UHB_PlayerAttributeSet::GetPlayer_ManaAttribute(), EGameplayModOp::Additive, -ManaCost);
		}
		if (CurrentMana <= ManaCost + KINDA_SMALL_NUMBER)
		{
			StopManaActionsAtZero();
		}
		return;
	}

	if (GetWorld()->GetTimeSeconds() - LastManaActivityTime < ManaRegenDelay)
	{
		return;
	}

	const float MaxMana = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxManaAttribute());
	const float ManaToRestore = FMath::Min(MaxMana - CurrentMana, ManaRegenPerSecond * ManaResourceTickInterval);
	if (ManaToRestore > 0.f)
	{
		ASC->ApplyModToAttribute(
			UHB_PlayerAttributeSet::GetPlayer_ManaAttribute(), EGameplayModOp::Additive, ManaToRestore);
	}
}

void AHBPlayerCharacter::StopManaActionsAtZero()
{
	if (bIsSprinting)
	{
		bIsSprinting = false;
		RefreshMovementSpeed();
		ForceNetUpdate();
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
		ASC && ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking))
	{
		StopBlocking();
	}
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
	RefreshLegsVisual();
}

void AHBPlayerCharacter::RefreshWeaponVisual()
{
	if (!GetEquipment() || !WeaponMeshComp)
	{
		return;
	}
	
	FItemInstance WeaponItem;
	if (GetEquipment()->GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
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
	if (!GetEquipment() || !ShieldMeshComp)
	{
		return;
	}
	
	FItemInstance ShieldItem;
	if (GetEquipment()->GetItemInSlot(EEquipSlotType::SecondaryEquip, ShieldItem))
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
	if (!GetEquipment() || !HelmetMeshComp)
	{
		return;
	}
	
	FItemInstance HelmetItem;
	if (GetEquipment()->GetItemInSlot(EEquipSlotType::Helmet, HelmetItem))
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
	if (!GetEquipment() || !ChestMeshComp)
	{
		return;
	}

	FItemInstance ChestItem;
	if (GetEquipment()->GetItemInSlot(EEquipSlotType::Chest, ChestItem))
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

void AHBPlayerCharacter::RefreshLegsVisual()
{
	if (!GetEquipment() || !LegsMeshComp)
	{
		return;
	}
	FItemInstance LegsItem;
	if (GetEquipment()->GetItemInSlot(EEquipSlotType::Legs, LegsItem))
	{
		if (const FItemData* LegsData = LegsItem.ItemRow.GetRow<FItemData>(TEXT("RefreshLegsVisual")))
		{
			LegsMeshComp->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, *LegsData->WorldMeshPath));
			if (ShoesMeshComp)
			{
				ShoesMeshComp->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, *LegsData->ShoesMeshPath));
			}
			return;
		}
	}
	LegsMeshComp->SetSkeletalMesh(nullptr);
	if (ShoesMeshComp)
	{
		ShoesMeshComp->SetSkeletalMesh(nullptr);
	}
}

#pragma region ModularEquipmentMesh
void AHBPlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetUnderwearVisible(bUnderwearVisible);
}

void AHBPlayerCharacter::SetUnderwearVisible(const bool bVisible)
{
	bUnderwearVisible = bVisible;
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	USkeletalMesh* BodyMesh = CharacterMesh ? CharacterMesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshRenderData* RenderData = BodyMesh ? BodyMesh->GetResourceForRendering() : nullptr;
	if (!RenderData)
	{
		return;
	}
	const int32 MaterialIndex = CharacterMesh->GetMaterialIndex(TEXT("MAT_M_LINGERIE"));
	if (MaterialIndex == INDEX_NONE)
	{
		return;
	}
	for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
	{
		const auto& Sections = RenderData->LODRenderData[LODIndex].RenderSections;
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			if (Sections[SectionIndex].MaterialIndex == MaterialIndex)
			{
				CharacterMesh->ShowMaterialSection(MaterialIndex, SectionIndex, bVisible, LODIndex);
			}
		}
	}
}

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


UInventoryComponent* AHBPlayerCharacter::GetInventory() const
{
	const AHB_PlayerState* PS = GetPlayerState<AHB_PlayerState>();
	return PS ? PS->GetInventory() : nullptr;
}

UEquipmentComponent* AHBPlayerCharacter::GetEquipment() const
{
	const AHB_PlayerState* PS = GetPlayerState<AHB_PlayerState>();
	return PS ? PS->GetEquipment() : nullptr;
}

UQuickSlotComponent* AHBPlayerCharacter::GetQuickSlot() const
{
	const AHB_PlayerState* PS = GetPlayerState<AHB_PlayerState>();
	return PS ? PS->GetQuickSlot() : nullptr;
}
