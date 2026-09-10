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
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Core/Tag/Cue/HB_CueTag.h"
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
#include "Combat/Ability/HBPlayerSkillGABase.h"
#include "Combat/Ability/Warrior/GA_WarriorParry.h"

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
	// 비용 출처는 일반공격이다. 전투 호흡이 이걸 보고 마나를 깎는다.
	if (!HasEnoughMana(ModifyManaCost(AbilityTag::TAG_Ability_BasicAttack, AttackManaCost)))
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

const FShieldData* AHBPlayerCharacter::GetEquippedShieldData() const
{
	if (!GetEquipment())
	{
		return nullptr;
	}
	if (!GetEquipment()->IsSlotOccupied(EEquipSlotType::SecondaryEquip))
	{
		return nullptr;
	}

	FItemInstance ShieldItem;
	if (!GetEquipment()->GetItemInSlot(EEquipSlotType::SecondaryEquip, ShieldItem))
	{
		return nullptr;
	}

	return ShieldItem.ItemRow.GetRow<FShieldData>(TEXT("GetEquippedShieldData"));
}

bool AHBPlayerCharacter::HasShieldEquipped() const
{
	return GetEquippedShieldData() != nullptr;
}

FGameplayTag AHBPlayerCharacter::GetBlockSourceTag() const
{
	// 방패가 최우선이다. 방패를 끼고 검을 들었으면 방패로 막는다.
	if (HasShieldEquipped())
	{
		return ItemTag::TAG_Block_Shield;
	}

	// 방패가 없으면 무기로 막는다. 무기 태그와 같은 방식으로 이름을 만든다.
	// 예시) Weapon.Sword → Block.Sword
	FItemInstance WeaponItem;
	if (GetEquipment() && GetEquipment()->GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
	{
		if (const FWeaponData* WeaponData = WeaponItem.ItemRow.GetRow<FWeaponData>(TEXT("GetBlockSourceTag")))
		{
			const FName TagName(*FString::Printf(TEXT("Block.%s"), *WeaponData->WeaponType));

			const FGameplayTag BlockTag = FGameplayTag::RequestGameplayTag(TagName, false);
			if (BlockTag.IsValid())
			{
				return BlockTag;
			}

			UE_LOG(LogTemp, Warning, TEXT("[Block] 등록되지 않은 방어 태그: %s"), *TagName.ToString());
		}
	}

	return ItemTag::TAG_Block_Unarmed;
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

		// 기본공격과 같은 구조다. 장착에 맞는 방어 BP 를 태그로 찾아 발동한다.
		// 그 태그를 가진 BP 가 없으면 여기서 조용히 실패한다.
		// 그게 곧 "이 무기로는 방어할 수 없다" 다. 별도 검사를 두지 않는다.
		ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(GetBlockSourceTag()));
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

void AHBPlayerCharacter::ApplyMeleeHit_Implementation(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit, EHBDamageSource Source)
{
	// 피해 확정은 서버 전용. 적 쪽(AEnemyCharacterBase::ApplyMeleeHit_Implementation)과 같은 규칙이다.
	// 왜 넣었나: 이 함수는 BlueprintNativeEvent 라 BP 가 클라이언트에서 부를 수 있고,
	// 그러면 데미지 GE 가 그 클라에만 적용되어 서버 값과 어긋난다.
	// 아래에 패링 판정이 붙으면서 남의 어빌리티까지 발동시키므로 방어선이 필요해졌다.
	// 쳐낼 조건: 클라 예측 피격 연출을 여기서 하게 되면 연출 부분만 가드 밖으로 뺀다.
	if (!HasAuthority())
	{
		return;
	}

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

	// 패링 성립 여부. 아래 GE 적용이 끝난 뒤 경직을 걸 때 다시 본다.
	// 지역 변수라 복제 대상이 늘지 않는다.
	bool bParried = false;

	float BlockMultiplier = 1.f;
	if (InstigatorActor)
	{
		if (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking))
		{
			BlockMultiplier = 1.f - FMath::Clamp(GetBlockDamageReduction(), 0.f, 1.f);

			// 방패는 "공격을 막음"이 조건. 막기 각도 안에 들어온 공격을 성공적으로
			// 막아냈을 때 1 깎는다. 방패가 없으면 슬롯이 비어 있어 조용히 무시된다.
			if (UEquipmentComponent* Equipment = GetEquipment())
			{
				bool bJustBroke = false;
				Equipment->DamageEquippedItemDurability(EEquipSlotType::SecondaryEquip, 1.f, bJustBroke);

			const FVector ToAttacker =
				(InstigatorActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			const float FacingDot = FMath::Clamp(
				FVector::DotProduct(GetActorForwardVector(), ToAttacker), -1.f, 1.f);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FacingDot));
			if (AngleDeg <= GetBlockAngle() * 0.5f)
			{
				// 방어 각도 안이면 일단 기본 감소율. 패링이 성립하면 그 값으로 덮는다.
				//
				// 작동방식: State.Warrior.ParryWindow 는 방어 진입 직후 짧게 붙는 GE 태그이고,
				// GetParryPassive() 는 패링 패시브를 배웠을 때만 설정을 돌려준다.
				// 둘 다여야 패링이고, 감소율은 그 패시브 애셋에 적혀 있다.
				float Reduction = GetBlockDamageReduction();

				// bDebugAlwaysParry 가 켜져 있으면 창 검사를 건너뛴다. 검증용 스위치다.
				// 패시브는 건너뛰지 않는다. 수치를 거기서 읽어야 하기 때문이다.
				bool bInParryWindow = bDebugAlwaysParry;
				if (!bInParryWindow)
				{
					bInParryWindow = ASC->HasMatchingGameplayTag(WarriorTag::TAG_State_ParryWindow);
				}

				const UGA_WarriorParry* ParryPassive = nullptr;
				if (bInParryWindow)
				{
					ParryPassive = GetParryPassive();
				}

				if (ParryPassive)
				{
					Reduction = ParryPassive->GetDamageReduction();
					bParried = true;

					UE_LOG(LogTemp, Warning,
						TEXT("[Parry] 성립 | Blocker=%s Instigator=%s Angle=%.1f Reduction=%.2f Debug=%d"),
						*GetNameSafe(this), *GetNameSafe(InstigatorActor),
						AngleDeg, Reduction, bDebugAlwaysParry ? 1 : 0);
				}

				BlockMultiplier = 1.f - FMath::Clamp(Reduction, 0.f, 1.f);
			}
		}
	}

	const UHB_PlayerAttributeSet* Attributes = HB_PlayerState ? HB_PlayerState->GetPlayerAttributeSet() : nullptr;
	const float TargetDefense = Attributes ? Attributes->GetPlayer_Defense() : 0.f;
	const float HitMultiplier = HBDamage::IsHeadshot(Hit) ? 2.f : 1.f;
	const float FinalDamage = HBDamage::CalculateFinalDamage(RawDamage, TargetDefense, HitMultiplier, BlockMultiplier, AdditionalMultiplier);

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

		// 방어구는 "피해를 받음"이 조건. 장착된 투구/몸통/바지를 동시에 1씩 깎는다.
		if (AppliedHandle.WasSuccessfullyApplied() && FinalDamage > 0.f)
		{
			if (UEquipmentComponent* Equipment = GetEquipment())
			{
				static const EEquipSlotType ArmorSlots[] =
				{
					EEquipSlotType::Helmet,
					EEquipSlotType::Chest,
					EEquipSlotType::Legs
				};
				for (const EEquipSlotType ArmorSlot : ArmorSlots)
				{
					bool bJustBroke = false;
					Equipment->DamageEquippedItemDurability(ArmorSlot, 1.f, bJustBroke);
				}
			}
		}
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
		PC->ClientPlayHitEffect(ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute()), ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute()));
	}

	// 타격 연출. 큐를 쏘는 책임이 맞는 쪽에 있다.
	//
	// 왜 여기인가: 막았는지 패링했는지는 이 함수만 안다. 때리는 쪽에서 쏘면
	// 무기 성질(Slash/Blunt)밖에 모르니 막아도 살점 소리가 난다.
	// 무기 성질보다 결과가 우선이므로 판정한 자리에서 쏜다.
	PlayHitCue(InstigatorActor, Hit, FinalDamage, BlockMultiplier, bParried);

	// 근접만 경직을 준다. 원거리는 피해 감소까지가 보상이다.
	// 왜 여기인가: 피해가 확정된 뒤라야 이 피격으로 죽었는지가 정해지고,
	// 죽은 뒤에 상대를 경직시키는 그림이 안 나온다.
	if (bParried)
	{
		if (Source == EHBDamageSource::Melee)
		{
			ApplyStaggerTo(InstigatorActor);
		}
	}
}

