// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Bow/GA_BowPiercingShot.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/Attack/HBBowShot.h"
#include "Combat/Attack/HBPlayerArrow.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Archer/HB_ArcherTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Player/HBPlayerCharacter.h"
#include "TimerManager.h"

UGA_BowPiercingShot::UGA_BowPiercingShot()
{
	Cooldown = 8.f;
	ManaCost = 25.f;
	bUseAttackSpeed = false;
	RequiredWeaponTags.AddTag(ItemTag::TAG_Weapon_Bow);

	CostGameplayEffectClass = UGE_SkillManaCost::StaticClass();
	CooldownGameplayEffectClass = UGE_BowPiercingCooldown::StaticClass();

	// 붙어 있는 동안 캐릭터가 좌클릭을 이 스킬로 보낸다. 우클릭은 이 스킬 취소.
	// 부모의 State.Attacking·State.UsingSkill 도 그대로 붙는다(평타·다른 스킬 차단).
	ActivationOwnedTags.AddTag(StateTag::TAG_State_Bow_SkillAiming);

	ArrowClass = AHBPlayerArrow::StaticClass();

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Power = Augments.AddDefaulted_GetRef();
	Power.AugmentTag = ArcherTag::TAG_Augment_Bow_PiercingShot_Power;
	Power.DisplayName = NSLOCTEXT("BowPiercingShot", "PowerName", "관통 사격: 위력");
	Power.Description = NSLOCTEXT("BowPiercingShot", "PowerDesc", "관통 사격 피해가 늘어납니다. (1단계 15%, 2단계 30%, 3단계 50%)");
	Power.Params.Add(SkillParamTag::TAG_SkillParam_Damage, 1.15f);
	Power.TierMagnitudes = { 1.15f, 1.30f, 1.50f };

	FHBSkillAugment& Rapid = Augments.AddDefaulted_GetRef();
	Rapid.AugmentTag = ArcherTag::TAG_Augment_Bow_PiercingShot_Rapid;
	Rapid.DisplayName = NSLOCTEXT("BowPiercingShot", "RapidName", "관통 사격: 속사");
	Rapid.Description = NSLOCTEXT("BowPiercingShot", "RapidDesc", "관통 사격 재사용 대기시간이 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Rapid.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Rapid.TierMagnitudes = { 0.85f, 0.75f, 0.65f };
}

// ---------------- 발동: 준비 ----------------

void UGA_BowPiercingShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 부모(UHBPlayerActiveGABase)를 부르지 않는다. 부모는 여기서 CommitAbility 를 하는데,
	// 관통 사격은 비용·쿨다운을 발사 순간(서버)에 낸다.

	bPrepareDone = false;
	bInputPressed = false;
	bInputReleased = false;
	bDrawStarted = false;
	bFinishing = false;
	bFireCommitted = false;
	bArrowSpawned = false;
	bFinishCancelled = false;
	bCancelling = false;
	PrepareTask.Reset();
	DrawTask.Reset();
	DrawStartTime = 0.f;

	AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bAuthority = ActorInfo->IsNetAuthority();

	// 화살이 없으면 시작도 안 한다. 안내는 누른 사람 화면에서만(서버가 또 RPC 로 보내면 두 번 뜬다).
	TSubclassOf<AHBPlayerArrow> UseArrowClass;
	FDataTableRowHandle UseArrowRow;
	ResolveArrow(Player, UseArrowClass, UseArrowRow);
	if (!HBBowShot::HasArrow(Player, UseArrowRow))
	{
		// 누른 사람 화면이면 바로 띄운다. 서버가 원격 클라 대신 거절한 경우(클라는 화살이 있다고 예측)는
		// NotifyNoArrow 가 클라 RPC 로 보낸다. 리슨 서버 호스트는 IsLocallyControlled 쪽 한 번만 지난다.
		if (ActorInfo->IsLocallyControlled() || bAuthority)
		{
			HBBowShot::NotifyNoArrow(Player, NoArrowMessage);
		}
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 화살 없음 — 시작 안 함  Auth=%d"), bAuthority ? 1 : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// CanActivateAbility 가 이미 보지만 방어로 한 번 더. 여기서는 확인만 하고 내지 않는다.
	if (!CheckCost(Handle, ActorInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 마나 부족 — 시작 안 함  Auth=%d"), bAuthority ? 1 : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 좌클릭 누름 대기는 준비와 동시에 건다. 준비 중에 누른 것도 받아서 플래그로 기억한다.
	// 클라에서도 돌아야 한다. 클라 태스크가 눌림을 ServerSetReplicatedEvent 로 서버에 알린다.
	// bTestAlreadyPressed 는 false. 발동과 같은 프레임에 걸어서 놓치는 눌림이 없고,
	// true 면 지난 발동에서 좌클릭을 누른 채 우클릭 취소한 경우 남은 Spec.InputPressed 를 눌림으로 잘못 받는다.
	UAbilityTask_WaitInputPress* PressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	PressTask->OnPress.AddDynamic(this, &UGA_BowPiercingShot::OnInputPressed);
	PressTask->ReadyForActivation();

	float PrepareTime = 0.f;
	if (PrepareMontage)
	{
		if (UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Prepare"), PrepareMontage))
		{
			// 경직 몽타주 등이 끊으면 취소. 당김 몽타주로 넘어갈 때는 이 태스크를 먼저 끝내서 오인하지 않는다.
			Task->OnInterrupted.AddDynamic(this, &UGA_BowPiercingShot::OnPhaseMontageInterrupted);
			Task->OnCancelled.AddDynamic(this, &UGA_BowPiercingShot::OnPhaseMontageInterrupted);
			PrepareTask = Task;
			Task->ReadyForActivation();
			PrepareTime = PrepareMontage->GetPlayLength();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 준비  Auth=%d  Local=%d  PrepareTime=%.2f"),
		bAuthority ? 1 : 0, ActorInfo->IsLocallyControlled() ? 1 : 0, PrepareTime);

	// 준비 끝은 몽타주 길이로 정한다. 몽타주가 없으면 바로 끝난다.
	UWorld* World = GetWorld();
	if (World && PrepareTime > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(PrepareTimerHandle, FTimerDelegate::CreateUObject(this, &UGA_BowPiercingShot::OnPrepareFinished), PrepareTime, false);
	}
	else
	{
		OnPrepareFinished();
	}

	// 경직·사망은 서버가 본다. GA_PlayerStagger 는 몽타주로만 끊는데 몽타주가 비어 있을 수 있어서다.
	// 취소는 복제되는 EndAbility 로 클라도 정리된다. 태그가 이미 있으면 바로 불린다(발동 차단 태그라 보통 없음).
	if (bAuthority && IsActive())
	{
		for (const FGameplayTag& InterruptTag : { StateTag::TAG_State_Staggered.GetTag(), AbilityTag::TAG_State_Dead.GetTag() })
		{
			UAbilityTask_WaitGameplayTagAdded* TagTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, InterruptTag, nullptr, true);
			TagTask->Added.AddDynamic(this, &UGA_BowPiercingShot::OnInterruptTagAdded);
			TagTask->ReadyForActivation();
			if (!IsActive())
			{
				break;	// 태그가 이미 있어서 바로 끝났다. 끝난 어빌리티에 태스크를 더 만들지 않는다
			}
		}
	}
}

void UGA_BowPiercingShot::OnPrepareFinished()
{
	if (!IsActive() || bFinishing)
	{
		return;
	}

	bPrepareDone = true;

	// 준비 중에 눌러서 아직 누르고 있다면 지금부터 당긴다.
	if (bInputPressed && !bInputReleased)
	{
		BeginDraw();
	}
}

// ---------------- 당김 ----------------

void UGA_BowPiercingShot::OnInputPressed(float /*TimeWaited*/)
{
	if (!IsActive() || bFinishing)
	{
		return;
	}

	bInputPressed = true;

	// 떼기 대기는 눌림 직후 바로 건다. 준비 중에 눌렀다 뗀 것도 받아야 취소가 된다.
	// 떼기 태스크도 클라에서 돌아야 서버가 떼기를 받는다. bTestAlreadyReleased 는 false(방금 눌렸다).
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	ReleaseTask->OnRelease.AddDynamic(this, &UGA_BowPiercingShot::OnInputReleased);
	ReleaseTask->ReadyForActivation();

	if (bPrepareDone)
	{
		BeginDraw();
	}
}

void UGA_BowPiercingShot::BeginDraw()
{
	if (bDrawStarted)
	{
		return;
	}
	bDrawStarted = true;

	// 서버·클라가 각자 잰다. 발사 여부는 서버 값으로만 정한다.
	if (const UWorld* World = GetWorld())
	{
		DrawStartTime = World->GetTimeSeconds();
	}

	SetDrawingPose(true);

	// 당김 몽타주가 준비 몽타주를 끊는 것은 정상 전환이다. 끊김 콜백이 오기 전에 준비 태스크를 끝낸다.
	EndPhaseMontageTask(PrepareTask);

	if (DrawMontage)
	{
		if (UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Draw"), DrawMontage))
		{
			// 끝 콜백은 받지 않는다. 떼기가 끝을 정한다. 경직 등으로 끊기면 취소.
			Task->OnInterrupted.AddDynamic(this, &UGA_BowPiercingShot::OnPhaseMontageInterrupted);
			Task->OnCancelled.AddDynamic(this, &UGA_BowPiercingShot::OnPhaseMontageInterrupted);
			DrawTask = Task;
			Task->ReadyForActivation();
		}
	}

	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 당김 시작  Auth=%d"), Info && Info->IsNetAuthority() ? 1 : 0);
}

void UGA_BowPiercingShot::OnInputReleased(float /*TimeHeld*/)
{
	if (!IsActive() || bFinishing)
	{
		return;
	}

	bInputReleased = true;
	SetDrawingPose(false);

	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	const bool bAuthority = Info && Info->IsNetAuthority();

	// TimeHeld 는 눌림부터 잰 값이라 준비 시간이 섞인다. 당김 시작부터 다시 잰다.
	float Drawn = 0.f;
	if (bDrawStarted)
	{
		if (const UWorld* World = GetWorld())
		{
			Drawn = World->GetTimeSeconds() - DrawStartTime;
		}
	}
	const bool bFullyDrawn = bDrawStarted && Drawn + KINDA_SMALL_NUMBER >= DrawTime;

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 뗌  Auth=%d  Drawn=%.2f/%.2f  Full=%d"),
		bAuthority ? 1 : 0, Drawn, DrawTime, bFullyDrawn ? 1 : 0);

	// 방어: 경직·사망 중이면 다 당겼어도 쏘지 않는다(태그 대기 태스크가 보통 먼저 취소한다).
	if (bAuthority && IsStaggeredOrDead())
	{
		CancelByInterrupt(TEXT("떼는 순간 경직·사망"));
		return;
	}

	if (!bFullyDrawn)
	{
		// 다 당기기 전에 놓음 = 취소. 아무것도 내지 않는다.
		PlayFinishAndEnd(CancelMontage, true);
		return;
	}

	if (bAuthority && !TryCommitFire())
	{
		PlayFinishAndEnd(CancelMontage, true);
		return;
	}

	// 클라는 몽타주만. 서버가 취소로 판정하면 서버의 EndAbility 복제가 정리한다.
	PlayFinishAndEnd(ReleaseMontage, false);
}

// ---------------- 발사 (서버) ----------------

bool UGA_BowPiercingShot::TryCommitFire()
{
	AHBPlayerCharacter* Player = GetPlayer();
	TSubclassOf<AHBPlayerArrow> UseArrowClass;
	FDataTableRowHandle UseArrowRow;
	ResolveArrow(Player, UseArrowClass, UseArrowRow);

	// 비용을 내기 전에 화살부터 본다. 당기는 사이 화살을 버렸으면 아무것도 내지 않고 취소.
	if (!HBBowShot::HasArrow(Player, UseArrowRow))
	{
		HBBowShot::NotifyNoArrow(Player, NoArrowMessage);
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 발사 직전 화살 없음 — 취소(무소모)"));
		return false;
	}

	// 여기서 처음으로 비용·쿨다운을 낸다. 클라는 커밋하지 않고 마나·쿨다운 GE 를 복제로 받는다.
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 커밋 실패(마나·쿨다운) — 취소"));
		return false;
	}

	if (!HBBowShot::ConsumeArrow(Player, UseArrowRow))
	{
		// 바로 위에서 확인했으므로 사실상 안 온다. 오면 이미 낸 비용은 되돌리지 않는다.
		HBBowShot::NotifyNoArrow(Player, NoArrowMessage);
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 발사 직전 화살 없음 — 취소(비용은 이미 냄)"));
		return false;
	}

	bFireCommitted = true;

	UWorld* World = GetWorld();
	if (World && FireDelay > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(FireTimerHandle, FTimerDelegate::CreateUObject(this, &UGA_BowPiercingShot::SpawnPiercingArrow), FireDelay, false);
	}
	else
	{
		SpawnPiercingArrow();
	}
	return true;
}

void UGA_BowPiercingShot::SpawnPiercingArrow()
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!bFireCommitted || bArrowSpawned || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	// 죽었으면 쏘지 않는다(EndAbility 의 사망 검사와 같은 규칙). FireDelay 타이머가 사망 직후에 와도 막힌다.
	// 경직은 막지 않는다 — 놓은 뒤에는 이미 낸 비용을 지킨다.
	const UAbilitySystemComponent* ASC = GetASC(Info);
	if (!ASC || ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
	{
		bArrowSpawned = true;	// 다시 시도하지 않는다
		UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 발사 전 사망 — 화살 안 나감"));
		return;
	}
	bArrowSpawned = true;

	FHBArrowLaunchOptions Options;
	Options.MaxPierceCount = MaxPierceCount;
	Options.bSkillArrow = true;

	AHBPlayerCharacter* Player = GetPlayer();
	TSubclassOf<AHBPlayerArrow> UseArrowClass;
	FDataTableRowHandle UseArrowRow;
	ResolveArrow(Player, UseArrowClass, UseArrowRow);

	const float FinalCoef = DamageCoef * GetAugmentParam(SkillParamTag::TAG_SkillParam_Damage, 1.f);
	const AHBPlayerArrow* Arrow = HBBowShot::SpawnArrow(Player, UseArrowClass, ArrowSpeed, FinalCoef, UseArrowRow, AimTraceDistance, SpawnForwardOffset, Options);

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 발사  Coef=%.2f  Pierce=%d  Speed=%.0f  Arrow=%s"),
		FinalCoef, MaxPierceCount, ArrowSpeed, *GetNameSafe(Arrow));
}

void UGA_BowPiercingShot::ResolveArrow(const AHBPlayerCharacter* Player, TSubclassOf<AHBPlayerArrow>& OutClass, FDataTableRowHandle& OutRow) const
{
	// 애셋에서 직접 지정한 값이 우선. 비었으면 활 평타 GA 의 값을 빌린다.
	OutClass = ArrowClass;
	OutRow = ArrowItemRow;
	HBBowShot::ResolveArrowDefaults(Player, OutClass, OutRow);
}

// ---------------- 마무리 ----------------

void UGA_BowPiercingShot::PlayFinishAndEnd(UAnimMontage* FinishMontage, const bool bCancelled)
{
	bFinishing = true;
	bFinishCancelled = bCancelled;

	// 발사·취소 몽타주가 당김 몽타주를 덮는 것은 정상 전환이다. 끊김 콜백이 취소로 오지 않게 먼저 끝낸다.
	EndPhaseMontageTask(PrepareTask);
	EndPhaseMontageTask(DrawTask);

	if (FinishMontage)
	{
		// 어빌리티가 먼저 끝나도(서버 종료 복제 등) 발사·취소 동작은 끝까지 재생되게 둔다.
		if (UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Finish"), FinishMontage, 1.f, NAME_None, false))
		{
			Task->OnCompleted.AddDynamic(this, &UGA_BowPiercingShot::OnFinishMontageEnded);
			Task->OnBlendOut.AddDynamic(this, &UGA_BowPiercingShot::OnFinishMontageEnded);
			Task->OnCancelled.AddDynamic(this, &UGA_BowPiercingShot::OnFinishMontageCancelled);
			Task->OnInterrupted.AddDynamic(this, &UGA_BowPiercingShot::OnFinishMontageCancelled);
			Task->ReadyForActivation();
			return;
		}
	}

	if (UWorld* World = GetWorld())
	{
		// 발사 경로에서 FireDelay 전에 끝나지 않게 한다.
		const float Wait = FMath::Max3(FinishRecoveryTime, FireDelay + 0.05f, KINDA_SMALL_NUMBER);
		World->GetTimerManager().SetTimer(FinishTimerHandle, FTimerDelegate::CreateUObject(this, &UGA_BowPiercingShot::FinishSelf, bCancelled), Wait, false);
		return;
	}

	FinishSelf(bCancelled);
}

void UGA_BowPiercingShot::OnFinishMontageEnded()
{
	FinishSelf(bFinishCancelled);
}

void UGA_BowPiercingShot::OnFinishMontageCancelled()
{
	FinishSelf(true);
}

void UGA_BowPiercingShot::FinishSelf(const bool bCancelled)
{
	if (!IsActive() || bCancelling)
	{
		return;
	}

	// 끝내는 주인은 서버다. 클라가 자기 판정(예: 시간이 조금 모자라 취소)으로 먼저 끝내며 복제하면
	// 서버의 발사가 FireDelay 전에 잘린다. 그래서 클라는 로컬만 끝내고, 서버 종료가 복제로 따라온다.
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	const bool bAuthority = Info && Info->IsNetAuthority();
	EndAbility(GetCurrentAbilitySpecHandle(), Info, GetCurrentActivationInfo(), bAuthority, bCancelled);
}

void UGA_BowPiercingShot::OnInterruptTagAdded()
{
	CancelByInterrupt(TEXT("경직·사망 태그"));
}

void UGA_BowPiercingShot::OnPhaseMontageInterrupted()
{
	CancelByInterrupt(TEXT("준비·당김 몽타주 끊김"));
}

void UGA_BowPiercingShot::CancelByInterrupt(const TCHAR* Reason)
{
	// 놓은 뒤(발사·취소 마무리 중)에는 끼어들지 않는다. 발사 몽타주의 끊김은 마무리 콜백이 받는다.
	if (!IsActive() || bFinishing || bCancelling)
	{
		return;
	}
	bFinishing = true;

	SetDrawingPose(false);

	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 끊김 취소(%s)  Auth=%d"), Reason, Info && Info->IsNetAuthority() ? 1 : 0);

	// 취소 몽타주는 틀지 않는다. 경직 몽타주를 덮으면 안 된다.
	// 끊김은 클라에서도 종료를 복제한다(부모 OnMontageCancelled 와 같다). 클라 몽타주만 끊기고 서버가 모르면
	// 서버가 State.Attacking·SkillAiming 을 쥔 채 남는다. 놓기 전 단계라 서버에 낸 비용·쏠 화살이 없어 잘릴 것도 없다.
	// (일반 마무리 FinishSelf 는 그대로 클라 로컬 종료 — 서버 발사가 FireDelay 전에 잘리지 않게.)
	EndAbility(GetCurrentAbilitySpecHandle(), Info, GetCurrentActivationInfo(), true, true);
}

bool UGA_BowPiercingShot::IsStaggeredOrDead() const
{
	const UAbilitySystemComponent* ASC = GetASC(GetCurrentActorInfo());
	return ASC && (ASC->HasMatchingGameplayTag(StateTag::TAG_State_Staggered) || ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead));
}

void UGA_BowPiercingShot::EndPhaseMontageTask(TWeakObjectPtr<UAbilityTask_PlayMontageAndWait>& Task)
{
	// EndTask 는 델리게이트만 끊고 몽타주는 멈추지 않는다(어빌리티가 끝난 게 아니라서). 다음 몽타주가 자연스럽게 덮는다.
	if (UAbilityTask_PlayMontageAndWait* Ptr = Task.Get())
	{
		Ptr->EndTask();
	}
	Task.Reset();
}

void UGA_BowPiercingShot::SetDrawingPose(const bool bDrawing) const
{
	// 활 AnimBP 의 팔·시위 IK 와 장전 화살이 이 값을 본다(활 평타와 같다).
	if (AHBPlayerCharacter* Player = GetPlayer())
	{
		Player->SetBowDrawing(bDrawing);
	}
}

void UGA_BowPiercingShot::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	// CancelAbility 는 OnGameplayAbilityCancelled 를 먼저 방송하고 그 안에서 몽타주 태스크가 OnCancelled 를 낸다.
	// 그 콜백이 FinishSelf 로 EndAbility 를 한 번 더 부르지 않게 막는다. 끝내기는 Super 가 한다.
	// 취소가 거부될 때(CanBeCanceled false) 플래그만 남아 이후 정상 종료를 막지 않게, 실제로 취소될 때만 세운다.
	if (CanBeCanceled())
	{
		bCancelling = true;
	}
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UGA_BowPiercingShot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(PrepareTimerHandle);
		Timers.ClearTimer(FireTimerHandle);
		Timers.ClearTimer(FinishTimerHandle);
	}

	// 비용을 이미 냈는데 화살이 아직 안 나갔으면 지금 쏜다. 발사 뒤(놓은 뒤 FireDelay 사이)에 발사 몽타주가
	// 끊긴 경우다 — 경직은 발사 전 단계(준비·당김)만 취소하고, 놓은 뒤에는 이미 낸 비용을 지킨다.
	// 죽어서 끝난 경우만 쏘지 않는다(사망 검사는 SpawnPiercingArrow 안에 있다).
	if (bFireCommitted && !bArrowSpawned && ActorInfo && ActorInfo->IsNetAuthority())
	{
		SpawnPiercingArrow();
	}

	// 우클릭·경직 등으로 끊기면 당김 자세와 준비·당김 몽타주를 푼다.
	if (AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo))
	{
		Player->SetBowDrawing(false);
	}
	if (UAnimInstance* Anim = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr)
	{
		if (PrepareMontage)
		{
			Anim->Montage_Stop(0.2f, PrepareMontage);
		}
		if (DrawMontage)
		{
			Anim->Montage_Stop(0.2f, DrawMontage);
		}
	}

	bFireCommitted = false;
	bFinishing = false;

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 관통 사격: 종료  Auth=%d  Cancelled=%d"),
		ActorInfo && ActorInfo->IsNetAuthority() ? 1 : 0, bWasCancelled ? 1 : 0);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
