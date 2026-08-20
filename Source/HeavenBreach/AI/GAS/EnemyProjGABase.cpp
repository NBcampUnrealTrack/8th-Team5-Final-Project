// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GAS/EnemyProjGABase.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "AI/EnemyProjectile.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "AI/Enum/EAlertState.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "AI/EnemyAIControllerBase.h"
#include "AI/AlertComponent.h"

void UEnemyProjGABase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 부모의 몽타주 실행/종료 기능 사용. CommitAbility도 여기에 있음
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 만일 실행이 실패되었을 시(몽타주 실행안됨) 취소한다.
	if (!IsActive())
	{
		return;
	}

	// 발사체가 없을시도 쓸수없음.
	if (!Projectile)
		return;


	// 몽타주의 AN_SendGameplayEvent 가 이 태그로 신호를 보낸다.
	// OnlyTriggerOnce = false 라 2연발이면 노티파이 두 개를 전부 받는다.
	UAbilityTask_WaitGameplayEvent* WaitFire = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag::TAG_Event_Ranged_Fire, nullptr, false, false);

	WaitFire->EventReceived.AddDynamic(this, &UEnemyProjGABase::OnFireEventReceived);
	WaitFire->ReadyForActivation();

	UE_LOG(LogEnemyAI, Warning, TEXT("[Proj] 발사체 발사 공격 어빌리티 활성화"));
}

void UEnemyProjGABase::OnFireEventReceived(FGameplayEventData Payload)
{
	// 적 캐릭터
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority() || !Projectile)
	{
		return;
	}



	APawn* Pawn = Cast<APawn>(Avatar);

	// 발사 지점은 소켓, 방향은 적의 전방.
	ACharacter* Char = Cast<ACharacter>(Avatar);
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;

	// 소켓의 위치기준. 소켓이 없다면 액터의 위치 기준
	FVector ActorLoc = Avatar->GetActorLocation();
	FVector SpawnLoc = (Mesh && Mesh->DoesSocketExist(MuzzleSocketName)) ? Mesh->GetSocketLocation(MuzzleSocketName) : ActorLoc;
	FRotator SpawnRot = Avatar->GetActorRotation();

	AEnemyAIControllerBase* AIC = Cast<AEnemyAIControllerBase>(Pawn->GetController());
	UAlertComponent* Alert = AIC->GetAlertComponent();
	AActor* Target = Alert->GetCurrentTarget();

	// 적이 조준중인 방향
	FVector Fwd = Avatar->GetActorForwardVector();
	Fwd.Z = 0.f;
	Fwd.Normalize();

	// 대상과의 거리. 어림짐작할 거리라 z만 뺀거로 본다.
	float Dist2D = Target ? FVector::Dist2D(ActorLoc, Target->GetActorLocation()) : 1000.f;
	Dist2D = FMath::Max(Dist2D, 300.f);

	// 대상의 중앙이 아닌 추가각도 보정을 추가해 노린다.
	FVector AimPoint = ActorLoc + Fwd * Dist2D;

	// 노리는 위치의 z값을 중앙이 아닌 추가각도 보정을 추가해 노린다.
	AimPoint.Z = (Target ? Target->GetActorLocation().Z : ActorLoc.Z) + AimHeightOffset;

	// 에임벡터 최종 연산
	FVector ToAim = AimPoint - SpawnLoc;

	// 스폰시 회전값
	SpawnRot = ToAim.Rotation();

	// 해당 사격각도는 한도를 둔다.
	SpawnRot.Pitch = FMath::Clamp(SpawnRot.Pitch, -45.f, 45.f);
	

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = Avatar;
	Params.Instigator = Cast<APawn>(Avatar);

	// 위치, 회전방향대로 스폰
	AEnemyProjectile* Proj = Avatar->GetWorld()->SpawnActor<AEnemyProjectile>(Projectile, SpawnLoc, SpawnRot, Params);

	if (!Proj)
	{
		return;
	}

	// 데미지는 적이 가지고잇을 어트리뷰트셋에서 추출
	// 없다면 기본 데미지라도 주게 -1이라도 넘긴다.
	float AttackPower = -1.f;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		bool bFound = false;
		float Value = ASC->GetGameplayAttributeValue(UEnemyAttributeSetBase::GetAttackPowerAttribute(), bFound);
		if (bFound)
		{
			AttackPower = Value;
		}
	}

	// 발사한다.
	Proj->Launch(Avatar, AttackPower);

	UE_LOG(LogEnemyAI, Log, TEXT("[Projectile] %s 발사 (%.0f)"), *GetNameSafe(Avatar), AttackPower);
}