void AHBPlayerCharacter::PlayHitCue(
	AActor* InstigatorActor, const FHitResult& Hit, float FinalDamage,
	float BlockMultiplier, bool bParried) const
{
	if (!InstigatorActor)
	{
		return;
	}

	// 결과가 무기 성질보다 우선이다. 패링 > 방어 > 성질 순으로 본다.
	// 큐를 직접 지정하면 HitCuePlay 가 성질 판정을 건너뛴다.
	FGameplayTag OverrideCue;
	if (bParried)
	{
		OverrideCue = CueTag::TAG_GameplayCue_Hit_Parry;
	}
	else if (BlockMultiplier < 1.f)
	{
		// 각도 안에서 막아 피해가 깎였다. 패링은 아니다.
		OverrideCue = CueTag::TAG_GameplayCue_Hit_Block;
	}

	// 큐를 안 지정하면 HitCuePlay 가 공격자 태그로 성질을 고른다. 기존 동작이다.
	HitCuePlay::PlayImpact(InstigatorActor, const_cast<AHBPlayerCharacter*>(this),
		Hit, FinalDamage, FGameplayTagContainer(), OverrideCue);
}

void AHBPlayerCharacter::ApplyStaggerTo(AActor* Target) const
{
	if (!HasAuthority())
	{
		return;
	}
	if (!Target)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		return;
	}

	const bool bActivated = TargetASC->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag::TAG_Ability_HitReact));

	// 실패하면 대부분 원인이 셋 중 하나다.
	// 상대에게 Ability.HitReact GA 가 없거나, 슈퍼아머 중이거나, 이미 경직 중이다.
	UE_LOG(LogTemp, Warning, TEXT("[Parry] 경직 시도 | Target=%s Activated=%d"),
		*GetNameSafe(Target), bActivated ? 1 : 0);
}

