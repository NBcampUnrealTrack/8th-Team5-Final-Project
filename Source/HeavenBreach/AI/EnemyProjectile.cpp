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

// Sets default values
AEnemyProjectile::AEnemyProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SetReplicateMovement(true);

	// 충돌체가 루트여야 한다. 이동 컴포넌트가 움직이는 대상이 루트다.
	CollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComp"));
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	RootComponent = CollisionComp;

	// 해당 메쉬는 충돌할 필요 없음. 충돌은 콜리전컴포넌트가 해준다.
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 미사일 컴포넌트
	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	// 스스로 움직이지 않고 지정된 컴포넌트를 밀어준다.
	MovementComp->UpdatedComponent = CollisionComp;
	// 발사 순간의 속도
	MovementComp->InitialSpeed = 3000.f;
	// 발사 최대의 속도
	MovementComp->MaxSpeed = 3000.f;
	// 진행방향으로 회전시킴. 안키면 화살이 옆을보며 날라가버릴수 있음
	MovementComp->bRotationFollowsVelocity = true;
	// 튕기는 옵션. 그건지금필요없음
	MovementComp->bShouldBounce = false;
	// 중력 영향. 0이면 무보정, 1이면 중력에 오나전영향받음
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

	// 충돌 시 등록
	CollisionComp->OnComponentHit.AddDynamic(this, &AEnemyProjectile::OnHit);
	
	// 영원히 날라가기 방지. 정해진시간동안만 생존한다.
	if (HasAuthority())
	{
		SetLifeSpan(MaxLifeSpan);
	}
}

void AEnemyProjectile::Launch(AActor* InInstigator, float InDamage)
{
	// 서버 전용
	if (!HasAuthority())
	{
		return;
	}

	// 데미지 등록
	if (InDamage >= 0.f)
	{
		// 배율 포함
		Damage = InDamage * DamageMultiplier;
	}

	// 미사일 무브먼트는 충돌 시 바로 멈추기에 자기자신은 OnHit이 발생하지않도록 막는다.
	// 그리고 이거 주인도 등록해둔다.
	if (InInstigator)
	{
		SetInstigator(Cast<APawn>(InInstigator));
		CollisionComp->IgnoreActorWhenMoving(InInstigator, true);

		// 발사자의 팀을 기억한다.
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

	// 판정은 서버 전용이고 클라는 OnRep_Hit으로
	if (!HasAuthority() || bHit)
	{
		return;
	}

	// 발사체 주인은 안맞도록 설정되어잇지만 혹시모르니 제외시킴
	if (OtherActor == this || OtherActor == GetInstigator())
	{
		return;
	}

	UE_LOG(LogEnemyAI, Warning, TEXT("[Proj] OnHit  Actor=%s  Comp=%s"), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));

	bHit = true;


	// 같은 팀이면 피해를 주지 않는다. ASC를 보유하고 보유한 팀태그 리스트에 일부 존재한다면 데미지적용x
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	bool bSameTeam = TargetASC && !OwnerTeamTag.IsEmpty() && TargetASC->HasAnyMatchingGameplayTags(OwnerTeamTag);

	// 적끼리는 데미지가 가해지지않음.
	// 인터페이스를 가지고 있으며 데미지가 가해질수 잇는 대상에게만
	if (!bSameTeam && OtherActor && OtherActor->Implements<UDamageableInterface>() && IDamageableInterface::Execute_CanBeDamaged(OtherActor, GetInstigator()))
	{
		// 데미지 적용
		IDamageableInterface::Execute_ApplyMeleeHit(OtherActor, GetInstigator(), Damage, Hit);
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Projectile] %s → %s (%.0f)"), *GetNameSafe(GetInstigator()), *GetNameSafe(OtherActor), Damage);

	// 바로 파괴되는 액터면 바로 삭제
	if (HitBehavior == false)
	{
		Destroy();
		return;
	}

	// 박힘.움직이는 대상의 위치에 부착시킨다.
	AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);
	SetLifeSpan(StickLifeSpan);

	// 서버에서 값을 바꿔도 RepNotify 는 자동 호출되지 않는다. 직접 부른다.
	OnRep_Hit();
}

void AEnemyProjectile::OnRep_Hit()
{
	//// 정지 시킨다.
	//if (MovementComp)
	//{
	//  // 움직임을 멈춘다.
	//	MovementComp->StopMovementImmediately();
	//	// 이제쓸일없으니 비활성화
	//	MovementComp->Deactivate();
	//}

	// 충돌을 끈다.
	if (CollisionComp)
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

