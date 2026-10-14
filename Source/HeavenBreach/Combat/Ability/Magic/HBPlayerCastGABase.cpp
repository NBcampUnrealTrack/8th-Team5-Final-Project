// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/HBPlayerCastGABase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameplayPrediction.h"
#include "Combat/Ability/Magic/HBMagic.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Player/HBPlayerCharacter.h"
#include "TimerManager.h"

UHBPlayerCastGABase::UHBPlayerCastGABase()
{
	// 시전 시간은 집중 배율만 따른다. 공격 속도는 섞지 않는다.
	bUseAttackSpeed = false;
}

float UHBPlayerCastGABase::GetCastMultiplier() const
{
	if (!bIsCasting)
	{
		return 1.f;	// 순간 이동·서리 지대는 모션 지연일 뿐 캐스팅이 아니다(설계 2.1)
	}

	const UAbilitySystemComponent* ASC = GetASC(GetCurrentActorInfo());
	const AHBPlayerCharacter* Player = GetPlayer();
	const bool bHasFocus = ASC && ASC->HasMatchingGameplayTag(MagicTag::TAG_State_Focus);
	const int32 FocusTier = Player ? Player->GetAugmentTier(MagicTag::TAG_Augment_Passive_Focus_Boost) : 0;
	return HBMagic::CastTimeMultiplier(bHasFocus, FocusTier);
}

// ---------------- 시전 ----------------

void UHBPlayerCastGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 부모(UHBPlayerActiveGABase)를 부르지 않는다. 부모는 여기서 CommitAbility 를 하는데,
	// 캐스팅 스킬은 비용·쿨다운을 발동 순간(서버)에 낸다. 시전 ID 는 부모 대신 직접 받는다.
	BeginCastId(ActorInfo);

	bReleased = false;
	bMontageEnded = false;
	bFinishing = false;
	bCancelling = false;
	bCastingTagAdded = false;
	bHandCueAdded = false;
	PlayingMontage = nullptr;
	MontageTask.Reset();
	CastingSlowHandle.Invalidate();

	AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	UAbilitySystemComponent* ASC = GetASC(ActorInfo);
	if (!Player || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bAuthority = ActorInfo->IsNetAuthority();

	// CanActivateAbility 가 이미 보지만 방어로 한 번 더. 여기서는 확인만 하고 내지 않는다.
	if (!CheckCost(Handle, ActorInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] %s: 마나 부족 — 시작 안 함  Auth=%d"), *GetName(), bAuthority ? 1 : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float CastMult = GetCastMultiplier();

	// 몽타주는 서버·예측 클라 모두 튼다. 어빌리티가 끝나도(서버 종료 복제) 마무리 동작은 끝까지 재생되게 두고,
	// 취소일 때만 EndAbility 에서 멈춘다.
	if (UAnimMontage* MontageToPlay = GetMontageToPlay())
	{
		if (UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, TEXT("Cast"), MontageToPlay, HBMagic::CastMontagePlayRate(MontagePlayRate, CastMult), NAME_None, false))
		{
			Task->OnCompleted.AddDynamic(this, &UHBPlayerCastGABase::OnCastMontageEnded);
			Task->OnBlendOut.AddDynamic(this, &UHBPlayerCastGABase::OnCastMontageEnded);
			Task->OnInterrupted.AddDynamic(this, &UHBPlayerCastGABase::OnCastMontageInterrupted);
			Task->OnCancelled.AddDynamic(this, &UHBPlayerCastGABase::OnCastMontageInterrupted);
			MontageTask = Task;
			PlayingMontage = MontageToPlay;
			Task->ReadyForActivation();
		}
	}

	// 손 준비 큐는 서버가 붙이고 복제된다(지속 큐라 예측하면 클라에서 떼는 시점이 꼬인다).
	if (bAuthority)
	{
		AddHandCue(ActorInfo);
	}

	if (bIsCasting)
	{
		// 감속은 어트리뷰트라 서버만 붙이고 복제를 따른다(발동 중 GE 와 같은 이유).
		if (bAuthority)
		{
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_CastingSlow::StaticClass(), 1.f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				CastingSlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}

		// 우클릭 취소는 누른 클라에서 시작하므로 태그는 서버·예측 클라가 각자 붙인다(ActivationOwnedTags 와 같은 방식).
		ASC->AddLooseGameplayTag(StateTag::TAG_State_Casting);
		bCastingTagAdded = true;
	}

	const float ReleaseDelay = HBMagic::CastReleaseDelay(ReleaseTime, MontagePlayRate, CastMult);
	UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] %s: 시전  Auth=%d  Local=%d  CastId=%d  Mult=%.2f  Release=%.2fs  Casting=%d"),
		*GetName(), bAuthority ? 1 : 0, ActorInfo->IsLocallyControlled() ? 1 : 0, CastId, CastMult, ReleaseDelay, bIsCasting ? 1 : 0);

	if (bAuthority)
	{
		OnCastStarted(ActorInfo);
		if (!IsActive())
		{
			return;
		}

		// 경직·사망은 서버가 본다. 경직 몽타주가 비어 있어도 끊겨야 한다(관통 사격과 같다).
		for (const FGameplayTag& InterruptTag : { StateTag::TAG_State_Staggered.GetTag(), AbilityTag::TAG_State_Dead.GetTag() })
		{
			UAbilityTask_WaitGameplayTagAdded* TagTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, InterruptTag, nullptr, true);
			TagTask->Added.AddDynamic(this, &UHBPlayerCastGABase::OnInterruptTagAdded);
			TagTask->ReadyForActivation();
			if (!IsActive())
			{
				return;	// 태그가 이미 있어서 바로 끝났다
			}
		}
	}

	// 서버는 발동, 클라는 시전 단계만 넘긴다(State.Casting 을 떼서 발동 뒤 우클릭이 취소를 보내지 않게).
	UWorld* World = GetWorld();
	if (World && ReleaseDelay > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(ReleaseTimerHandle, FTimerDelegate::CreateUObject(this, &UHBPlayerCastGABase::OnReleaseTime), ReleaseDelay, false);
	}
	else
	{
		OnReleaseTime();
	}
}