float AHBPlayerCharacter::GetAttackPower() const
{
	// 어트리뷰트를 아래에서도 쓰므로 한 번만 얻어 둔다.
	const UHB_PlayerAttributeSet* Attributes = nullptr;
	if (HB_PlayerState)
	{
		Attributes = HB_PlayerState->GetPlayerAttributeSet();
	}

	float TotalAttackPower = BaseAttackPower;
	if (Attributes)
	{
		TotalAttackPower = Attributes->GetPlayer_AttackPower();
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
	
	if (!Attributes)
	{
		return TotalAttackPower;
	}

	// 전사의 분노: 체력이 낮아질수록 공격력이 서서히 오르고, 임계 체력에서 최대가 된다.
	//
	// 작동방식: 진행도 = (1 - 체력비율) / (1 - 임계비율).
	// 체력 100% 면 0, 임계 체력에서 1 이 되고, 그 아래로는 클램프로 더 오르지 않는다.
	// 예) 임계 0.3 / 최대 0.5 → 체력 65% 에서 +25%, 체력 30% 이하에서 +50%.
	//
	// 보너스가 0 이면 기능이 꺼진 것이므로 패시브 태그를 따로 볼 필요가 없다.
	// 수치를 어트리뷰트로 둔 이유: 밸런스가 패시브 GE 애셋에서 만지고,
	// 증강이나 장비의 같은 효과도 이 어트리뷰트에 모인다.
	//
	// 쳐낼 조건: 선형이 아닌 곡선이 필요해지면 여기에 커브 애셋을 끼운다.
	const float MaxBonus = Attributes->GetPlayer_LowHealthAttackBonus();
	if (MaxBonus <= 0.f)
	{
		return TotalAttackPower;
	}
	const float MaxHP = Attributes->GetPlayer_MaxHP();
	if (MaxHP <= 0.f)
	{
		return TotalAttackPower;
	}

	const float HealthRatio = Attributes->GetPlayer_HP() / MaxHP;
	const float Threshold = Attributes->GetPlayer_LowHealthAttackThreshold();

	// PreAttributeChange 의 클램프가 Threshold <= 0.95 를 보장하므로 0 으로 나눌 일이 없다.
	const float Span = 1.f - Threshold;
	float Progress = (1.f - HealthRatio) / Span;
	Progress = FMath::Clamp(Progress, 0.f, 1.f);

	TotalAttackPower *= 1.f + MaxBonus * Progress;

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
	// 흡혈. 피의 광란 같은 버프 GE 가 Player_LifeStealRatio 를 올리면 그만큼 체력으로 되돌린다.
	//
	// 작동방식: 판정이 캐릭터를 거치므로(MeleeTraceComponent::ApplyDamage 가 부른다) 여기서 처리한다.
	// 어트리뷰트가 0 이면 아무 일도 없다. 버프 태그를 따로 볼 필요가 없다.
	//
	// 수치를 어트리뷰트로 둔 이유: 밸런스 조정이 GE 애셋에서 끝나고,
	// 증강 Lifesteal 과 장비의 흡혈 옵션도 같은 어트리뷰트 하나에 모인다.
	// 상한 100% 는 PreAttributeChange 가 막는다.
	//
	// 쳐낼 조건: 흡혈이 대상에 따라 달라져야 하면(보스는 절반 등) 여기에 분기가 필요해진다.
	const float LifeStealRatio = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_LifeStealRatioAttribute());
	if (LifeStealRatio > 0.f)
	{
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
	float Final = (Cost + Flat) * Multiplier;

	// 전투 호흡: 일반공격 마나만 줄인다. 스킬과 달리기, 방어에는 걸리지 않는다.
	//
	// 작동방식: 일반공격 경로가 Ability.BasicAttack 을 넘기고 여기서 그것만 본다.
	// 스킬은 빈 태그를, 달리기도 빈 태그를, 방어는 State.Blocking 을 넘긴다.
	//
	// 전에는 "넘어온 태그가 지금 든 무기 태그와 같은가" 로 판별했다.
	// 무기를 바꾸면 판단이 흔들리고, 읽는 사람이 의도를 알 수 없었다.
	//
	// 쳐낼 조건: 방어에만 걸리는 마나 감소가 생기면 여기에 State.Blocking 분기를 더한다.
	if (SourceTag == AbilityTag::TAG_Ability_BasicAttack)
	{
		Final *= ASC->GetNumericAttribute(
			UHB_PlayerAttributeSet::GetPlayer_BasicAttackManaCostMultiplierAttribute());
	}

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
	if (!HB_PlayerState)
	{
		return false;
	}

	// 보유 판정의 주인은 PlayerState 다. 폰이 죽고 다시 스폰돼도 유지돼야 하므로
	// 캐릭터에 들고 있으면 안 된다.
	return HB_PlayerState->HasAugment(AugmentTag);
}

bool AHBPlayerCharacter::FindAugmentInfo(FGameplayTag AugmentTag, FHBSkillAugment& Out) const
{
	if (!AugmentTag.IsValid())
	{
		return false;
	}
	if (!JobData)
	{
		return false;
	}

	if (FindAugmentInCatalog(JobData->GetActiveSkillCatalog(), AugmentTag, Out))
	{
		return true;
	}
	if (FindAugmentInCatalog(JobData->GetPassiveSkillCatalog(), AugmentTag, Out))
	{
		return true;
	}

	// 궁극기 증강은 기획에서 폐기됐지만 조회는 해 둔다.
	// 나중에 생겼을 때 "왜 안 뜨지" 로 시간을 버리지 않기 위해서다.
	TArray<FHBJobSkillData> UltimateOnly;
	UltimateOnly.Add(JobData->UltimateSkill);
	return FindAugmentInCatalog(UltimateOnly, AugmentTag, Out);
}

bool AHBPlayerCharacter::FindAugmentInCatalog(
	const TArray<FHBJobSkillData>& Catalog, FGameplayTag AugmentTag, FHBSkillAugment& Out)
{
	for (const FHBJobSkillData& Skill : Catalog)
	{
		if (!Skill.AbilityClass)
		{
			continue;
		}

		// 증강 목록은 GA 애셋의 클래스 기본값이라 인스턴스 없이 CDO 에서 읽는다.
		// 그래서 장착하지 않은 스킬의 증강도 조회된다.
		const UHBPlayerSkillGABase* SkillCDO =
			Cast<UHBPlayerSkillGABase>(Skill.AbilityClass->GetDefaultObject());
		if (!SkillCDO)
		{
			continue;   // 아직 새 베이스를 안 쓰는 스킬
		}

		if (SkillCDO->FindAugment(AugmentTag, Out))
		{
			return true;
		}
	}
	return false;
}

const UGA_WarriorParry* AHBPlayerCharacter::GetParryPassive() const
{
	// 배우지 않았으면 설정을 볼 것도 없다. 태그가 곧 보유 여부다.
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return nullptr;
	}
	if (!ASC->HasMatchingGameplayTag(WarriorTag::TAG_Passive_Parry))
	{
		return nullptr;
	}

	if (CachedParryPassive)
	{
		return CachedParryPassive;
	}
	if (!JobData)
	{
		return nullptr;
	}

	// 직업의 패시브 카탈로그에서 패링 클래스를 찾는다. 한 번만 돈다.
	for (const FHBJobSkillData& Skill : JobData->GetPassiveSkillCatalog())
	{
		if (!Skill.AbilityClass)
		{
			continue;
		}
		const UGA_WarriorParry* Parry = Cast<UGA_WarriorParry>(Skill.AbilityClass->GetDefaultObject());
		if (!Parry)
		{
			continue;
		}
		CachedParryPassive = Parry;
		return CachedParryPassive;
	}

	// 태그는 있는데 클래스가 없다. BP 부모를 GA_WarriorParry 로 안 바꿨을 때 여기 온다.
	UE_LOG(LogTemp, Warning,
		TEXT("[Parry] %s 는 패링 태그를 갖고 있는데 UGA_WarriorParry 패시브를 못 찾았다."),
		*GetNameSafe(this));
	return nullptr;
}

