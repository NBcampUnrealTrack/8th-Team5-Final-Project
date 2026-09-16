// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemyBeamGABase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

#include "AI/AlertComponent.h"
#include "AI/EnemyAIControllerBase.h"
#include "AI/Enum/EAlertState.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "Core/Tag/Team/HB_TeamTag.h"

void UEnemyBeamGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 부모의 몽타주 실행/종료 기능 사용. CommitAbility 도 여기에 있음
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 몽타주가 안 나갔으면 이어서 할 게 없다
	if (!IsActive())
	{
		return;
	}

	FiredCount = 0;

	// 몽타주의 AN_SendGameplayEvent 가 이 태그로 신호를 보낸다.
	// OnlyTriggerOnce = false 라 노티파이를 여러 개 깔면 전부 받는다.
	UAbilityTask_WaitGameplayEvent* WaitFire = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag::TAG_Event_Beam_Fire, nullptr, false, false);
	WaitFire->EventReceived.AddDynamic(this, &UEnemyBeamGABase::OnBeamFireEventReceived);
	WaitFire->ReadyForActivation();

	// 즉발이면 Stop 신호를 받을 일이 없지만 구독 비용이 없어서 항상 건다.
	UAbilityTask_WaitGameplayEvent* WaitStop = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag::TAG_Event_Beam_Stop, nullptr, false, false);
	WaitStop->EventReceived.AddDynamic(this, &UEnemyBeamGABase::OnBeamStopEventReceived);
	WaitStop->ReadyForActivation();

	UE_LOG(LogEnemyAI, Warning, TEXT("[Beam] 빔 어빌리티 활성화  Mode=%d  Aim=%d"), static_cast<int32>(BeamMode), static_cast<int32>(AimMode));
}

void UEnemyBeamGABase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소로 끝나도 빔은 멈춰야 한다. 안 끊으면 죽은 뒤에도 계속 쏜다.
	StopBeamTick();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEnemyBeamGABase::OnBeamFireEventReceived(FGameplayEventData Payload)
{
	if (BeamMode == EHBBeamMode::Instant)
	{
		FireBeamOnce();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 지속 모드. 이미 돌고 있으면 다시 걸지 않는다.
	// 노티파이를 실수로 두 개 깔아도 타이머가 겹치지 않는다.
	if (World->GetTimerManager().IsTimerActive(BeamTickTimer))
	{
		return;
	}

	// 첫 판정은 즉시, 이후 TickInterval 마다
	FireBeamOnce();
	World->GetTimerManager().SetTimer(BeamTickTimer, this, &UEnemyBeamGABase::FireBeamOnce, TickInterval, true);
}

void UEnemyBeamGABase::OnBeamStopEventReceived(FGameplayEventData Payload)
{
	StopBeamTick();
}

void UEnemyBeamGABase::StopBeamTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(BeamTickTimer);
}

void UEnemyBeamGABase::FireBeamOnce()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// 판정은 서버 전용
	if (!Avatar->HasAuthority())
	{
		return;
	}

	// VFX 제약으로 타수가 정해져 있다. 몽타주에 노티파이가 더 깔려 있어도 여기서 막는다.
	if (FiredCount >= MaxFireCount)
	{
		StopBeamTick();
		return;
	}
	++FiredCount;

	FVector Start = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	if (!ResolveBeamOrigin(Start, Direction))
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Beam] 발사 지점을 구하지 못했다"));
		return;
	}

	const FVector End = Start + Direction * BeamRange;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Avatar);

	const EDrawDebugTrace::Type DrawType = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	TArray<FHitResult> OutHits;
	UKismetSystemLibrary::SphereTraceMulti(
		Avatar,
		Start,
		End,
		BeamRadius,
		UEngineTypes::ConvertToTraceType(TraceChannel),
		false,                                              // bTraceComplex
		ActorsToIgnore,
		DrawType,
		OutHits,
		true,                                               // bIgnoreSelf
		FLinearColor::Yellow,
		FLinearColor::Red,
		DebugDrawTime);

	// 공격력은 어트리뷰트에서. 없으면 기본값이라도 넘긴다.
	float AttackPower = 10.f;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		bool bFound = false;
		const float Value = ASC->GetGameplayAttributeValue(UEnemyAttributeSetBase::GetAttackPowerAttribute(), bFound);
		if (bFound)
		{
			AttackPower = Value;
		}
	}

	// 아군 판별용 태그. 팀 태그가 없으면 필터가 안 돈다.
	FGameplayTagContainer OwnerTeamTag;
	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Avatar))
	{
		FGameplayTagContainer All;
		OwnerASC->GetOwnedGameplayTags(All);

		FGameplayTagContainer TeamTags;
		TeamTags.AddTag(TeamTag::TAG_Team_Player);
		TeamTags.AddTag(TeamTag::TAG_Team_Enemy);

		OwnerTeamTag = All.Filter(TeamTags);
	}

	// 한 발 안에서의 중복 방지.
	// SphereTraceMulti 는 같은 액터의 여러 본에 각각 히트를 돌려주므로 없으면 두 번 맞는다.
	TSet<TWeakObjectPtr<AActor>> HitActors;

	for (const FHitResult& Hit : OutHits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target)
		{
			continue;
		}

		// 같은 진영은 건너뛴다
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (TargetASC)
		{
			if (!OwnerTeamTag.IsEmpty())
			{
				if (TargetASC->HasAnyMatchingGameplayTags(OwnerTeamTag))
				{
					continue;
				}
			}
		}

		if (HitActors.Contains(Target))
		{
			continue;
		}
		HitActors.Add(Target);

		ApplyBeamHit(Avatar, Target, Hit, AttackPower);

		// 관통이 아니면 첫 대상에서 멈춘다
		if (!bPenetrate)
		{
			break;
		}
	}
}

