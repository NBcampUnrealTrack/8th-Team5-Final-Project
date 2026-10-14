// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/HBMageOrb.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Ability/Magic/HBMagic.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Components/SphereComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Cue/HB_CueTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayCueManager.h"
#include "NiagaraComponent.h"
#include "AI/EnemyCharacterBase.h"
#include "Combat/Ability/HBSkillTargets.h"
#include "Engine/OverlapResult.h"
#include "Player/HBPlayerCharacter.h"

AHBMageOrb::AHBMageOrb()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 4.f;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComp->InitSphereRadius(10.f);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	// 날아가는 구체가 상호작용·카메라 트레이스를 가리지 않게.
	CollisionComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComp->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionComp;

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	MovementComp->UpdatedComponent = CollisionComp;
	MovementComp->ProjectileGravityScale = 0.f;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->bShouldBounce = false;
	// 속도는 Launch 가 정한다. 그 전엔 멈춰 있다.
	MovementComp->bAutoActivate = false;

	TrailComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	TrailComp->SetupAttachment(CollisionComp);
	TrailComp->SetRelativeScale3D(FVector(0.3f));

	// 마법 타격. BP_GCN_Hit_Magic 애셋이 없으면 적에게 맞아도 적중음이 안 난다.
	ImpactTypeTag = CueTag::TAG_GameplayCue_Hit_Magic;
}

void AHBMageOrb::Launch(AHBPlayerCharacter* InShooter, const FVector& Dir, const float InSpeed, const float InDamageCoef, const int32 InCastId,
	const float InExplosionRadius, const float InEdgeFraction)
{
	Shooter = InShooter;
	DamageCoef = InDamageCoef;
	CastId = InCastId;
	ExplosionRadius = FMath::Max(0.f, InExplosionRadius);
	EdgeFraction = InEdgeFraction;

	const FVector Forward = Dir.GetSafeNormal();
	if (!Forward.IsZero())
	{
		SetActorRotation(Forward.Rotation());
	}

	if (InShooter)
	{
		// 쏜 사람 몸(캡슐·메시)에 바로 부딪히지 않게.
		CollisionComp->IgnoreActorWhenMoving(InShooter, true);
	}

	if (HasAuthority())
	{
		CollisionComp->OnComponentHit.AddDynamic(this, &AHBMageOrb::OnHit);
	}

	MovementComp->InitialSpeed = InSpeed;
	MovementComp->MaxSpeed = InSpeed;
	MovementComp->Velocity = Forward * InSpeed;
	MovementComp->Activate(true);
}

void AHBMageOrb::OnHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (bHasHit || !HasAuthority() || (OtherActor && OtherActor == Shooter.Get()))
	{
		return;
	}
	bHasHit = true;

	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 원소 구체 적중  대상=%s  컴포넌트=%s"), *GetNameSafe(OtherActor), *GetNameSafe(Hit.GetComponent()));

	// 적이든 벽이든 맞은 점에서 폭발한다.
	const FVector Center(Hit.ImpactPoint);
	Explode(Center, OtherActor);

	PlayImpactCue(Center);
	Destroy();
}