TArray<FHBSkillAugment> AHBPlayerCharacter::GetTakenAugmentInfos() const
{
	TArray<FHBSkillAugment> Result;
	if (!HB_PlayerState)
	{
		return Result;
	}

	// 태그만으로는 UI 에 아무것도 못 그린다. 태그를 애셋의 증강 정보로 되돌린다.
	// 서버에 물어보지 않는다. 태그는 복제되고 GA 애셋은 클라에도 있다.
	const FGameplayTagContainer Taken = HB_PlayerState->GetTakenAugments();
	for (const FGameplayTag& Tag : Taken)
	{
		FHBSkillAugment Found;
		if (!FindAugmentInfo(Tag, Found))
		{
			// 애셋에서 증강을 지웠는데 태그가 남은 경우다. 조용히 넘기면
			// UI 에 빈 칸이 생기고 원인을 못 찾는다.
			UE_LOG(LogTemp, Warning,
				TEXT("[Augment] 보유 태그 '%s' 를 직업 카탈로그에서 못 찾았다."),
				*Tag.GetTagName().ToString());
			continue;
		}
		Result.Add(Found);
	}
	return Result;
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
	// 스킬은 빈 태그를 넘긴다. GetFinalManaCostFrom 과 같은 값이어야 한다.
	return ModifyManaCost(FGameplayTag(), ActiveCDO->GetBaseManaCost());
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
	// 방패가 없으면 무기 방어라 기본값을 쓴다.
	const FShieldData* Shield = GetEquippedShieldData();
	if (!Shield)
	{
		return BaseBlockDamageReduction;
	}
	return Shield->DamageReduction;
}

