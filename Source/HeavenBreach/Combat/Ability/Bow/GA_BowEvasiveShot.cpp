// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Bow/GA_BowEvasiveShot.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Combat/Attack/HBBowShot.h"
#include "Combat/Attack/HBPlayerArrow.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Player/HBPlayerCharacter.h"
#include "TimerManager.h"

UGA_BowEvasiveShot::UGA_BowEvasiveShot()
{
	Cooldown = 6.f;
	ManaCost = 20.f;
	bUseAttackSpeed = false;
	RequiredWeaponTags.AddTag(ItemTag::TAG_Weapon_Bow);

	CostGameplayEffectClass = UGE_SkillManaCost::StaticClass();
	CooldownGameplayEffectClass = UGE_BowEvasiveCooldown::StaticClass();

	ArrowClass = AHBPlayerArrow::StaticClass();

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Rapid = Augments.AddDefaulted_GetRef();
	Rapid.AugmentTag = ArcherTag::TAG_Augment_Bow_EvasiveShot_Rapid;
	Rapid.DisplayName = NSLOCTEXT("BowEvasiveShot", "RapidName", "회피 공격: 재빠른 몸놀림");
	Rapid.Description = NSLOCTEXT("BowEvasiveShot", "RapidDesc", "회피 공격 재사용 대기시간이 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Rapid.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Rapid.TierMagnitudes = { 0.85f, 0.75f, 0.65f };

	FHBSkillAugment& Light = Augments.AddDefaulted_GetRef();
	Light.AugmentTag = ArcherTag::TAG_Augment_Bow_EvasiveShot_Light;
	Light.DisplayName = NSLOCTEXT("BowEvasiveShot", "LightName", "회피 공격: 가벼운 호흡");
	Light.Description = NSLOCTEXT("BowEvasiveShot", "LightDesc", "회피 공격 스태미나 소모가 줄어듭니다. (1단계 20%, 2단계 35%, 3단계 50%)");
	Light.Params.Add(SkillParamTag::TAG_SkillParam_ManaCost, 0.8f);
	Light.TierMagnitudes = { 0.80f, 0.65f, 0.50f };
}

// ---------------- 발동 판정 ----------------

bool UGA_BowEvasiveShot::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 공중 사용 불가(설계 1장). 점프·낙하 중에 뒤로 미는 힘을 주면 공중 대시가 된다.
	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	const UCharacterMovementComponent* Movement = Player ? Player->GetCharacterMovement() : nullptr;
	if (!Movement || Movement->IsFalling())
	{
		return false;
	}
	return true;
}

// ---------------- 발동 ----------------

UAnimMontage* UGA_BowEvasiveShot::GetMontageToPlay() const
{
	// 부모 ActivateAbility 안과 발동 직후에 불리므로 CurrentActorInfo 가 유효하다.
	const FName Variant = HBBowShot::GetBowVariant(GetPlayerFrom(GetCurrentActorInfo()));
	if (const TObjectPtr<UAnimMontage>* Found = Variant.IsNone() ? nullptr : VariantMontages.Find(Variant))
	{
		return *Found;
	}
	return Super::GetMontageToPlay();
}

