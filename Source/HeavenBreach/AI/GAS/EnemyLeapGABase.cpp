// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemyLeapGABase.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "AI/EnemyAIControllerBase.h"
#include "AI/AlertComponent.h"
#include "AI/Enum/EAlertState.h"
#include "Core/Tag/Event/HB_EventTag.h"

void UEnemyLeapGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 기존 어빌리티활성화코드 재사용
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 만일 실행이 실패되었을 시(몽타주 실행안됨) 취소한다.
	if (!IsActive())
	{
		return;
	}

	bLaunched = false;
	LeapIndex = 0;

	// 발사 이벤트 태그를 대기
	UAbilityTask_WaitGameplayEvent* WaitLaunch = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag::TAG_Event_Leap_Launch, nullptr, false, false);
	WaitLaunch->EventReceived.AddDynamic(this, &UEnemyLeapGABase::OnLaunchEventReceived);
	WaitLaunch->ReadyForActivation();

	// 착지 시점에 대한 이벤트 태그 
	if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Char->LandedDelegate.AddDynamic(this, &UEnemyLeapGABase::OnCharacterLanded);
	}
}

void UEnemyLeapGABase::OnLaunchEventReceived(FGameplayEventData Payload)
{
	// 혹시 모르니 서버에서만 가능하다 확실히 해둠
	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Char || !Char->HasAuthority())
	{
		return;
	}

	// 타겟은 컨트롤러의 감각 컴포넌트가 갖고 있다.
	// 타겟이 없다면 공격을 해선안됨.
	AEnemyAIControllerBase* AIC = Cast<AEnemyAIControllerBase>(Char->GetController());
	UAlertComponent* Alert = AIC ? AIC->GetAlertComponent() : nullptr;
	AActor* Target = Alert ? Alert->GetCurrentTarget() : nullptr;
	if (!Target)
	{
		return;
	}
	FVector Start = Char->GetActorLocation();
	// 종착지점은 대상의 위치로 가지않고 바라보는방향기준 대상과 거리로 한다.
	// 시작할 당시 상대가 옆에 있는경우가 발생하면 옆으로 점프뛰기방지
	FVector Fwd = Char->GetActorForwardVector();
	Fwd.Z = 0.f;
	Fwd.Normalize();
	float Dist2D = FVector::Dist2D(Start, Target->GetActorLocation());
	FVector End = Start + Fwd * Dist2D;
	End.Z = Target->GetActorLocation().Z;

	FVector LaunchVelocity = FVector::ZeroVector;

	// 시작점에서 도착점까지 가는 포물선 속도를 구한다.
	// OutLaunchVelocity : 최종적으로 구한 속도벡터
	// StartPos와 EndPos : 시작/종료위치
	// OverrideGravityZ : 기본중력값 대신 쓸중력값. 0이면 기본중력사용
	// ArcParam : 궤적의 둥근 정도. 0에가까울수록 수직
	bool bFound = UGameplayStatics::SuggestProjectileVelocity_CustomArc(Char, LaunchVelocity, Start, End, 0.f, ArcParam);

	// 계산 실패된건지도 확인
	// 너무가깝거나 건네준 인자가 잘못되었거나 물리적으로 불가능할경우 실패함
	if (!bFound)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Leap] %s 도약 궤적 계산 실패"), *GetNameSafe(Char));
		return;
	}

	// 현재 뛴상태임을 기억함.
	bLaunched = true;

	// 두 인자 true = 기존 x,y,z속도 무시하고 전부 덮어쓰기
	Char->LaunchCharacter(LaunchVelocity, true, true);
}

void UEnemyLeapGABase::OnCharacterLanded(const FHitResult& Hit)
{
	// 해당함수는 서버에서호출하기
	ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Char || !Char->HasAuthority())
	{
		return;
	}

	// 아직 출발도 안한 상태에서 땅에 닿은경우(떨어지는와중 공격시행) 무시.
	if (!bLaunched)
	{
		return;
	}

	// 다음부터 발사가 된다면 그때 다시 검사하도록함.
	bLaunched = false;

	// 몽타주를 재생하는놈은 UAnimInstance이다. UAnimInstance를 꺼내기
	USkeletalMeshComponent* Mesh = Char->GetMesh();
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;

	// 다음 섹션으로 넘어가기. 등록된 섹션이름이 존재하는지 확인.
	if (Anim && Montage && LandSectionNames.IsValidIndex(LeapIndex))
	{
		// 해당이름의 섹션으로 넘어감.
		Anim->Montage_JumpToSection(LandSectionNames[LeapIndex], Montage);
	}

	++LeapIndex;
}

void UEnemyLeapGABase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 이제 착륙시 이벤트 호출 구독필요없으니 구독해제
	if (ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Char->LandedDelegate.RemoveDynamic(this, &UEnemyLeapGABase::OnCharacterLanded);
	}

	bLaunched = false;

	// 부모의 종료과정 처리
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}