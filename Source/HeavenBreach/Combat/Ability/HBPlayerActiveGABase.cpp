// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Ability/HBPlayerActiveGABase.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Item/ItemEffectRules.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "AbilitySystemGlobals.h"
#include "HBPlayerPassiveGABase.h"

UHBPlayerActiveGABase::UHBPlayerActiveGABase()
{
    // InstancingPolicy 는 부모가 InstancedPerActor 로 정한다.

    // 클라가 예측한다. 반응성을 위해서다.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 액티브 공통 규약. 기본공격(GA_PlayerAttackRE)·방어(GA_PlayerBlock)와 같은 태그를 쓴다.
	//
	// 작동방식: 발동하면 GAS 가 ActivationOwnedTags 를 ASC 에 붙이고,
	// CanActivateAbility 가 ASC 의 태그와 ActivationBlockedTags 를 비교해 발동을 막는다.
	// State.Attacking 을 "붙이면서 동시에 막는" 한 쌍이 상호 배제를 만든다.
	//   스킬 중 다른 스킬   전 스킬이 이 베이스를 상속하므로 서로 막힌다
	//   스킬 중 기본공격    기본공격도 이 태그를 Blocked 로 갖고 있다
	//   기본공격 중 스킬    아래 Blocked 가 막는다
	//
	// BP 에 맡기지 않는 이유: 하나만 빠뜨려도 그 스킬이 아무 때나 나가고,
	// 빠뜨린 사실이 로그 없이 조용히 넘어간다.
	//
	// 쳐낼 조건: 기본공격을 스킬로 캔슬하고 싶어지면 State.Attacking 을 Blocked 에서 빼고
	// CancelAbilitiesWithTag 로 옮긴다. 지금은 "휘두르는 중엔 스킬 안 나감" 이다.
	ActivationOwnedTags.AddTag(StateTag::TAG_State_Attacking);

	// 스킬이 도는 동안 붙는다. 근접 트레이스가 이걸 보고 스킬 타격인지 가른다.
	// 이 부모를 쓰는 스킬은 따로 설정하지 않아도 스킬 피해 옵션을 받는다.
	// 기본 공격(GA_PlayerAttack)은 이 부모가 아니라 태그가 안 붙는다.
	ActivationOwnedTags.AddTag(StateTag::TAG_State_UsingSkill);

	ActivationBlockedTags.AddTag(StateTag::TAG_State_Attacking);
	ActivationBlockedTags.AddTag(AbilityTag::TAG_State_Dead);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Blocking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_BoxInteracting);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_InventoryBusy);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Staggered);

	// State.Grabbed 는 넣지 않는다. 잡힘 중 사용 가능 여부가 스킬마다 다르므로
	// 필요한 스킬의 BP 에서 Activation Blocked Tags 에 개별로 추가한다.
	// 주의: BP 에서 그 컨테이너를 건드리면 이후 여기에 태그를 추가해도 그 BP 는 못 받는다.
}

// ---------------- ActorInfo 를 직접 받는 것 ----------------
// GetASC / GetPlayerFrom 은 부모(UHBPlayerSkillGABase)에 있다.

float UHBPlayerActiveGABase::GetFinalManaCostFrom(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// 이 스킬에만 걸리는 증강이 먼저다. 전역 보정을 먼저 태우면
	// 하한선(0)에 걸린 뒤라 스킬 증강이 씹힌다.
	const float SkillCost = ManaCost * GetAugmentParamFrom(ActorInfo, SkillParamTag::TAG_SkillParam_ManaCost, 1.f);

	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return SkillCost;
	}
	// 전역 보정(장비, 패시브, 증강 GE)을 태운 최종 마나 코스트.
	// 빈 태그를 넘긴다. 스킬은 일반공격도 방어도 아니라서 특별 취급이 없다.
	return Player->ModifyManaCost(FGameplayTag(), SkillCost);
}

