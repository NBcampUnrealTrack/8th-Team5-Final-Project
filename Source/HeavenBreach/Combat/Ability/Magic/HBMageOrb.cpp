// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/HBMageOrb.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Components/SphereComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Cue/HB_CueTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayCueManager.h"
#include "NiagaraComponent.h"
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

void AHBMageOrb::Launch(AHBPlayerCharacter* InShooter, const FVector& Dir, const float InSpeed, const float InDamageCoef, const int32 InCastId)
{
	Shooter = InShooter;
	DamageCoef = InDamageCoef;
	CastId = InCastId;

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

	if (OtherActor)
	{
		ApplyOrbDamage(OtherActor, Hit);
	}

	// 적이든 벽이든 적중 큐. 위치는 맞은 점.
	PlayImpactCue(FVector(Hit.ImpactPoint));
	Destroy();
}

void AHBMageOrb::ApplyOrbDamage(AActor* Target, const FHitResult& Hit)
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
	float FinalDamage = Player->GetAttackPower() * DamageCoef * Player->GetSkillDamageMultiplier();
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
	Params.Instigator = Player;
	Params.EffectCauser = const_cast<AHBMageOrb*>(this);
	ASC->ExecuteGameplayCue(MagicTag::TAG_GameplayCue_Magic_OrbImpact, Params);
}
