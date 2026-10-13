// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/Ability/Warrior/GA_PlayerLeapAttack.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "Core/Tag/Warrior/HB_WarriorTag.h"

UGA_PlayerLeapAttack::UGA_PlayerLeapAttack()
{
	// 공격속도로 몽타주를 빠르게 하면 체공 시간과 애니가 어긋난다.
	// 도약은 애니가 아니라 물리가 시간을 정하므로 배속을 걸면 안 된다.
	bUseAttackSpeed = false;

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Efficiency = Augments.AddDefaulted_GetRef();
	Efficiency.AugmentTag = WarriorTag::TAG_Augment_ChargeAttack_Efficiency;
	Efficiency.DisplayName = NSLOCTEXT("PlayerLeapAttack", "EfficiencyName", "돌진 강타: 효율");
	Efficiency.Description = NSLOCTEXT("PlayerLeapAttack", "EfficiencyDesc", "돌진 강타의 재사용 대기시간과 마나 소모가 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Efficiency.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Efficiency.Params.Add(SkillParamTag::TAG_SkillParam_ManaCost, 0.85f);
	Efficiency.TierMagnitudes = { 0.85f, 0.75f, 0.65f };

	FHBSkillAugment& Reach = Augments.AddDefaulted_GetRef();
	Reach.AugmentTag = WarriorTag::TAG_Augment_ChargeAttack_Reach;
	Reach.DisplayName = NSLOCTEXT("PlayerLeapAttack", "ReachName", "돌진 강타: 거리");
	Reach.Description = NSLOCTEXT("PlayerLeapAttack", "ReachDesc", "돌진 강타의 도약 거리가 늘어납니다. (1단계 20%, 2단계 40%, 3단계 60%)");
	Reach.Params.Add(SkillParamTag::TAG_SkillParam_Reach, 1.2f);
	Reach.TierMagnitudes = { 1.2f, 1.4f, 1.6f };

	// 받는 피해 배율의 하한이 0.5 라(PreAttributeChange) 3단계 0.5 가 최대다.
	FHBSkillAugment& Guard = Augments.AddDefaulted_GetRef();
	Guard.AugmentTag = WarriorTag::TAG_Augment_ChargeAttack_Guard;
	Guard.DisplayName = NSLOCTEXT("PlayerLeapAttack", "GuardName", "돌진 강타: 가드");
	Guard.Description = NSLOCTEXT("PlayerLeapAttack", "GuardDesc", "돌진 강타를 쓰는 동안 받는 피해가 줄어듭니다. (1단계 20%, 2단계 35%, 3단계 50%)");
	Guard.TierMagnitudes = { 0.80f, 0.65f, 0.50f };
	Guard.DuringEffect = UGE_WarriorChargeGuardAugment::StaticClass();
}

void UGA_PlayerLeapAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 베이스가 비용 부족이나 몽타주 없음으로 이미 끝냈으면 아무것도 걸지 않는다.
	if (!IsActive())
	{
		return;
	}

	bLaunched = false;

	// 발사 이벤트 태그 데리게이트 대기
	UAbilityTask_WaitGameplayEvent* WaitLaunch = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag::TAG_Event_Leap_Launch, nullptr, false, false);
	if (WaitLaunch)
	{
		WaitLaunch->EventReceived.AddDynamic(this, &UGA_PlayerLeapAttack::OnLaunchEventReceived);
		WaitLaunch->ReadyForActivation();
	}
	// 땅에 착륙 이벤트 데리게이트 등록
	if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Char->LandedDelegate.AddDynamic(this, &UGA_PlayerLeapAttack::OnCharacterLanded);
	}
}