void AHBMageOrb::Explode(const FVector& Center, const AActor* DirectTarget)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 몬스터와 팀원(팀킬 규칙은 화살·평타와 같다). 시전자·시체 제외(HBSkillTargets).
	TArray<AActor*> Targets;
	HBSkillTargets::FindInSphere(World, Center, ExplosionRadius, Shooter.Get(), Targets);

	// 직접 맞은 대상은 겹침 검사 결과와 상관없이 넣는다(반경 0 이어도 맞는다). 시체·시전자는 빼고.
	AActor* Direct = const_cast<AActor*>(DirectTarget);
	if (Direct && Direct != Shooter.Get() && Direct->Implements<UDamageableInterface>())
	{
		const UAbilitySystemComponent* DirectASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Direct);
		if (DirectASC && !DirectASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
		{
			Targets.AddUnique(Direct);
		}
	}

	int32 HitCount = 0;
	for (AActor* Enemy : Targets)
	{

		// 직접 맞은 적은 100%. 나머지는 폭발 중심에서 적 위치까지 거리로 줄인다(캡슐 반지름만큼 빼서 몸 가장자리 기준).
		const float Distance = FMath::Max(0.f, FVector::Dist(Center, Enemy->GetActorLocation()) - Enemy->GetSimpleCollisionRadius());
		const float Scale = Enemy == DirectTarget ? 1.f : HBMagic::OrbExplosionFalloff(Distance, ExplosionRadius, EdgeFraction);
		if (Scale <= 0.f)
		{
			continue;
		}

		FHitResult Hit;
		Hit.ImpactPoint = Enemy->GetActorLocation();
		Hit.Location = Hit.ImpactPoint;
		Hit.ImpactNormal = (Center - Hit.ImpactPoint).GetSafeNormal();
		Hit.Normal = Hit.ImpactNormal;
		Hit.TraceStart = Center;
		Hit.TraceEnd = Hit.ImpactPoint;
		ApplyOrbDamage(Enemy, Hit, Scale);
		++HitCount;
	}

	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 원소 구체 폭발  반경=%.0f  적중=%d  직접=%s"), ExplosionRadius, HitCount, *GetNameSafe(DirectTarget));
}

void AHBMageOrb::ApplyOrbDamage(AActor* Target, const FHitResult& Hit, const float Scale)
{
	AHBPlayerCharacter* Player = Shooter.Get();
	if (!Player || !Target->Implements<UDamageableInterface>())
	{
		return;
	}
	if (!IDamageableInterface::Execute_CanBeDamaged(Target, Player))
	{
		UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 원소 구체 피해 없음 (대상이 거부)  대상=%s"), *GetNameSafe(Target));
		return;
	}

	// 공격력 × 계수 × 스킬 피해 배율 → 패시브 보정. 구체는 지금 시전이 아니라 자기 시전 ID 로만 잔류 마력을 알린다(날아가는 사이 스킬이 끝나도 적용).
	float FinalDamage = Player->GetAttackPower() * DamageCoef * Scale * HBMagic::DamageMultiplier * Player->GetSkillDamageMultiplier();
	FinalDamage = Player->ModifyOutgoingDamage(Target, FinalDamage, /*bCreditCurrentSkillCast*/ false);
	if (CastId > 0)
	{
		Player->NotifySkillHit(CastId);
	}

	{
		// 맞는 순간에만 시전자에게 구체 성질을 붙인다. 맞는 쪽 큐가 이걸 보고 소리를 고른다.
		// 구체는 GA 가 끝난 뒤에 맞아서, 이게 없으면 시전자에게 태그가 없어 기본음이 났다.
		HitCuePlay::FScopedImpactTag ImpactScope(Player, ImpactTypeTag);
		IDamageableInterface::Execute_ApplyMeleeHit(Target, Player, FinalDamage, Hit, EHBDamageSource::Ranged);
	}
	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 원소 구체 피해 %.1f → %s"), FinalDamage, *GetNameSafe(Target));

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const bool bKilled = TargetASC && TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
	Player->NotifyDamageDealt(Target, FinalDamage, bKilled);
}

void AHBMageOrb::PlayImpactCue(const FVector& Location) const
{
	AHBPlayerCharacter* Player = Shooter.Get();
	UAbilitySystemComponent* ASC = Player ? Player->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	FGameplayCueParameters Params;
	Params.Location = Location;
	// 큐가 폭발 범위에 맞춰 크기를 키울 수 있게 반경을 넘긴다(BP_GCN_Magic_OrbImpact 의 ScaleDivisor 가 0 이면 크기 그대로).
	Params.RawMagnitude = ExplosionRadius;
	Params.Instigator = Player;
	Params.EffectCauser = const_cast<AHBMageOrb*>(this);
	ASC->ExecuteGameplayCue(MagicTag::TAG_GameplayCue_Magic_OrbImpact, Params);
}
