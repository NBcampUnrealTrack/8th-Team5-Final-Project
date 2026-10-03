// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Attack/HBPlayerArrow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/HBPlayerController.h"

AHBPlayerArrow::AHBPlayerArrow()
{
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComp->InitSphereRadius(4.f);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	// Visibility 는 무시한다. 날아가는 화살이 상호작용 트레이스를 가리지 않게.
	CollisionComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComp->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	MovementComp->UpdatedComponent = CollisionComp;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->bShouldBounce = false;
	// 속도는 Launch 가 정한다. 그 전엔 멈춰 있다.
	MovementComp->bAutoActivate = false;

	TrailComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	TrailComp->SetupAttachment(CollisionComp);

	// 평소엔 꺼 둔다. 벽·사물에 박히면 Visibility(상호작용 트레이스)만 막도록 켠다.
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(CollisionComp);
	PickupSphere->InitSphereRadius(15.f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickupSphere->SetGenerateOverlapEvents(false);
}

void AHBPlayerArrow::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// 클라 트레이스와 서버 판정이 같도록 모든 머신에서 BP 의 PickupRadius 를 반영한다.
	PickupSphere->SetSphereRadius(PickupRadius);
}

void AHBPlayerArrow::Launch(AHBPlayerCharacter* InShooter, const float InSpeed, const float InDamageCoef, const FDataTableRowHandle& InAmmoRow)
{
	Shooter = InShooter;
	DamageCoef = InDamageCoef;
	AmmoItemRow = InAmmoRow;
	SetLifeSpan(ArrowLifeSpan);

	if (InShooter)
	{
		// 쏜 사람 몸(캡슐·메시)에 바로 부딪히지 않게.
		CollisionComp->IgnoreActorWhenMoving(InShooter, true);
	}

	if (HasAuthority())
	{
		CollisionComp->OnComponentHit.AddDynamic(this, &AHBPlayerArrow::OnHit);
	}

	MovementComp->ProjectileGravityScale = GravityScale;
	MovementComp->InitialSpeed = InSpeed;
	MovementComp->MaxSpeed = InSpeed;
	MovementComp->Velocity = GetActorForwardVector() * InSpeed;
	MovementComp->Activate(true);
}

void AHBPlayerArrow::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHBPlayerArrow, AmmoItemRow);
	DOREPLIFETIME(AHBPlayerArrow, StuckState);
}

void AHBPlayerArrow::OnHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (bHasHit || !HasAuthority())
	{
		return;
	}
	bHasHit = true;

	UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 명중  대상=%s  컴포넌트=%s  본=%s"),
		*GetNameSafe(OtherActor), *GetNameSafe(Hit.GetComponent()), *Hit.BoneName.ToString());

	if (OtherActor && OtherActor != Shooter)
	{
		ApplyArrowDamage(OtherActor, Hit);
	}

	StickTo(OtherActor, Hit);
}

