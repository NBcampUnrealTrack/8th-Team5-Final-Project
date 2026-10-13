// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnemyProjectile.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Combat/Interface/DamageableInterface.h"
#include "AI/Enum/EAlertState.h"
#include "AI/EnemyCharacterBase.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Core/Tag/Team/HB_TeamTag.h"
#include "Combat/GameplayCue/HitCuePlay.h"

// Sets default values
AEnemyProjectile::AEnemyProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SetReplicateMovement(true);

	CollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComp"));
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	MovementComp->UpdatedComponent = CollisionComp;
	MovementComp->InitialSpeed = 3000.f;
	MovementComp->MaxSpeed = 3000.f;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->bShouldBounce = false;
	MovementComp->ProjectileGravityScale = 0.f;
}

void AEnemyProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnemyProjectile, bHit);
}

// Called when the game starts or when spawned
void AEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionComp->OnComponentHit.AddDynamic(this, &AEnemyProjectile::OnHit);
	
	if (HasAuthority())
	{
		SetLifeSpan(MaxLifeSpan);
	}
}

void AEnemyProjectile::Launch(AActor* InInstigator, float InDamage)
{
	if (!HasAuthority())
	{
		return;
	}

	if (InDamage >= 0.f)
	{
		Damage = InDamage * DamageMultiplier;
	}

	if (InInstigator)
	{
		SetInstigator(Cast<APawn>(InInstigator));
		CollisionComp->IgnoreActorWhenMoving(InInstigator, true);

		OwnerTeamTag.Reset();
		if (UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InInstigator))
		{
			FGameplayTagContainer All;
			OwnerASC->GetOwnedGameplayTags(All);

			FGameplayTagContainer TeamTags;
			TeamTags.AddTag(TeamTag::TAG_Team_Player);
			TeamTags.AddTag(TeamTag::TAG_Team_Enemy);

			OwnerTeamTag = All.Filter(TeamTags);
		}
	}
}

void AEnemyProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{

	if (!HasAuthority() || bHit)
	{
		return;
	}

	if (OtherActor == this || OtherActor == GetInstigator())
	{
		return;
	}

	UE_LOG(LogEnemyAI, Warning, TEXT("[Proj] OnHit  Actor=%s  Comp=%s"), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));

	bHit = true;


	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	bool bSameTeam = TargetASC && !OwnerTeamTag.IsEmpty() && TargetASC->HasAnyMatchingGameplayTags(OwnerTeamTag);

	// 맞는 쪽이 큐를 쐈는가. 막기·패링에 따라 소리가 갈리므로 큐는 맞는 쪽이 쏜다.
	// 벽처럼 피해를 안 받는 대상은 아무도 안 쏘므로 아래에서 여기가 대신 쏜다.
	bool bTargetPlayedCue = false;

	if (!bSameTeam && OtherActor && OtherActor->Implements<UDamageableInterface>() && IDamageableInterface::Execute_CanBeDamaged(OtherActor, GetInstigator()))
	{
		// 맞는 순간에만 발사자에게 투사체 성질을 붙인다. 맞는 쪽이 이걸 보고 소리를 고른다.
		// 이게 없으면 발사자 GA 에 성질 태그가 없어 플레이어에게 맞아도 기본음이 났다.
		HitCuePlay::FScopedImpactTag ImpactScope(GetInstigator(), ImpactTypeTag);

		// 투사체라 원거리다. 패링을 당해도 발사자에게 스태거가 걸리지 않는다.
		IDamageableInterface::Execute_ApplyMeleeHit(OtherActor, GetInstigator(), Damage, Hit, EHBDamageSource::Ranged);

		// 맞는 쪽이 자기 큐를 쐈다. 여기서 또 쏘면 소리가 두 번 난다.
		bTargetPlayedCue = true;
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Projectile] %s �� %s (%.0f)"), *GetNameSafe(GetInstigator()), *GetNameSafe(OtherActor), Damage);

	{
		FGameplayTagContainer ImpactTags;
		if (ImpactTypeTag.IsValid())
		{
			ImpactTags.AddTag(ImpactTypeTag);
		}

		// 벽처럼 피해를 안 받는 대상만 여기서 쏜다.
		//
		// 쳐낼 조건: 막기·패링이면 맞는 쪽이 결과음을 우선해서(PlayHitCue) 관통음은 안 난다.
		// 둘을 겹치려면 맞는 쪽의 큐 선택 순서를 바꿔야 한다.
		if (!bTargetPlayedCue)
		{
			HitCuePlay::PlayImpact(GetInstigator(), OtherActor, Hit, Damage, ImpactTags);
		}
	}

	if (HitBehavior == false)
	{
		Destroy();
		return;
	}

	AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);
	SetLifeSpan(StickLifeSpan);

	OnRep_Hit();
}

void AEnemyProjectile::OnRep_Hit()
{
	//if (MovementComp)
	//{
	//	MovementComp->StopMovementImmediately();
	//	MovementComp->Deactivate();
	//}

	if (CollisionComp)
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