// ---------------- 발동 ----------------

void UHBPlayerCastGABase::OnReleaseTime()
{
	if (!IsActive() || bReleased || bFinishing)
	{
		return;
	}

	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (Info && Info->IsNetAuthority())
	{
		ReleaseCast();
		return;
	}

	// 클라: 발동 판정은 서버가 한다. 여기서는 시전 단계만 끝낸다.
	bReleased = true;
	RemoveCastingState(Info);
}

void UHBPlayerCastGABase::ReleaseCast()
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();

	// 방어: 경직·사망 중이면 발동하지 않는다(태그 대기 태스크가 보통 먼저 취소한다).
	if (IsStaggeredOrDead())
	{
		CancelByInterrupt(TEXT("발동 순간 경직·사망"));
		return;
	}

	bReleased = true;

	if (!CanRelease(Info))
	{
		// 순간 이동 목적지 실패 등. 비용 없이 끝낸다. 취소가 아니라 스킬이 스스로 접은 것.
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] %s: 발동 조건 실패 — 비용 없이 종료"), *GetName());
		FinishSelf(false);
		return;
	}

	// 여기서 처음으로 비용·쿨다운을 낸다. 클라는 커밋하지 않고 GE 를 복제로 받는다.
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), Info, GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] %s: 커밋 실패(마나·쿨다운) — 종료"), *GetName());
		FinishSelf(true);
		return;
	}

	// 잔류 마력: 이제부터 이 시전의 즉발 피해(마력 폭발 등)가 크레딧을 받는다.
	if (AHBPlayerCharacter* Player = GetPlayer())
	{
		Player->MarkSkillCastReleased(CastId);
	}

	UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] %s: 발동  CastId=%d"), *GetName(), CastId);
	OnCastReleased(Info);
	if (!IsActive())
	{
		return;
	}

	// 발동했으니 감속·우클릭 취소·손 준비가 끝난다. 남은 몽타주는 마무리 동작이다.
	RemoveCastingState(Info);

	if (!MontageTask.IsValid() || bMontageEnded)
	{
		ScheduleFinish();
	}
}

// ---------------- 몽타주 ----------------

void UHBPlayerCastGABase::OnCastMontageEnded()
{
	bMontageEnded = true;

	// 끝내는 주인은 서버다. 발동 전에 몽타주가 먼저 끝났으면(ReleaseTime 이 몽타주보다 길다) 발동이 마무리를 건다.
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (bReleased && Info && Info->IsNetAuthority())
	{
		FinishSelf(false);
	}
}

void UHBPlayerCastGABase::OnCastMontageInterrupted()
{
	if (bCancelling)
	{
		return;
	}

	if (!bReleased)
	{
		// 발동 전에 경직 몽타주 등에 끊겼다. 서버·클라 어느 쪽이 먼저 알아도 취소를 복제한다.
		CancelByInterrupt(TEXT("시전 몽타주 끊김"));
		return;
	}

	// 발동 뒤 마무리 동작이 끊긴 것. 이미 낸 비용·효과는 그대로 두고 끝낸다.
	OnCastMontageEnded();
}

// ---------------- 취소 ----------------

void UHBPlayerCastGABase::OnInterruptTagAdded()
{
	if (bReleased)
	{
		return;	// 발동 뒤에는 끊지 않는다. 마무리 몽타주의 끊김은 몽타주 콜백이 받는다
	}
	CancelByInterrupt(TEXT("경직·사망 태그"));
}

