// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Bow/GA_BowHunterMark.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AI/EnemyCharacterBase.h"
#include "Combat/Attack/HBHunterMark.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Player/HBPlayerCharacter.h"
#include "TimerManager.h"

UGA_BowHunterMark::UGA_BowHunterMark()
{
	Cooldown = 12.f;
	ManaCost = 20.f;
	bUseAttackSpeed = false;
	RequiredWeaponTags.AddTag(ItemTag::TAG_Weapon_Bow);

	CostGameplayEffectClass = UGE_SkillManaCost::StaticClass();
	CooldownGameplayEffectClass = UGE_BowHunterMarkCooldown::StaticClass();

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Deep = Augments.AddDefaulted_GetRef();
	Deep.AugmentTag = ArcherTag::TAG_Augment_Bow_HunterMark_Deep;
	Deep.DisplayName = NSLOCTEXT("BowHunterMark", "DeepName", "사냥꾼의 표식: 깊은 상처");
	Deep.Description = NSLOCTEXT("BowHunterMark", "DeepDesc", "표식이 붙은 적이 받는 피해가 더 늘어납니다. (1단계 25%, 2단계 30%, 3단계 35%)");
	Deep.Params.Add(SkillParamTag::TAG_SkillParam_MarkBonus, 0.25f);
	Deep.TierMagnitudes = { 0.25f, 0.30f, 0.35f };

	FHBSkillAugment& Wide = Augments.AddDefaulted_GetRef();
	Wide.AugmentTag = ArcherTag::TAG_Augment_Bow_HunterMark_Wide;
	Wide.DisplayName = NSLOCTEXT("BowHunterMark", "WideName", "사냥꾼의 표식: 넓은 시야");
	Wide.Description = NSLOCTEXT("BowHunterMark", "WideDesc", "표식 범위가 넓어집니다. (1단계 5m, 2단계 6m, 3단계 7m)");
	Wide.Params.Add(SkillParamTag::TAG_SkillParam_Radius, 500.f);
	Wide.TierMagnitudes = { 500.f, 600.f, 700.f };
}

// ---------------- 발동 ----------------

void UGA_BowHunterMark::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bMarkApplied = false;

	const bool bAuthority = ActorInfo && ActorInfo->IsNetAuthority();

	// 몽타주가 없으면 부모가 커밋 직후 바로 끝낸다. 타이머를 걸 틈이 없으니 직접 커밋하고 즉시 붙인다.
	// 이 경로는 부모의 발동 큐(ActivationCueTag)·자버프를 거치지 않는다. 표식은 둘 다 쓰지 않는다.
	if (!GetMontageToPlay())
	{
		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 사냥꾼의 표식: 몽타주 없음 — 즉시 적용  Auth=%d"), bAuthority ? 1 : 0);
		if (bAuthority)
		{
			ApplyMarks();
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

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 사냥꾼의 표식: 발동  Auth=%d  Local=%d  ApplyDelay=%.2f"),
		bAuthority ? 1 : 0, ActorInfo->IsLocallyControlled() ? 1 : 0, ApplyDelay);

	// 표식은 서버만 붙인다. 클라는 몽타주만.
	if (!bAuthority)
	{
		return;
	}

	// 경직·사망은 서버가 본다. 경직 몽타주가 비어 있으면 몽타주 끊김이 오지 않을 수 있어서다.
	for (const FGameplayTag& InterruptTag : { StateTag::TAG_State_Staggered.GetTag(), AbilityTag::TAG_State_Dead.GetTag() })
	{
		UAbilityTask_WaitGameplayTagAdded* TagTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, InterruptTag, nullptr, true);
		TagTask->Added.AddDynamic(this, &UGA_BowHunterMark::OnInterruptTagAdded);
		TagTask->ReadyForActivation();
		if (!IsActive())
		{
			break;	// 태그가 이미 있어서 바로 끝났다. 끝난 어빌리티에 태스크를 더 만들지 않는다
		}
	}
	if (!IsActive())
	{
		return;	// 태그가 이미 있어서 바로 끝났다
	}

	UWorld* World = GetWorld();
	if (World && ApplyDelay > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(ApplyTimerHandle, FTimerDelegate::CreateUObject(this, &UGA_BowHunterMark::ApplyMarks), ApplyDelay, false);
	}
	else
	{
		ApplyMarks();
	}
}