float AHBPlayerCharacter::GetBlockAngle() const
{
	const FShieldData* Shield = GetEquippedShieldData();
	if (!Shield)
	{
		return BaseBlockingAngle;
	}
	return Shield->BlockAngle;
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

void AHBPlayerCharacter::TestBreakEquipment(int32 Slot)
{
	// 클라이언트 창 콘솔에서 친 경우 여기서 서버로 넘긴다.
	// Equipment는 서버 권한(ROLE_Authority)에서만 실제로 바뀌기 때문에,
	// 클라 쪽에서 곧장 시도하면 항상 조용히 실패한다(Result=false).
	if (!HasAuthority())
	{
		ServerTestBreakEquipment(Slot);
		return;
	}

	UEquipmentComponent* Equipment = GetEquipment();
	if (!Equipment)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TestBreakEquipment] Equipment 컴포넌트 없음"));
		return;
	}

	const EEquipSlotType TargetSlot = static_cast<EEquipSlotType>(Slot);
	bool bJustBroke = false;
	const bool bResult = Equipment->DamageEquippedItemDurability(TargetSlot, 9999.f, bJustBroke);

	UE_LOG(LogTemp, Warning, TEXT("[TestBreakEquipment] Slot=%d Result=%s JustBroke=%s"),
		Slot, bResult ? TEXT("true") : TEXT("false"), bJustBroke ? TEXT("true") : TEXT("false"));
}

