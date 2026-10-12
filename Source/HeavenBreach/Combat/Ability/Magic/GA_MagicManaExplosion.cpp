// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/GA_MagicManaExplosion.h"

#include "AbilitySystemComponent.h"
#include "AI/EnemyCharacterBase.h"
#include "Combat/Ability/Magic/HBMagic.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Components/CapsuleComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Player/HBPlayerCharacter.h"

UGA_MagicManaExplosion::UGA_MagicManaExplosion()
{
	Cooldown = 15.f;
	ManaCost = 35.f;
	bUseAttackSpeed = false;
	RequiredWeaponTags.AddTag(ItemTag::TAG_Weapon_Staff);

	CostGameplayEffectClass = UGE_SkillManaCost::StaticClass();
	CooldownGameplayEffectClass = UGE_MagicExplosionCooldown::StaticClass();

	bIsCasting = true;
	ReleaseTime = 1.6f;
	CastHandFxIndex = 4;
	CastHandScale = 0.14f;

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Power = Augments.AddDefaulted_GetRef();
	Power.AugmentTag = MagicTag::TAG_Augment_Magic_ManaExplosion_Power;
	Power.DisplayName = NSLOCTEXT("MagicManaExplosion", "PowerName", "마력 폭발: 위력");
	Power.Description = NSLOCTEXT("MagicManaExplosion", "PowerDesc", "마력 폭발의 피해가 늘어납니다. (1단계 +15%, 2단계 +30%, 3단계 +50%)");
	Power.Params.Add(SkillParamTag::TAG_SkillParam_Damage, 1.15f);
	Power.TierMagnitudes = { 1.15f, 1.30f, 1.50f };

	FHBSkillAugment& Wide = Augments.AddDefaulted_GetRef();
	Wide.AugmentTag = MagicTag::TAG_Augment_Magic_ManaExplosion_Wide;
	Wide.DisplayName = NSLOCTEXT("MagicManaExplosion", "WideName", "마력 폭발: 확장");
	Wide.Description = NSLOCTEXT("MagicManaExplosion", "WideDesc", "마력 폭발의 반경이 넓어집니다. (1단계 3.5m, 2단계 4m, 3단계 4.5m)");
	Wide.Params.Add(SkillParamTag::TAG_SkillParam_Radius, 350.f);
	Wide.TierMagnitudes = { 350.f, 400.f, 450.f };
}

void UGA_MagicManaExplosion::OnCastReleased(const FGameplayAbilityActorInfo* Info)
{
	AHBPlayerCharacter* Player = GetPlayerFrom(Info);
	UWorld* World = GetWorld();
	UAbilitySystemComponent* SourceASC = GetASC(Info);
	if (!Player || !World || !SourceASC || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	const float FinalRadius = GetAugmentParam(SkillParamTag::TAG_SkillParam_Radius, Radius);
	const float DamageMul = GetAugmentParam(SkillParamTag::TAG_SkillParam_Damage, 1.f);
	const FVector Center = Player->GetActorLocation();

	// 적 BP 캡슐은 대개 "Enemy" 오브젝트 채널이라 Pawn 만 찾으면 0명이 나온다. 움직이는 오브젝트 전부를 보고 AEnemyCharacterBase 로 거른다.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MagicManaExplosion), false, Player);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects), FCollisionShape::MakeSphere(FinalRadius), QueryParams);

	// 한 적의 여러 컴포넌트가 걸릴 수 있다. 액터당 한 번만 때린다.
	TSet<const AActor*> Seen;
	int32 HitCount = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(Overlap.GetActor());
		if (!Enemy || Seen.Contains(Enemy))
		{
			continue;
		}
		Seen.Add(Enemy);

		const UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead) || !Enemy->Implements<UDamageableInterface>())
		{
			continue;	// 죽은 적·ASC 없는 적은 건너뛴다
		}
		if (!IDamageableInterface::Execute_CanBeDamaged(Enemy, Player))
		{
			continue;
		}

		// 공격력 × 계수 × 위력 증강 × 스킬 피해 배율 → 패시브 보정. 즉시 피해라 현재 시전을 인정한다(잔류 마력 1회 처리는 그쪽에서).
		float FinalDamage = Player->GetAttackPower() * DamageCoef * DamageMul * HBMagic::DamageMultiplier * Player->GetSkillDamageMultiplier();
		FinalDamage = Player->ModifyOutgoingDamage(Enemy, FinalDamage);

		// 맞은 점은 대상 위치, 법선은 시전자 쪽.
		FHitResult Hit;
		Hit.Component = Overlap.Component;
		Hit.Location = Enemy->GetActorLocation();
		Hit.ImpactPoint = Enemy->GetActorLocation();
		Hit.ImpactNormal = (Center - Enemy->GetActorLocation()).GetSafeNormal();
		Hit.Normal = Hit.ImpactNormal;
		Hit.TraceStart = Center;
		Hit.TraceEnd = Enemy->GetActorLocation();

		IDamageableInterface::Execute_ApplyMeleeHit(Enemy, Player, FinalDamage, Hit, EHBDamageSource::Ranged);
		++HitCount;

		const bool bKilled = TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
		Player->NotifyDamageDealt(Enemy, FinalDamage, bKilled);
	}

	// 연출: 발밑 지면에 반경만큼. 지면을 못 찾으면 캡슐 바닥.
	FVector CueLocation = Center;
	if (const UCapsuleComponent* Capsule = Player->GetCapsuleComponent())
	{
		CueLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}
	FHitResult GroundHit;
	if (World->LineTraceSingleByChannel(GroundHit, Center, Center - FVector(0.f, 0.f, 1000.f), ECC_Visibility, QueryParams) && !GroundHit.bStartPenetrating)
	{
		CueLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, 2.f);
	}
	FGameplayCueParameters CueParams;
	CueParams.Location = CueLocation;
	CueParams.RawMagnitude = FinalRadius;
	CueParams.Instigator = Player;
	CueParams.EffectCauser = Player;
	SourceASC->ExecuteGameplayCue(MagicTag::TAG_GameplayCue_Magic_ManaExplosion, CueParams);

	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 마력 폭발  반경=%.0f  적중=%d  시전ID=%d"), FinalRadius, HitCount, CastId);
}