// ---------------- 표식 (서버) ----------------

void UGA_BowHunterMark::ApplyMarks()
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!IsActive() || bMarkApplied || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	// 방어: 경직·사망 중이면 붙이지 않는다(태그 대기 태스크가 보통 먼저 끝낸다).
	if (IsStaggeredOrDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 사냥꾼의 표식: 적용 직전 경직·사망 — 안 붙임"));
		return;
	}
	bMarkApplied = true;

	AHBPlayerCharacter* Player = GetPlayer();
	UAbilitySystemComponent* SourceASC = GetASC(Info);
	UWorld* World = GetWorld();
	if (!Player || !SourceASC || !World)
	{
		return;
	}

	const FVector Center = Player->ComputeAimPoint(AimTraceDistance);
	const float FinalRadius = GetAugmentParam(SkillParamTag::TAG_SkillParam_Radius, Radius);
	const float Bonus = GetAugmentParam(SkillParamTag::TAG_SkillParam_MarkBonus, 0.2f);
	const FGameplayTag DeepTag = HBHunterMark::TagForBonus(Bonus);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BowHunterMark), false, Player);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(FinalRadius), QueryParams);

	// 한 적의 여러 컴포넌트가 걸릴 수 있다. 액터당 한 번만 붙인다.
	TSet<const AActor*> Seen;
	int32 MarkedCount = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(Overlap.GetActor());
		if (!Enemy || Seen.Contains(Enemy))
		{
			continue;
		}
		Seen.Add(Enemy);

		UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
		{
			continue;	// 죽은 적·ASC 없는 적은 건너뛴다
		}

		// 플레이어 ASC 로 만든다. 컨텍스트의 Instigator 가 플레이어가 된다.
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UGE_HunterMark::StaticClass(), GetAbilityLevel(), SourceASC->MakeEffectContext());
		if (!Spec.IsValid())
		{
			continue;
		}
		Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Duration, MarkDuration);
		if (DeepTag.IsValid())
		{
			// 깊은 상처 단계 태그. HBHunterMark::DamageBonus 가 가장 큰 값을 고른다.
			Spec.Data->DynamicGrantedTags.AddTag(DeepTag);
		}
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		++MarkedCount;

		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 사냥꾼의 표식:   대상 %s  위치=%s"), *GetNameSafe(Enemy), *Enemy->GetActorLocation().ToCompactString());
	}

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 사냥꾼의 표식: 적용  대상=%d  중심=%s  반경=%.0f  보너스=%.2f  단계태그=%s  지속=%.1f"),
		MarkedCount, *Center.ToCompactString(), FinalRadius, Bonus, *DeepTag.ToString(), MarkDuration);
}

// ---------------- 끊김·종료 ----------------

void UGA_BowHunterMark::OnInterruptTagAdded()
{
	if (!IsActive())
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 사냥꾼의 표식: 경직·사망으로 취소  표식적용=%d"), bMarkApplied ? 1 : 0);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

bool UGA_BowHunterMark::IsStaggeredOrDead() const
{
	const UAbilitySystemComponent* ASC = GetASC(GetCurrentActorInfo());
	return ASC && (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Staggered) || ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead));
}

void UGA_BowHunterMark::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 끝난 뒤에는 아무것도 붙지 않는다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyTimerHandle);
	}

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 사냥꾼의 표식: 종료  Auth=%d  Cancelled=%d  표식적용=%d"),
		ActorInfo && ActorInfo->IsNetAuthority() ? 1 : 0, bWasCancelled ? 1 : 0, bMarkApplied ? 1 : 0);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