bool UEnemyBeamGABase::ResolveBeamOrigin(FVector& OutStart, FVector& OutDirection) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return false;
	}

	ACharacter* Char = Cast<ACharacter>(Avatar);
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;

	const bool bHasSocket = Mesh ? Mesh->DoesSocketExist(MuzzleSocketName) : false;

	// 소켓이 없으면 액터 위치에서 나간다. 눈에 띄게 어긋나므로 바로 알아챌 수 있다.
	OutStart = bHasSocket ? Mesh->GetSocketLocation(MuzzleSocketName) : Avatar->GetActorLocation();

	if (AimMode == EHBBeamAim::SocketForward)
	{
		if (bHasSocket)
		{
			// 소켓의 X축이 빔 방향이다.
			// 안 맞으면 소켓 회전을 돌리는 게 먼저고, 그게 안 되면 오프셋으로 보정한다.
			const FQuat Final = Mesh->GetSocketQuaternion(MuzzleSocketName) * MuzzleRotationOffset.Quaternion();
			OutDirection = Final.GetForwardVector();
			return true;
		}

		// 소켓이 없으면 액터 전방으로 떨어진다
		OutDirection = Avatar->GetActorForwardVector();
		return true;
	}

	if (AimMode == EHBBeamAim::Target)
	{
		AActor* Target = nullptr;

		APawn* Pawn = Cast<APawn>(Avatar);
		if (Pawn)
		{
			AEnemyAIControllerBase* AIC = Cast<AEnemyAIControllerBase>(Pawn->GetController());
			if (AIC)
			{
				UAlertComponent* Alert = AIC->GetAlertComponent();
				if (Alert)
				{
					Target = Alert->GetCurrentTarget();
				}
			}
		}

		if (Target)
		{
			OutDirection = (Target->GetActorLocation() - OutStart).GetSafeNormal();
			return true;
		}

		// 타겟이 없으면 액터 전방으로 떨어진다
	}

	OutDirection = Avatar->GetActorForwardVector();
	return true;
}

void UEnemyBeamGABase::ApplyBeamHit(AActor* Owner, AActor* Target, const FHitResult& Hit, float AttackPower)
{
	// 대상이 인터페이스를 가진지 확인한다
	if (!Target->Implements<UDamageableInterface>())
	{
		return;
	}

	// 맞을 수 있는 상태인지는 대상이 판단
	if (!IDamageableInterface::Execute_CanBeDamaged(Target, Owner))
	{
		return;
	}

	const float FinalDamage = AttackPower * DamageMultiplier;

	// 수치만 넘긴다. GE 를 만드는 건 맞는 쪽의 몫.
	// 원거리로 넘기는 이유: 패링 경직은 근접에만 준다는 규칙이 맞는 쪽에 있다.
	IDamageableInterface::Execute_ApplyMeleeHit(Target, Owner, FinalDamage, Hit, EHBDamageSource::Ranged);

	// 타격 연출. 큐 태그는 어빌리티의 Activation Owned Tags 에서 읽는다.
	HitCuePlay::PlayImpact(Owner, Target, Hit, FinalDamage);

	UE_LOG(LogEnemyAI, Log, TEXT("[Beam] %s → %s (%.0f)"), *GetNameSafe(Owner), *GetNameSafe(Target), FinalDamage);
}
