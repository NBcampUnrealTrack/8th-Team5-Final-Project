// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Attack/HBPlayerArrow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "EquipmentComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Player/HBPlayerCharacter.h"

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
}

void AHBPlayerArrow::Launch(AHBPlayerCharacter* InShooter, const float InSpeed, const float InDamageCoef)
{
	Shooter = InShooter;
	DamageCoef = InDamageCoef;
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

void AHBPlayerArrow::OnHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (bHasHit || !HasAuthority())
	{
		return;
	}
	bHasHit = true;

	if (OtherActor && OtherActor != Shooter)
	{
		ApplyArrowDamage(OtherActor, Hit);
	}

	Destroy();
}

void AHBPlayerArrow::ApplyArrowDamage(AActor* Target, const FHitResult& Hit)
{
	if (!Shooter || !Target->Implements<UDamageableInterface>())
	{
		return;
	}

	if (!IDamageableInterface::Execute_CanBeDamaged(Target, Shooter))
	{
		return;
	}

	// 근접 판정과 같은 순서: 공격력 × 계수 → 패시브 보정 → 맞는 쪽에 수치만 넘김.
	// 헤드샷은 맞는 쪽이 Hit 의 본으로 판단한다.
	float FinalDamage = Shooter->GetAttackPower() * DamageCoef;
	FinalDamage = Shooter->ModifyOutgoingDamage(Target, FinalDamage);

	IDamageableInterface::Execute_ApplyMeleeHit(Target, Shooter, FinalDamage, Hit, EHBDamageSource::Ranged);

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