float UHBPlayerActiveGABase::GetFinalCooldownFrom(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// 마나와 같은 이유로 스킬 한정이 먼저다.
	// 뒤집으면 ModifyCooldown 의 하한선 0.1 에 걸려 스킬 증강이 통째로 사라진다.
	const float SkillCooldown = Cooldown * GetAugmentParamFrom(ActorInfo, SkillParamTag::TAG_SkillParam_Cooldown, 1.f);

	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return SkillCooldown;
	}
	// 전역 보정을 태운 최종 쿨타임
	return Player->ModifyCooldown(FGameplayTag(), SkillCooldown);
}

// ---------------- 발동 판정 ----------------

bool UHBPlayerActiveGABase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 발동이 가능한지 확인
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 필요 태그 확인(필요 무기 제약이 비어있다면 제약따윈없음)
	if (RequiredWeaponTags.IsEmpty())
	{
		return true;   // 무기 제약 없음
	}

	// 당연하지만 플레이어에게만 동작한다.
	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return false;
	}

	// 무기의 달인처럼 발동할 GA가 제약을 면제하는 태그를 갖고 있는지 본다.
	bool bBypassed = false;
	if (WeaponBypassTag.IsValid())
	{
		UAbilitySystemComponent* ASC = GetASC(ActorInfo);
		if (ASC)
		{
			bBypassed = ASC->HasMatchingGameplayTag(WeaponBypassTag);
		}
	}
	if (bBypassed)
	{
		return true;	// 제약조건 완화 덕에 통과
	}

	// 이제 실제 장비의 태그를 확인.
	const FGameplayTagContainer Equipped(Player->GetEquippedWeaponTag());
	if (!Equipped.HasAny(RequiredWeaponTags))
	{
		return false;	// 요구조건을 만족하는 장비가 아님.
	}

	// 요구 무기를 들고 있다.
	return true;
}

// ---------------- 비용 ----------------
// 비용만 Check 와 Apply 를 둘 다 오버라이드한다.
// 쿨타임은 CheckCooldown 이 태그 유무만 보므로 Apply 하나로 끝나지만,
// 비용은 CheckCost 가 GE 의 모디파이어를 실제로 평가하기 때문에 둘 다 필요하다.

bool UHBPlayerActiveGABase::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// GAS의 CheckCost는 CanApplyAttributeModifiers 로 GE의 모디파이어를 실제 평가하는데,
	// 그 안에서 스펙을 새로 만들기 때문에 우리가 ApplyCost 에서 넘길 SetByCaller 값을 모른다.
	// Super::로 사용되면 비용이 0 으로 평가돼 검사가 항상 통과한다.
	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return false;
	}

	// 이 함수는 두 번 불린다. CanActivateAbility 에서 한 번, CommitCheck 에서 한 번.
	// 발동을 시작한 뒤 자원이 사라졌을 수 있어서 GAS 가 커밋 직전에 다시 본다.
	// 그리고 클라에서도 돌기 때문에, 마나가 부족하면 몽타주조차 나가지 않는다.
	if (Player->HasEnoughMana(GetFinalManaCostFrom(ActorInfo)))
	{
		return true;
	}

	// 실패 이유를 호출한 쪽에 남긴다. UI가 마나 부족 을 띄울때 이걸 본다.
	const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
	if (OptionalRelevantTags && CostTag.IsValid())
	{
		OptionalRelevantTags->AddTag(CostTag);
	}
	return false;
}