void AHBPlayerCharacter::ServerTestBreakEquipment_Implementation(int32 Slot)
{
	TestBreakEquipment(Slot);
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

bool AHBPlayerCharacter::CanSprint() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}

	// 달리기를 막는 상태들.
	//
	// State.Attacking 은 기본공격과 액티브 스킬이 공용으로 쓴다. 한 줄로 둘 다 막힌다.
	// 쳐낼 조건: 특정 스킬만 달리면서 쓰게 하려면 그 스킬이 State.Attacking 을
	// 안 붙이도록 바꾼다. 여기에 예외를 늘리지 말 것.
	if (ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
	{
		return false;
	}
	if (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking))
	{
		return false;
	}
	if (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Attacking))
	{
		return false;
	}
	if (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Staggered))
	{
		return false;
	}

	// 마나가 없으면 달릴 수 없다. 달리는 동안 계속 소모하기 때문이다.
	return ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute()) > 0.f;
}

void AHBPlayerCharacter::StartSprint()
{
	if (!CanSprint())
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
	// 클라의 요청을 그대로 믿지 않는다. 서버가 같은 조건으로 다시 본다.
	bIsSprinting = false;
	if (bNewSprinting)
	{
		bIsSprinting = CanSprint();
	}

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
	// 달리는 도중에 공격이나 경직이 걸리면 여기서 자동으로 풀린다.
	//
	// 왜 여기인가: 입력 경로마다 StopSprint() 를 심으면 새 스킬을 만들 때마다
	// 빼먹는다. 실제로 방어에는 있었고 공격에는 없어서, 달리면서 휘두르면
	// 계속 달리고 있었다. 속도를 계산하는 이 한 곳에서 막는다.
	//
	// 이 함수는 태그·어트리뷰트가 바뀔 때마다 불린다.
	if (bIsSprinting)
	{
		if (!CanSprint())
		{
			bIsSprinting = false;
		}
	}

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