void UHBPlayerCastGABase::CancelByInterrupt(const TCHAR* Reason)
{
	if (!IsActive() || bFinishing || bCancelling)
	{
		return;
	}

	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] %s: 시전 취소(%s)  Auth=%d  Cancelable=%d"), *GetName(), Reason, Info && Info->IsNetAuthority() ? 1 : 0, CanBeCanceled() ? 1 : 0);

	// 취소가 실제로 될 때만 세운다. 거부된 취소가 bFinishing 을 남기면 이후 발동·마무리가 막혀 스킬이 안 끝난다.
	if (!CanBeCanceled())
	{
		return;
	}
	bFinishing = true;

	CancelAbility(GetCurrentAbilitySpecHandle(), Info, GetCurrentActivationInfo(), true);
}

bool UHBPlayerCastGABase::IsStaggeredOrDead() const
{
	const UAbilitySystemComponent* ASC = GetASC(GetCurrentActorInfo());
	return ASC && (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Staggered) || ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead));
}

void UHBPlayerCastGABase::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	// 관통 사격과 같다. 실제로 취소될 때만 세워서, 거부된 취소가 이후 정상 종료를 막지 않게 한다.
	if (CanBeCanceled())
	{
		bCancelling = true;
	}
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

// ---------------- 마무리 ----------------

void UHBPlayerCastGABase::ScheduleFinish()
{
	UWorld* World = GetWorld();
	if (World && FinishRecoveryTime > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(FinishTimerHandle, FTimerDelegate::CreateUObject(this, &UHBPlayerCastGABase::FinishSelf, false), FinishRecoveryTime, false);
		return;
	}
	FinishSelf(false);
}

void UHBPlayerCastGABase::FinishSelf(const bool bCancelled)
{
	if (!IsActive() || bCancelling)
	{
		return;
	}
	bFinishing = true;

	// 서버만 부른다. 종료를 복제해서 클라도 끝난다.
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bCancelled);
}

void UHBPlayerCastGABase::AddHandCue(const FGameplayAbilityActorInfo* Info)
{
	UAbilitySystemComponent* ASC = GetASC(Info);
	if (!ASC || bHandCueAdded)
	{
		return;
	}

	// 손 큐(Task 8)가 읽는 규약: RawMagnitude = 크기, AbilityLevel = 이펙트 종류, Location = hand_l 기준 오프셋.
	FGameplayCueParameters Params;
	Params.RawMagnitude = CastHandScale;
	Params.AbilityLevel = CastHandFxIndex;
	Params.Location = CastHandOffset;
	Params.Instigator = Info->AvatarActor.Get();
	Params.EffectCauser = Info->AvatarActor.Get();

	// 빈 예측 키로 감싼다. 발동 예측 창 안에서 서버가 붙이면 그 키를 가진 소유 클라는 "자기가 예측했다" 고 보고
	// OnActive(멀티캐스트)·WhileActive(복제 추가)를 둘 다 건너뛴다 — 그런데 클라는 큐를 예측하지 않으므로 아무것도 안 보인다.
	// 키를 비우면 소유 클라를 포함한 모든 클라가 재생한다.
	FScopedPredictionWindow NoKey(ASC, FPredictionKey(), false);
	ASC->AddGameplayCue(MagicTag::TAG_GameplayCue_Magic_CastHand, Params);
	bHandCueAdded = true;
}

void UHBPlayerCastGABase::RemoveCastingState(const FGameplayAbilityActorInfo* Info)
{
	UAbilitySystemComponent* ASC = GetASC(Info);
	if (!ASC)
	{
		return;
	}

	// 안 떼면 캐릭터가 영구히 느려지고, 우클릭이 계속 이 스킬을 찾는다.
	if (CastingSlowHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(CastingSlowHandle);
		CastingSlowHandle.Invalidate();
	}
	if (bCastingTagAdded)
	{
		ASC->RemoveLooseGameplayTag(StateTag::TAG_State_Casting);
		bCastingTagAdded = false;
	}
	if (bHandCueAdded)
	{
		// 붙일 때와 같은 이유로 빈 예측 키(서버만 여기 온다 — bHandCueAdded 는 서버에서만 선다).
		FScopedPredictionWindow NoKey(ASC, FPredictionKey(), false);
		ASC->RemoveGameplayCue(MagicTag::TAG_GameplayCue_Magic_CastHand);
		bHandCueAdded = false;
	}
}

void UHBPlayerCastGABase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(ReleaseTimerHandle);
		Timers.ClearTimer(FinishTimerHandle);
	}

	RemoveCastingState(ActorInfo);

	// 취소면 시전 동작도 멈춘다. 정상 종료는 마무리 동작이 끝까지 재생되게 둔다(몽타주 태스크를 멈추지 않게 만들었다).
	if (bWasCancelled && PlayingMontage)
	{
		if (UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.2f, PlayingMontage);
		}
	}
	PlayingMontage = nullptr;
	MontageTask.Reset();

	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] %s: 종료  Auth=%d  Released=%d  Cancelled=%d"),
		*GetName(), ActorInfo && ActorInfo->IsNetAuthority() ? 1 : 0, bReleased ? 1 : 0, bWasCancelled ? 1 : 0);

	bFinishing = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
