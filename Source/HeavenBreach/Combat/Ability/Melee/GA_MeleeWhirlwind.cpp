// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Melee/GA_MeleeWhirlwind.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AI/EnemyCharacterBase.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Melee/HB_MeleeTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Engine/OverlapResult.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Ability/HBSkillTargets.h"
#include "Engine/World.h"
#include "Player/HBPlayerCharacter.h"
#include "TimerManager.h"

UGA_MeleeWhirlwind::UGA_MeleeWhirlwind()
{
	Cooldown = 8.f;
	ManaCost = 25.f;
	bUseAttackSpeed = false;
	RequiredWeaponTags.AddTag(ItemTag::TAG_Weapon_Sword);

	CostGameplayEffectClass = UGE_SkillManaCost::StaticClass();
	CooldownGameplayEffectClass = UGE_MeleeWhirlwindCooldown::StaticClass();

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Power = Augments.AddDefaulted_GetRef();
	Power.AugmentTag = MeleeTag::TAG_Augment_Melee_Whirlwind_Power;
	Power.DisplayName = NSLOCTEXT("MeleeWhirlwind", "PowerName", "회전 베기: 위력");
	Power.Description = NSLOCTEXT("MeleeWhirlwind", "PowerDesc", "회전 베기의 피해가 늘어납니다. (1단계 ×1.15, 2단계 ×1.30, 3단계 ×1.50)");
	Power.Params.Add(SkillParamTag::TAG_SkillParam_Damage, 1.15f);
	Power.TierMagnitudes = { 1.15f, 1.30f, 1.50f };

	FHBSkillAugment& Wide = Augments.AddDefaulted_GetRef();
	Wide.AugmentTag = MeleeTag::TAG_Augment_Melee_Whirlwind_Wide;
	Wide.DisplayName = NSLOCTEXT("MeleeWhirlwind", "WideName", "회전 베기: 확장");
	Wide.Description = NSLOCTEXT("MeleeWhirlwind", "WideDesc", "회전 베기의 반경이 넓어집니다. (1단계 3m, 2단계 3.5m, 3단계 4m)");
	Wide.Params.Add(SkillParamTag::TAG_SkillParam_Radius, 300.f);
	Wide.TierMagnitudes = { 300.f, 350.f, 400.f };
}

// ---------------- 발동 ----------------

void UGA_MeleeWhirlwind::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bHitApplied = false;

	const bool bAuthority = ActorInfo && ActorInfo->IsNetAuthority();

	// 몽타주가 없으면 부모가 커밋 직후 바로 끝낸다. 타이머를 걸 틈이 없으니 직접 커밋하고 즉시 판정한다.
	if (!GetMontageToPlay())
	{
		// 부모를 안 거치므로 시전 ID(잔류 마력)도 직접 받는다. 서버만.
		BeginCastId(ActorInfo);
		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		if (bAuthority)
		{
			if (AHBPlayerCharacter* CastPlayer = GetPlayerFrom(ActorInfo))
			{
				CastPlayer->MarkSkillCastReleased(CastId);
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[MeleeSkill] 회전 베기: 몽타주 없음 — 즉시 판정  Auth=%d"), bAuthority ? 1 : 0);
		if (bAuthority)
		{
			ApplyHits();
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 커밋(비용·쿨다운) + 몽타주. 몽타주가 끝나거나 끊기면 부모가 끝낸다.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;	// 커밋 실패 등으로 이미 끝났다
	}

	UE_LOG(LogTemp, Warning, TEXT("[MeleeSkill] 회전 베기: 발동  Auth=%d  Local=%d  HitDelay=%.2f"),
		bAuthority ? 1 : 0, ActorInfo->IsLocallyControlled() ? 1 : 0, HitDelay);

	// 피해는 서버만. 클라는 몽타주만.
	if (!bAuthority)
	{
		return;
	}

	// 경직·사망은 서버가 본다. 경직 몽타주가 비어 있으면 몽타주 끊김이 오지 않을 수 있어서다.
	for (const FGameplayTag& InterruptTag : { StateTag::TAG_State_Staggered.GetTag(), AbilityTag::TAG_State_Dead.GetTag() })
	{
		UAbilityTask_WaitGameplayTagAdded* TagTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, InterruptTag, nullptr, true);
		TagTask->Added.AddDynamic(this, &UGA_MeleeWhirlwind::OnInterruptTagAdded);
		TagTask->ReadyForActivation();
		if (!IsActive())
		{
			break;	// 태그가 이미 있어서 바로 끝났다. 끝난 어빌리티에 태스크를 더 만들지 않는다
		}
	}
	if (!IsActive())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World && HitDelay > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(HitTimerHandle, FTimerDelegate::CreateUObject(this, &UGA_MeleeWhirlwind::ApplyHits), HitDelay, false);
	}
	else
	{
		ApplyHits();
	}
}