void UHBPlayerActiveGABase::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 아이템 동작 효과의 "스킬 사용 시" (마력의 고리 등). 조건과 수치는 ItemEffectRules 와 DT.
	// 여기 맨 위에 두는 이유: 아래는 마나 코스트가 0 이면 일찍 반환한다. 그래도 아이템 효과는 돌아야 한다.
	ItemEffectRules::OnSkillUsed(ActorInfo);

	// 증강이 요구하는 추가 코스트. 원래 코스트와 별개로 낸다.
	// "마나 대신 체력" 은 이 GE 와 Params 의 ManaCost 0 을 같이 써서 만든다.
	// 여기 먼저 두는 이유: 아래에서 마나 코스트가 0 이라 일찍 반환해도
	// 추가 코스트는 내야 하기 때문이다.
	ApplyAdditionalCost(Handle, ActorInfo, ActivationInfo);

	// 부모함수를 부르지 않는다. 부모는 ApplyGameplayEffectToOwner 로 CDO 를 그대로 붙이기 때문에 SetByCaller 를 꽂을 틈이 없다.
	// 부르면 0 짜리 GE 가 한 번 더 붙으면서 "magnitude had not yet been set by caller" 경고만 남는다.
	if (!CostGameplayEffectClass)
	{
		return;   // 비용 없는 어빌리티
	}

	// 증강의 마나 감소가 여기서 반영된다. CheckCost 와 같은 값을 봐야 한다.
	const float FinalCost = GetFinalManaCostFrom(ActorInfo);
	if (FinalCost <= 0.f)
	{
		return;		// 코스트가 0이되면 할필요없지.
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo,CostGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid())
	{
		return;
	}

	// 증강의 마나 감소가 여기서 반영된다.
	Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_ManaCost, -FinalCost);

	// 활성화 예측 키로 붙기 때문에 클라 마나 바가 즉시 줄어든다.
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UHBPlayerActiveGABase::ApplyAdditionalCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const TSubclassOf<UGameplayEffect> ExtraCost = FindAdditionalCostEffect(ActorInfo);
	if (!ExtraCost)
	{
		return;   // 코스트를 건드리는 증강이 없다
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, ExtraCost, GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid())
	{
		return;
	}

	// 소모량은 GE 애셋에 직접 적는다. SetByCaller 를 안 쓰는 이유는
	// 자원마다 계산식이 달라서, 여기서 하나의 숫자로 넘길 수가 없기 때문이다.
	//
	// 쳐낼 조건: CheckCost 가 이 자원이 충분한지 보지 않는다. 체력이 모자라도
	// 발동되고 어트리뷰트 클램프에 걸린다. 막아야 하면 CheckCost 에 검사를 더한다.
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

// ---------------- 쿨타임 ----------------

void UHBPlayerActiveGABase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 증강이나 장비로 0 이 되면 지속시간 0짜리 GE가 붙어 쿨타임이 즉시 끝난다.
	const float FinalCooldown = GetFinalCooldownFrom(ActorInfo);
	if (FinalCooldown <= 0.f)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!Spec.IsValid())
	{
		return;
	}

	// 증강의 쿨타임 감소가 여기서 반영된다.
	Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Cooldown, FinalCooldown);

	// 클라도 즉시 자기 쿨타임을 갖는다.
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

// ---------------- 발동 ----------------

void UHBPlayerActiveGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 여기서 ApplyCost 와 ApplyCooldown 이 예측 키로 붙는다.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplySelfBuff(ActorInfo);
	ApplyDuringAbilityEffect(ActorInfo);

	// 몽타주 없는 스킬도 있다. 즉발 버프 같은 것.
	// 비용과 쿨타임과 버프는 CommitAbility로 이미 적용되었다.
	// 여기서 안 끝내면 몽타주가 null 이라 델리게이트가 하나도 안 오고
	// ActivationOwnedTags 가 남아 다음 발동이 막힌다.
	UAnimMontage* MontageToPlay = GetMontageToPlay();
	if (!MontageToPlay)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 공격속도 반영. 어트리뷰트는 1.0 이 기본이고 1.2 가 20% 증가다.
	float PlayRate = 1.f;
	if (bUseAttackSpeed)
	{
		UAbilitySystemComponent* ASC = GetASC(ActorInfo);
		if (ASC)
		{
			const float Rate = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute());
			PlayRate = FMath::Max(0.1f, Rate);
		}
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MontageToPlay, PlayRate);
	if (!Task)
	{
		// AnimInstance 가 없으면 null 이 온다. 그냥 붙이면 크래시한다.
		UE_LOG(LogTemp, Warning, TEXT("[GA] %s 몽타주 태스크 생성 실패"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 네 경로 모두 종료로 묶는다. 하나라도 빠지면 어빌리티가 안 끝난다.
	Task->OnCompleted.AddDynamic(this, &UHBPlayerActiveGABase::OnMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &UHBPlayerActiveGABase::OnMontageFinished);
	Task->OnCancelled.AddDynamic(this, &UHBPlayerActiveGABase::OnMontageCancelled);
	Task->OnInterrupted.AddDynamic(this, &UHBPlayerActiveGABase::OnMontageCancelled);
	Task->ReadyForActivation();
}

// ---------------- 자기 버프 ----------------

void UHBPlayerActiveGABase::ApplySelfBuff(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo)
	{
		return;
	}

	// 버프는 서버가 붙이고 복제된다.
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (!ASC)
	{
		return;
	}

	// 증강으로 늘어난 지속시간. 아무 증강도 Duration 을 안 정했으면 1.0 이 와서
	// 그대로 곱해진다. 그래서 여기에 분기가 없다.
	const float FinalDuration = BuffDuration * GetAugmentParam(SkillParamTag::TAG_SkillParam_Duration, 1.f);

	if (SelfBuffEffect)
	{
		FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(SelfBuffEffect, 1.f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Duration, FinalDuration);
			BuffHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
		}
	}

	// 증강이 있으면 GE 를 하나 더 붙인다. 지속시간은 같이 간다.
	for (const TPair<FGameplayTag, TSubclassOf<UGameplayEffect>>& Pair : AugmentBuffEffects)
	{
		if (!Pair.Value)
		{
			continue;
		}
		if (!HasAugment(Pair.Key))
		{
			continue;
		}

		FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(Pair.Value, 1.f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Duration, FinalDuration);
			BuffHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
		}
	}
}

void UHBPlayerActiveGABase::ApplyDuringAbilityEffect(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!DuringAbilityEffect)
	{
		return;   // 감속이 필요 없는 스킬. 비워두면 여기서 끝난다
	}
	if (!ActorInfo)
	{
		return;
	}

	// 서버가 붙이고 어트리뷰트 복제로 클라에 전달된다.
	// 이동속도는 어트리뷰트라 클라가 자체 적용하면 곧 도착할 서버 값과 싸운다.
	// 기본공격(GA_PlayerAttackRE)도 같은 방식이라 체감을 맞췄다.
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (!ASC)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DuringAbilityEffect, 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	DuringAbilityHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UHBPlayerActiveGABase::RemoveDuringAbilityEffect(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!DuringAbilityHandle.IsValid())
	{
		return;
	}

	// 취소나 사망으로 끝나도 여기를 거친다. 안 떼면 캐릭터가 영구히 느려진다.
	if (UAbilitySystemComponent* ASC = GetASC(ActorInfo))
	{
		ASC->RemoveActiveGameplayEffect(DuringAbilityHandle);
	}
	DuringAbilityHandle.Invalidate();
}

void UHBPlayerActiveGABase::RemoveSelfBuff()
{
	UAbilitySystemComponent* ASC = GetASC(GetCurrentActorInfo());
	if (!ASC)
	{
		return;
	}

	for (const FActiveGameplayEffectHandle& BuffHandle : BuffHandles)
	{
		if (!BuffHandle.IsValid())
		{
			continue;
		}
		ASC->RemoveActiveGameplayEffect(BuffHandle);
	}
	BuffHandles.Reset();
}

// ---------------- 종료 ----------------

void UHBPlayerActiveGABase::OnMontageFinished()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UHBPlayerActiveGABase::OnMontageCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UHBPlayerActiveGABase::EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,bool bReplicateEndAbility, bool bWasCancelled)
{
	// 발동 중에만 유지하는 것은 여기서 뗀다. 취소·사망으로 끝나도 반드시 지나간다.
	RemoveDuringAbilityEffect(ActorInfo);

	// 버프의 지속시간은 어빌리티 수명보다 길다. 걷지 않고 핸들만 비운다.
	BuffHandles.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}