void AHBPlayerArrow::ApplyArrowDamage(AActor* Target, const FHitResult& Hit)
{
	if (!Shooter || !Target->Implements<UDamageableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 피해 없음 (Damageable 아님)  대상=%s"), *GetNameSafe(Target));
		return;
	}

	if (!IDamageableInterface::Execute_CanBeDamaged(Target, Shooter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 피해 없음 (대상이 거부)  대상=%s"), *GetNameSafe(Target));
		return;
	}

	// 근접 판정과 같은 순서: 공격력 × 계수 → 패시브 보정 → 맞는 쪽에 수치만 넘김.
	// 헤드샷은 맞는 쪽이 Hit 의 본으로 판단한다.
	float FinalDamage = Shooter->GetAttackPower() * DamageCoef;
	FinalDamage = Shooter->ModifyOutgoingDamage(Target, FinalDamage);

	IDamageableInterface::Execute_ApplyMeleeHit(Target, Shooter, FinalDamage, Hit, EHBDamageSource::Ranged);
	UE_LOG(LogTemp, Warning, TEXT("[Bow] 피해 %.1f → %s"), FinalDamage, *GetNameSafe(Target));

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const bool bKilled = TargetASC && TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
	Shooter->NotifyDamageDealt(Target, FinalDamage, bKilled);

	// 한 발이 맞으면 내구도 1. 빗나가면 안 닳는다(근접 헛스윙과 같다).
	if (UEquipmentComponent* Equipment = Shooter->GetEquipment())
	{
		bool bJustBroke = false;
		Equipment->DamageEquippedItemDurability(EEquipSlotType::PrimaryWeapon, 1.f, bJustBroke);
	}
}

void AHBPlayerArrow::StickTo(AActor* HitActor, const FHitResult& Hit)
{
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	const EHBArrowStuckKind Kind = HBArrow::ResolveStuckKind(HitActor, HitComp);
	if (Kind == EHBArrowStuckKind::None || !IsValid(HitActor))
	{
		Destroy();
		return;
	}

	MovementComp->StopMovementImmediately();
	MovementComp->Deactivate();

	// 맞은 지점에서 조금 더 밀어 넣어 촉이 파묻힌 것처럼 보이게 한다.
	SetActorLocation(GetActorLocation() + GetActorForwardVector() * StickPenetration, false, nullptr, ETeleportType::TeleportPhysics);

	// 몬스터·플레이어는 맞은 본에 붙여 움직임·래그돌을 따라간다(EnemyProjectile HitBehavior 와 같은 방식).
	// 벽·사물은 움직이면서 네트워크로 찾을 수 있는 것만 붙인다. 런타임에 만든 던전 조각처럼 복제되지 않는
	// 정적 물체에 붙이면 클라가 부모를 못 찾아 위치가 어긋날 수 있어서, 그런 곳은 그 자리에 멈춘 채로 둔다.
	UPrimitiveComponent* AttachComp = HitComp;
	FName AttachBone = Hit.BoneName;
	bool bAttach = false;
	if (Kind == EHBArrowStuckKind::Character)
	{
		bAttach = true;
		// 캡슐에 먼저 맞으므로(본 이름 없음) 메시의 최근접 본에 붙인다. 그래야 애니메이션·래그돌을 따라간다.
		if (!Cast<USkeletalMeshComponent>(HitComp))
		{
			if (const ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
			{
				if (USkeletalMeshComponent* CharMesh = HitCharacter->GetMesh())
				{
					const FName ClosestBone = CharMesh->FindClosestBone(Hit.ImpactPoint, nullptr, 0.f, /*bRequirePhysicsAsset*/ true); // IK·가상 본 제외
					if (!ClosestBone.IsNone())
					{
						AttachComp = CharMesh;
						AttachBone = ClosestBone;
					}
				}
			}
		}
	}
	else
	{
		bAttach = HitComp->Mobility == EComponentMobility::Movable && HitComp->IsSupportedForNetworking();
	}

	if (bAttach)
	{
		AttachToComponent(AttachComp, FAttachmentTransformRules::KeepWorldTransform, AttachBone);
	}

	if (Kind == EHBArrowStuckKind::Character)
	{
		HitActor->OnDestroyed.AddDynamic(this, &AHBPlayerArrow::OnStuckActorDestroyed);
		SetLifeSpan(CharacterStickLifeSpan);
	}
	else
	{
		SetLifeSpan(WorldStickLifeSpan);
	}

	StuckState.Kind = Kind;
	// World 는 Actor 를 안 읽고, 복제 안 되는 런타임 던전 조각일 수 있어 비운다.
	StuckState.Actor = (Kind == EHBArrowStuckKind::Character) ? HitActor : nullptr;

	// 서버는 RepNotify 가 자동으로 안 불린다.
	OnRep_StuckState();
	ForceNetUpdate();

	UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 박힘  종류=%s  대상=%s  컴포넌트=%s  본=%s"),
		Kind == EHBArrowStuckKind::World ? TEXT("World") : TEXT("Character"), *GetNameSafe(HitActor),
		bAttach ? *GetNameSafe(AttachComp) : TEXT("(none)"), *AttachBone.ToString());
}

void AHBPlayerArrow::OnRep_StuckState()
{
	if (StuckState.Kind == EHBArrowStuckKind::None)
	{
		return;
	}

	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrailComp->Deactivate();

	if (StuckState.Kind == EHBArrowStuckKind::World)
	{
		PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		return;
	}

	// 맞은 본인 1인칭 화면에선 숨긴다. 머리에 박히면 카메라를 가린다.
	if (const APawn* StuckPawn = Cast<APawn>(StuckState.Actor))
	{
		if (StuckPawn->IsLocallyControlled())
		{
			MeshComp->SetHiddenInGame(true);
		}
	}
}

void AHBPlayerArrow::OnStuckActorDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

bool AHBPlayerArrow::CanInteract_Implementation(AActor* Interactor) const
{
	return StuckState.Kind == EHBArrowStuckKind::World && !AmmoItemRow.IsNull() && !bPickedUp;
}

FText AHBPlayerArrow::GetInteractPrompt_Implementation() const
{
	return PickupPrompt;
}

void AHBPlayerArrow::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor))
	{
		return;
	}

	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(Interactor);
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!Inventory)
	{
		return;
	}

	// 이름은 Gold 지만 아무 행이나 넣는다. 같은 화살 스택이 있으면 합쳐진다.
	if (Inventory->AddGold(AmmoItemRow, 1))
	{
		bPickedUp = true;
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 회수  %s"), *GetNameSafe(Player));
		Destroy();
		return;
	}

	if (AHBPlayerController* PC = Player->GetController<AHBPlayerController>())
	{
		PC->ClientShowHudNotification(BagFullMessage);
	}
}