// ---------------- 판정 (서버) ----------------

void UGA_MeleeWhirlwind::ApplyHits()
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!IsActive() || bHitApplied || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	// 방어: 경직·사망 중이면 판정하지 않는다(태그 대기 태스크가 보통 먼저 끝낸다).
	if (IsStaggeredOrDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MeleeSkill] 회전 베기: 판정 직전 경직·사망 — 판정 안 함"));
		return;
	}
	bHitApplied = true;

	AHBPlayerCharacter* Player = GetPlayer();
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		return;
	}

	const float FinalRadius = GetAugmentParam(SkillParamTag::TAG_SkillParam_Radius, Radius);
	const float DamageMul = GetAugmentParam(SkillParamTag::TAG_SkillParam_Damage, 1.f);
	const FVector Center = Player->GetActorLocation();

	// 범위 안 피해 받을 수 있는 대상. 몬스터와 팀원(팀킬 규칙은 화살·평타와 같다), 시전자·시체 제외(HBSkillTargets).
	TArray<AActor*> Targets;
	HBSkillTargets::FindInSphere(World, Center, FinalRadius, Player, Targets);

	int32 HitCount = 0;
	for (AActor* Enemy : Targets)
	{
		const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
		if (!IDamageableInterface::Execute_CanBeDamaged(Enemy, Player))
		{
			continue;
		}

		// 공격력 × 계수 × 위력 증강 × 스킬 피해 배율 → 패시브 보정. 즉시 스킬 피해라 현재 시전을 인정하는 기본값을 쓴다.
		float FinalDamage = Player->GetAttackPower() * DamageCoef * DamageMul * Player->GetSkillDamageMultiplier();
		FinalDamage = Player->ModifyOutgoingDamage(Enemy, FinalDamage);

		// 맞은 점은 대상 위치, 법선은 시전자 쪽.
		FHitResult Hit;
		Hit.Location = Enemy->GetActorLocation();
		Hit.ImpactPoint = Enemy->GetActorLocation();
		Hit.ImpactNormal = (Center - Enemy->GetActorLocation()).GetSafeNormal();
		Hit.Normal = Hit.ImpactNormal;
		Hit.TraceStart = Center;
		Hit.TraceEnd = Enemy->GetActorLocation();

		IDamageableInterface::Execute_ApplyMeleeHit(Enemy, Player, FinalDamage, Hit, EHBDamageSource::Melee);
		++HitCount;

		const bool bKilled = TargetASC && TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
		Player->NotifyDamageDealt(Enemy, FinalDamage, bKilled);
	}

	UE_LOG(LogTemp, Warning, TEXT("[MeleeSkill] 회전 베기: 판정  적중=%d  반경=%.0f  배율=%.2f"), HitCount, FinalRadius, DamageMul);
}

// ---------------- 끊김·종료 ----------------

void UGA_MeleeWhirlwind::OnInterruptTagAdded()
{
	if (!IsActive())
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[MeleeSkill] 회전 베기: 경직·사망으로 취소  판정=%d"), bHitApplied ? 1 : 0);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

bool UGA_MeleeWhirlwind::IsStaggeredOrDead() const
{
	const UAbilitySystemComponent* ASC = GetASC(GetCurrentActorInfo());
	return ASC && (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Staggered) || ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead));
}

void UGA_MeleeWhirlwind::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 끝난 뒤에는 아무것도 판정되지 않는다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitTimerHandle);
	}

	UE_LOG(LogTemp, Warning, TEXT("[MeleeSkill] 회전 베기: 종료  Auth=%d  Cancelled=%d  판정=%d"),
		ActorInfo && ActorInfo->IsNetAuthority() ? 1 : 0, bWasCancelled ? 1 : 0, bHitApplied ? 1 : 0);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