void UGA_BowEvasiveShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bArrowFired = false;

	AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bAuthority = ActorInfo->IsNetAuthority();

	// 화살이 없으면 커밋 전에 거절한다(스태미나·쿨다운 소모 없음). 관통 사격과 같은 방식.
	TSubclassOf<AHBPlayerArrow> UseArrowClass;
	FDataTableRowHandle UseArrowRow;
	ResolveArrow(Player, UseArrowClass, UseArrowRow);
	if (!HBBowShot::HasArrow(Player, UseArrowRow))
	{
		// 누른 사람 화면이면 바로, 서버가 원격 클라 대신 거절한 경우는 NotifyNoArrow 가 클라 RPC 로 보낸다.
		if (ActorInfo->IsLocallyControlled() || bAuthority)
		{
			HBBowShot::NotifyNoArrow(Player, NoArrowMessage);
		}
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 회피 공격: 화살 없음 — 시작 안 함  Auth=%d"), bAuthority ? 1 : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bHasMontage = GetMontageToPlay() != nullptr;
	if (bHasMontage)
	{
		// 커밋(비용·쿨다운) + 몽타주. 몽타주가 끝나거나 끊기면 부모가 끝낸다.
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
		if (!IsActive())
		{
			return;	// 커밋 실패 등으로 이미 끝났다
		}
	}
	else
	{
		// 몽타주가 없으면 부모가 바로 끝내서 후퇴·발사를 못 한다. 직접 커밋하고 FireDelay+0.1 뒤 끝낸다.
		// 이 경로는 부모의 발동 큐·자버프·발동 중 GE 를 거치지 않는다. 회피는 셋 다 쓰지 않는다.
		// 부모를 안 거치므로 시전 ID(잔류 마력)도 직접 받는다. 서버만.
		BeginCastId(ActorInfo);
		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FinishTimerHandle, FTimerDelegate::CreateUObject(this, &UGA_BowEvasiveShot::FinishWithoutMontage), FireDelay + 0.1f, false);
		}
	}

	// 활 AnimBP 가 스킬 동안 평타용 시위 보정을 끈다. 서버·소유 클라에서 세팅, 다른 클라는 복제.
	if (AHBPlayerCharacter* PosePlayer = GetPlayerFrom(ActorInfo))
	{
		PosePlayer->SetInBowSkillPose(true);
	}

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 회피 공격: 발동  Auth=%d  Local=%d  Montage=%d  RootMotionByMontage=%d"),
		bAuthority ? 1 : 0, ActorInfo->IsLocallyControlled() ? 1 : 0, bHasMontage ? 1 : 0, bUseMontageRootMotion ? 1 : 0);

	// 후퇴: 서버와 예측 클라가 둘 다 같은 태스크를 만든다. CMC 가 루트 모션 소스를 예측·보정한다.
	if (!bUseMontageRootMotion && RetreatDistance > KINDA_SMALL_NUMBER)
	{
		if (RetreatStart > KINDA_SMALL_NUMBER)
		{
			UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, RetreatStart);
			DelayTask->OnFinish.AddDynamic(this, &UGA_BowEvasiveShot::StartRetreat);
			DelayTask->ReadyForActivation();
		}
		else
		{
			StartRetreat();
		}
	}

	if (!bAuthority)
	{
		return;	// 발사·끊김 판정은 서버
	}

	// 경직·사망은 서버가 본다. 발사 전에 붙으면 취소로 끝낸다(타이머도 EndAbility 에서 지운다).
	for (const FGameplayTag& InterruptTag : { StateTag::TAG_State_Staggered.GetTag(), AbilityTag::TAG_State_Dead.GetTag() })
	{
		UAbilityTask_WaitGameplayTagAdded* TagTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, InterruptTag, nullptr, true);
		TagTask->Added.AddDynamic(this, &UGA_BowEvasiveShot::OnInterruptTagAdded);
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
	if (World && FireDelay > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(FireTimerHandle, FTimerDelegate::CreateUObject(this, &UGA_BowEvasiveShot::FireArrow), FireDelay, false);
	}
	else
	{
		FireArrow();
	}
}

// ---------------- 후퇴 ----------------

void UGA_BowEvasiveShot::StartRetreat()
{
	if (!IsActive())
	{
		return;
	}

	AHBPlayerCharacter* Player = GetPlayer();
	if (!Player)
	{
		return;
	}

	// 수평 뒤쪽. 캐릭터가 바닥을 보고 있어도 땅으로 파고들지 않게 Z 를 버린다.
	FVector Direction = -Player->GetActorForwardVector();
	Direction.Z = 0.f;
	if (!Direction.Normalize())
	{
		return;
	}

	const float Duration = FMath::Max(RetreatDuration, 0.01f);
	const float Strength = RetreatDistance / Duration;

	// Override(가산 아님): 입력 이동을 덮어 정확히 그 거리만 간다.
	// 끝나면 SetVelocity(0): 멈춘다. 놔두면 마지막 루트 모션 속도(약 225cm/s)로 미끄러진다.
	// bEnableGravity=true: Z 는 루트 모션이 덮지 않아 경사·낭떠러지에서 중력을 받는다.
	UAbilityTask_ApplyRootMotionConstantForce* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("EvasiveRetreat"), Direction, Strength, Duration,
		false, nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.f, true);
	if (Task)
	{
		Task->ReadyForActivation();
	}

	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 회피 공격: 후퇴 시작  Auth=%d  Dist=%.1f  Dur=%.3f  Speed=%.0f"),
		Info && Info->IsNetAuthority() ? 1 : 0, RetreatDistance, Duration, Strength);
}

