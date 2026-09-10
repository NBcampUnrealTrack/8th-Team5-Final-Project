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

	// �浹ü�� ��Ʈ���� �Ѵ�. �̵� ������Ʈ�� �����̴� ����� ��Ʈ��.
	CollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComp"));
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	RootComponent = CollisionComp;

	// �ش� �޽��� �浹�� �ʿ� ����. �浹�� �ݸ���������Ʈ�� ���ش�.
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// �̻��� ������Ʈ
	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	// ������ �������� �ʰ� ������ ������Ʈ�� �о��ش�.
	MovementComp->UpdatedComponent = CollisionComp;
	// �߻� ������ �ӵ�
	MovementComp->InitialSpeed = 3000.f;
	// �߻� �ִ��� �ӵ�
	MovementComp->MaxSpeed = 3000.f;
	// ����������� ȸ����Ŵ. ��Ű�� ȭ���� �������� ���󰡹����� ����
	MovementComp->bRotationFollowsVelocity = true;
	// ƨ��� �ɼ�. �װ������ʿ����
	MovementComp->bShouldBounce = false;
	// �߷� ����. 0�̸� ������, 1�̸� �߷¿� �������������
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

	// �浹 �� ���
	CollisionComp->OnComponentHit.AddDynamic(this, &AEnemyProjectile::OnHit);
	
	// ������ ���󰡱� ����. �������ð����ȸ� �����Ѵ�.
	if (HasAuthority())
	{
		SetLifeSpan(MaxLifeSpan);
	}
}

void AEnemyProjectile::Launch(AActor* InInstigator, float InDamage)
{
	// ���� ����
	if (!HasAuthority())
	{
		return;
	}

	// ������ ���
	if (InDamage >= 0.f)
	{
		// ���� ����
		Damage = InDamage * DamageMultiplier;
	}

	// �̻��� �����Ʈ�� �浹 �� �ٷ� ���߱⿡ �ڱ��ڽ��� OnHit�� �߻������ʵ��� ���´�.
	// �׸��� �̰� ���ε� ����صд�.
	if (InInstigator)
	{
		SetInstigator(Cast<APawn>(InInstigator));
		CollisionComp->IgnoreActorWhenMoving(InInstigator, true);

		// �߻����� ���� ����Ѵ�.
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

	// ������ ���� �����̰� Ŭ��� OnRep_Hit����
	if (!HasAuthority() || bHit)
	{
		return;
	}

	// �߻�ü ������ �ȸµ��� �����Ǿ������� Ȥ�ø𸣴� ���ܽ�Ŵ
	if (OtherActor == this || OtherActor == GetInstigator())
	{
		return;
	}

	UE_LOG(LogEnemyAI, Warning, TEXT("[Proj] OnHit  Actor=%s  Comp=%s"), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));

	bHit = true;


	// ���� ���̸� ���ظ� ���� �ʴ´�. ASC�� �����ϰ� ������ ���±� ����Ʈ�� �Ϻ� �����Ѵٸ� ����������x
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	bool bSameTeam = TargetASC && !OwnerTeamTag.IsEmpty() && TargetASC->HasAnyMatchingGameplayTags(OwnerTeamTag);

	// 맞는 쪽이 큐를 쐈는가. 막기·패링에 따라 소리가 갈리므로 큐는 맞는 쪽이 쏜다.
	// 벽처럼 피해를 안 받는 대상은 아무도 안 쏘므로 아래에서 여기가 대신 쏜다.
	bool bTargetPlayedCue = false;

	// �������� �������� ������������.
	// �������̽��� ������ ������ �������� �������� �մ� ��󿡰Ը�
	if (!bSameTeam && OtherActor && OtherActor->Implements<UDamageableInterface>() && IDamageableInterface::Execute_CanBeDamaged(OtherActor, GetInstigator()))
	{
		// ������ ����
		// 투사체라 원거리다. 패링을 당해도 발사자에게 스태거가 걸리지 않는다.
		IDamageableInterface::Execute_ApplyMeleeHit(OtherActor, GetInstigator(), Damage, Hit, EHBDamageSource::Ranged);

		// 맞는 쪽이 자기 큐를 쐈다. 여기서 또 쏘면 소리가 두 번 난다.
		bTargetPlayedCue = true;
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Projectile] %s �� %s (%.0f)"), *GetNameSafe(GetInstigator()), *GetNameSafe(OtherActor), Damage);

	// Ÿ�� Cue ȣ��. ��� ���� �Ҹ��� �ٸ�
	{
		// �߻�ü�� ASC�� ���� �±׸� ���������ʴ´�. �׷��� �Ѱ��� �±׵��� �־��ش�.
		FGameplayTagContainer ImpactTags;
		if (ImpactTypeTag.IsValid())
		{
			ImpactTags.AddTag(ImpactTypeTag);
		}

		// �߻�ü�� ����, �ڱ��ڽ�, ��Ʈ����, ������(��������� �����), �±׵��� �Ѱ� Cueȣ��
		// 벽처럼 피해를 안 받는 대상만 여기서 쏜다.
		//
		// 쳐낼 조건: 투사체의 성질 태그는 발사자 ASC 에 없어서 맞는 쪽이 알 수 없다.
		// 그래서 화살이 막히면 관통음이 아니라 방어음만 난다. 둘을 겹치려면
		// ApplyMeleeHit 에 성질 태그를 같이 넘겨야 한다.
		if (!bTargetPlayedCue)
		{
			HitCuePlay::PlayImpact(GetInstigator(), OtherActor, Hit, Damage, ImpactTags);
		}
	}

	// �ٷ� �ı��Ǵ� ���͸� �ٷ� ����
	if (HitBehavior == false)
	{
		Destroy();
		return;
	}

	// ����.�����̴� ����� ��ġ�� ������Ų��.
	AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);
	SetLifeSpan(StickLifeSpan);

	// �������� ���� �ٲ㵵 RepNotify �� �ڵ� ȣ����� �ʴ´�. ���� �θ���.
	OnRep_Hit();
}

void AEnemyProjectile::OnRep_Hit()
{
	//// ���� ��Ų��.
	//if (MovementComp)
	//{
	//  // �������� �����.
	//	MovementComp->StopMovementImmediately();
	//	// �������Ͼ����� ��Ȱ��ȭ
	//	MovementComp->Deactivate();
	//}

	// �浹�� ����.
	if (CollisionComp)
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