void UGA_PlayerLeapAttack::OnLaunchEventReceived(FGameplayEventData Payload)
{
	// 해당 판정은 서버만 담당한다.
	if (!ShouldSimulateLeap())
	{
		return;
	}

	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Char)
	{
		return;
	}
	UCharacterMovementComponent* Movement = Char->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// 증강 Reach 는 거리를 늘린다. 속도는 궤적 계산이 알아서 같이 올린다.
	// 아무 증강도 Reach 를 안 정했으면 1.0 이 와서 그대로 곱해진다.
	const float FinalDistance =
		LeapDistance * GetAugmentParam(SkillParamTag::TAG_SkillParam_Reach, 1.f);
	if (FinalDistance <= 0.f)
	{
		return;
	}

	// 시점의 수평 성분만 쓴다. 하늘을 보고 눌러도 앞으로 뛰게 한다.
	FVector Forward = Char->GetActorForwardVector();
	if (Char->GetController())
	{
		Forward = Char->GetControlRotation().Vector();
	}
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		return;
	}

	// 빠르게 보이기 위해 중력을 먼저 바꾼다. 궤적 계산이 이 중력을 전제로 해야 착지점이 맞는다.
	SavedGravityScale = Movement->GravityScale;
	bGravityOverridden = true;
	Movement->GravityScale = LeapGravityScale;

	const FVector Start = Char->GetActorLocation();
	const FVector End = Start + Forward * FinalDistance;

	FVector LaunchVelocity = FVector::ZeroVector;
	const bool bFound = UGameplayStatics::SuggestProjectileVelocity_CustomArc(Char, LaunchVelocity, Start, End, Movement->GetGravityZ(), ArcParam);
	if (!bFound)
	{
		// 너무 가깝거나 물리적으로 불가능한 경우다. 중력을 돌려놓고 도약만 포기한다.
		UE_LOG(LogTemp, Warning, TEXT("[Leap] %s 도약 궤적 계산 실패"), *GetNameSafe(Char));
		RestoreGravity();
		return;
	}

	bLaunched = true;

	// 두 인자 true = 기존 XY, Z 속도를 무시하고 전부 덮어쓴다.
	Char->LaunchCharacter(LaunchVelocity, true, true);

	// 구멍에 빠지면 LandedDelegate 가 영영 안 온다. 중력이 3배인 채로 남는 것을 막는다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AirTimeTimer, this, &UGA_PlayerLeapAttack::OnAirTimeExpired, MaxAirTime, false);
	}
}

void UGA_PlayerLeapAttack::OnCharacterLanded(const FHitResult& Hit)
{
	if (!ShouldSimulateLeap())
	{
		return;
	}
	// 발사 전에 땅에 닿은 경우(공중에서 발동)는 무시한다.
	if (!bLaunched)
	{
		return;
	}
	bLaunched = false;

	// 안전장치 해제
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AirTimeTimer);
	}
	// 중력 되돌리기
	RestoreGravity();

	// 공중에서만 걸리는 버프를 착지 순간 걷는다. 증강 Guard(도약 중 피해 감소)가 이것이다.
	//
	// 왜 여기서 거두나: 자버프 GE 의 지속시간은 BuffDuration 을 따르는데, 도약은
	// 지형과 거리에 따라 체공 시간이 매번 다르다. 시간으로 맞추면 짧으면 무방비고
	// 길면 착지 후에도 남는다. 착지라는 사건으로 끊는 게 정확하다.
	//
	// 쳐낼 조건: 착지 후에도 유지돼야 하는 자버프가 생기면 그것만 따로 관리해야 한다.
	// 지금은 이 스킬의 자버프가 전부 공중용이다.
	RemoveSelfBuff();

	if (LandSectionName.IsNone())
	{
		return;
	}

	// AnimInstance 를 직접 부르지 않는다.
	//
	// 왜: Anim->Montage_JumpToSection 은 그 머신에만 남는다. 서버와 본인은
	// 착지 모션을 보지만 다른 플레이어 화면에서는 도약 루프가 계속 돌다가
	// 어빌리티가 끝날 때 뚝 끊긴다.
	//
	// 작동방식: 어빌리티의 이 함수는 ASC 를 거치므로 복제된다.
	// 방어(GA_PlayerBlock::OnBlockReleaseReceive)가 이미 같은 이유로 이걸 쓴다.
	//
	// 판정은 이 섹션에 붙은 ANS 가 한다.
	MontageJumpToSection(LandSectionName);
}

void UGA_PlayerLeapAttack::OnAirTimeExpired()
{
	if (!bLaunched)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Leap] %s 착지하지 못해 강제 종료한다."),*GetNameSafe(GetAvatarActorFromActorInfo()));

	bLaunched = false;
	RestoreGravity();
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_PlayerLeapAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AirTimeTimer);
	}

	// 취소나 사망으로 끝나도 중력이 남으면 캐릭터가 영구히 무거워진다.
	RestoreGravity();
	bLaunched = false;

	if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Char->LandedDelegate.RemoveDynamic(this, &UGA_PlayerLeapAttack::OnCharacterLanded);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_PlayerLeapAttack::RestoreGravity()
{
	if (!bGravityOverridden)
	{
		return;
	}
	bGravityOverridden = false;

	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Char)
	{
		return;
	}
	UCharacterMovementComponent* Movement = Char->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}
	Movement->GravityScale = SavedGravityScale;
}

bool UGA_PlayerLeapAttack::ShouldSimulateLeap() const
{
	const ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Char)
	{
		return false;
	}
	if (Char->HasAuthority())
	{
		return true;
	}
	// 시뮬레이션 프록시는 CharacterMovement 복제로 따라온다. 직접 튀면 이중 적용이다.
	return Char->IsLocallyControlled();
}