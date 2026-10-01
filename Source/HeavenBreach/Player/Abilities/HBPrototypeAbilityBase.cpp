#include "Player/Abilities/HBPrototypeAbilityBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Engine/OverlapResult.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "Combat/Effect/HBEffectLibrary.h"

UHBPrototypeAbilityBase::UHBPrototypeAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

bool UHBPrototypeAbilityBase::BeginAbilityWithCooldown(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayTag CooldownTag,
	const float CooldownDuration, const float ManaCost)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || HBEffects::IsCooldownActive(ASC, CooldownTag)) return false;

	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
	const float FinalCost = Player ? Player->ModifyManaCost(CooldownTag, ManaCost) : ManaCost;

	if (FinalCost > 0.f)
	{
		const float Mana = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute());
		if (Mana < FinalCost) 
			return false;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) 
		return false;

	if (FinalCost > 0.f)
	{
		ASC->ApplyModToAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute(), EGameplayModOp::Additive, -FinalCost);
	}
	HBEffects::TryApplyCooldown(ASC, CooldownTag, CooldownDuration);
	return true;
}

void UHBPrototypeAbilityBase::FinishAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

AActor* UHBPrototypeAbilityBase::FindLineTarget(AActor* Avatar, const float Range, const float Radius) const
{
	const TArray<AActor*> Targets = FindLineTargets(Avatar, Range, Radius);
	return Targets.IsEmpty() ? nullptr : Targets[0];
}

TArray<AActor*> UHBPrototypeAbilityBase::FindLineTargets(AActor* Avatar, const float Range, const float Radius) const
{
	TArray<AActor*> Result;
	if (!Avatar || !Avatar->GetWorld()) return Result;
	FVector Start = Avatar->GetActorLocation() + FVector::UpVector * 50.f;
	FVector Direction = Avatar->GetActorForwardVector();
	if (const APawn* Pawn = Cast<APawn>(Avatar))
	{
		Start = Pawn->GetPawnViewLocation();
		Direction = Pawn->GetBaseAimRotation().Vector();
	}
	const FVector End = Start + Direction * Range;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	TArray<FHitResult> Hits;
	Avatar->GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);
	Hits.Sort([Start](const FHitResult& A, const FHitResult& B)
	{
		return FVector::DistSquared(Start, A.ImpactPoint) < FVector::DistSquared(Start, B.ImpactPoint);
	});
	for (const FHitResult& Hit : Hits)
	{
		if (AActor* Target = Hit.GetActor(); Target && !Result.Contains(Target)) Result.Add(Target);
	}
	return Result;
}

TArray<AActor*> UHBPrototypeAbilityBase::FindTargetsInRadius(
	AActor* Avatar, const FVector Center, const float Radius) const
{
	TArray<AActor*> Result;
	if (!Avatar || !Avatar->GetWorld()) return Result;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Avatar);
	TArray<FOverlapResult> Overlaps;
	Avatar->GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AActor* Target = Overlap.GetActor(); Target && !Result.Contains(Target)) Result.Add(Target);
	}
	return Result;
}

TArray<AActor*> UHBPrototypeAbilityBase::FindTargetsInCone(
	AActor* Avatar, const float Range, const float HalfAngleDegrees) const
{
	TArray<AActor*> Result;
	for (AActor* Target : FindTargetsInRadius(Avatar, Avatar->GetActorLocation(), Range))
	{
		const FVector Direction = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal();
		if (FVector::DotProduct(Avatar->GetActorForwardVector(), Direction)
			>= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees))) Result.Add(Target);
	}
	return Result;
}

bool UHBPrototypeAbilityBase::ApplySkillDamage(
	AHBPlayerCharacter* Player, AActor* Target, float Damage) const
{
	if (!Player || !Target || !Target->Implements<UDamageableInterface>()) return false;

	// 장비의 스킬 피해 옵션. 근접 트레이스(MeleeTraceComponent)와 같은 순서로
	// 패시브 보정(ModifyOutgoingDamage) 전에 곱한다.
	Damage *= Player->GetSkillDamageMultiplier();
	Damage = Player->ModifyOutgoingDamage(Target, Damage);
	FHitResult Hit;
	Hit.Location = Target->GetActorLocation();
	Hit.ImpactPoint = Hit.Location;
	// 이 헬퍼를 쓰는 건 궁수·마법사 어빌리티뿐이라 원거리로 고정한다.
	// 근접 직업이 이 헬퍼를 쓰게 되면 Source 를 파라미터로 받아야 한다.
	IDamageableInterface::Execute_ApplyMeleeHit(Target, Player, Damage, Hit, EHBDamageSource::Ranged);
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const bool bKilled = TargetASC && TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
	Player->NotifyDamageDealt(Target, Damage, bKilled);
	return bKilled;
}

void UHBPrototypeAbilityBase::AddMana(const FGameplayAbilityActorInfo* ActorInfo, const float Amount) const
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const float Mana = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute());

		const float MaxMana = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxManaAttribute());

		ASC->SetNumericAttributeBase(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute(), FMath::Clamp(Mana + Amount, 0.f, MaxMana));
	}
}