// ---------------- 발사 (서버) ----------------

void UGA_BowEvasiveShot::FireArrow()
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!IsActive() || bArrowFired || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	// 방어: 경직·사망 중이면 쏘지 않는다(태그 대기 태스크가 보통 먼저 끝낸다).
	if (IsStaggeredOrDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 회피 공격: 발사 직전 경직·사망 — 안 쏨"));
		return;
	}
	bArrowFired = true;

	AHBPlayerCharacter* Player = GetPlayer();
	TSubclassOf<AHBPlayerArrow> UseArrowClass;
	FDataTableRowHandle UseArrowRow;
	ResolveArrow(Player, UseArrowClass, UseArrowRow);

	// 발동 뒤 화살을 버렸으면 쏘지 않는다. 비용은 이미 냈다(발동 때 확인했으므로 드문 경우).
	if (!HBBowShot::ConsumeArrow(Player, UseArrowRow))
	{
		HBBowShot::NotifyNoArrow(Player, NoArrowMessage);
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 회피 공격: 발사 직전 화살 없음 — 안 쏨(비용은 이미 냄)"));
		return;
	}

	FHBArrowLaunchOptions Options;
	Options.MaxPierceCount = 0;
	Options.bSkillArrow = true;
	Options.SkillCastId = CastId;

	const float FinalCoef = DamageCoef * GetAugmentParam(SkillParamTag::TAG_SkillParam_Damage, 1.f);
	const AHBPlayerArrow* Arrow = HBBowShot::SpawnArrow(Player, UseArrowClass, ArrowSpeed, FinalCoef, UseArrowRow,AimTraceDistance, SpawnForwardOffset, Options);

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 회피 공격: 발사  Coef=%.2f  Speed=%.0f  Arrow=%s"),
		FinalCoef, ArrowSpeed, *GetNameSafe(Arrow));
}

void UGA_BowEvasiveShot::ResolveArrow(const AHBPlayerCharacter* Player, TSubclassOf<AHBPlayerArrow>& OutClass, FDataTableRowHandle& OutRow) const
{
	// 애셋에서 직접 지정한 값이 우선. 비었으면 활 평타 GA 의 값을 빌린다.
	OutClass = ArrowClass;
	OutRow = ArrowItemRow;
	HBBowShot::ResolveArrowDefaults(Player, OutClass, OutRow);
}

// ---------------- 끊김·종료 ----------------

void UGA_BowEvasiveShot::OnInterruptTagAdded()
{
	if (!IsActive())
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 회피 공격: 경직·사망으로 취소  발사=%d"), bArrowFired ? 1 : 0);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UGA_BowEvasiveShot::FinishWithoutMontage()
{
	if (!IsActive())
	{
		return;
	}
	// 끝내는 주인은 서버다. 클라는 로컬만 끝내고 서버 종료가 복제로 따라온다.
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	EndAbility(GetCurrentAbilitySpecHandle(), Info, GetCurrentActivationInfo(), Info && Info->IsNetAuthority(), false);
}

bool UGA_BowEvasiveShot::IsStaggeredOrDead() const
{
	const UAbilitySystemComponent* ASC = GetASC(GetCurrentActorInfo());
	return ASC && (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Staggered) || ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead));
}

void UGA_BowEvasiveShot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 끝난 뒤에는 아무것도 쏘지 않는다. 후퇴 태스크는 어빌리티와 같이 끝나며 루트 모션 소스를 뗀다.
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(FireTimerHandle);
		Timers.ClearTimer(FinishTimerHandle);
	}

	if (AHBPlayerCharacter* PosePlayer = GetPlayerFrom(ActorInfo))
	{
		PosePlayer->SetInBowSkillPose(false);
	}

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 회피 공격: 종료  Auth=%d  Cancelled=%d  발사=%d"),
		ActorInfo && ActorInfo->IsNetAuthority() ? 1 : 0, bWasCancelled ? 1 : 0, bArrowFired ? 1 : 0);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
