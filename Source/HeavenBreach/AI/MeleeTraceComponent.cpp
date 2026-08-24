// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/MeleeTraceComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AI/Enum/EAlertState.h"
#include "Combat/Interface/DamageableInterface.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "AI/EnemyCharacterBase.h"

// Sets default values for this component's properties
UMeleeTraceComponent::UMeleeTraceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UMeleeTraceComponent::BeginTrace(const TArray<FName>& InSockets, TSubclassOf<UGameplayEffect> InDamageEffect, float InRadius, float InDamageMultiplier)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	ActiveSockets = InSockets;
	TraceRadius = InRadius;
	ActiveDamageEffect = InDamageEffect;
	DamageMultiplier = InDamageMultiplier;

	// 이전 공격의 기록이 남으면 안 된다.
	HitActors.Empty();
	bTracing = true;
}

void UMeleeTraceComponent::UpdateTrace()
{
	AActor* Owner = GetOwner();

	if (!Owner || !Owner->HasAuthority() || !bTracing) 
		return;
	if (!CachedMesh || ActiveSockets.Num() < 2) 
		return;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Owner);   // 자기 자신은 제외

	const EDrawDebugTrace::Type DrawType = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	// 인접 소켓끼리 이어서 트레이스검사
	for (int32 i = 1; i < ActiveSockets.Num(); ++i)
	{
		const FName& StartSocket = ActiveSockets[i - 1];
		const FName& EndSocket = ActiveSockets[i];

		if (!CachedMesh->DoesSocketExist(StartSocket) || !CachedMesh->DoesSocketExist(EndSocket))
		{
			// 소켓이 없으면 해당 소켓은 무시함
			UE_LOG(LogEnemyAI, Warning, TEXT("[HIT] 소켓 없음: %s 또는 %s"), *StartSocket.ToString(), *EndSocket.ToString());
			continue;
		}

		TArray<FHitResult> OutHits;

		// SphereTrace로 검사
		UKismetSystemLibrary::SphereTraceMulti(
			CachedMesh,                                          // World Context
			CachedMesh->GetSocketLocation(StartSocket),
			CachedMesh->GetSocketLocation(EndSocket),
			TraceRadius,
			UEngineTypes::ConvertToTraceType(TraceChannel),
			false,                                             // bTraceComplex
			ActorsToIgnore,
			DrawType,
			OutHits,
			true,                                              // bIgnoreSelf
			FLinearColor::Green,
			FLinearColor::Red,
			DebugDrawTime
		);

		// 충돌결과 탐색
		for (const FHitResult& Hit : OutHits)
		{
			AActor* Target = Hit.GetActor();

			if (!Target)
				continue;

			// 타겟이 적인지 확인. 현재 적끼리는 피해를 입지 않아야 한다.
			if (Cast<AEnemyCharacterBase>(Target))
				continue;

			// 구간 동안 매틱 도므로, 없으면 한 번 휘두르고 수십 대를 때린다.
			if (HitActors.Contains(Target))
				continue;

			HitActors.Add(Target);

			ApplyDamage(Target, Hit);
		}

	}
}

void UMeleeTraceComponent::EndTrace()
{
	bTracing = false;
	ActiveSockets.Reset();
	ActiveDamageEffect = nullptr;

	// 지워야 다음 공격에서 같은 대상을 다시 때릴 수 있다.
	HitActors.Empty();
}


// Called when the game starts
void UMeleeTraceComponent::BeginPlay()
{
	Super::BeginPlay();

	// 메쉬 기억하기
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		CachedMesh = OwnerCharacter->GetMesh();
	}
	
}

void UMeleeTraceComponent::ApplyDamage(AActor* Target, const FHitResult& Hit)
{
	AActor* Owner = GetOwner();

	//UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	//
	//// 대상에게 ASC 가 없으면 데미지를 못 준다. 판정이 나갔다는 것만 남긴다.
	//if (!TargetASC)
	//{
	//	UE_LOG(LogEnemyAI, Log, TEXT("[HIT] %s → %s (ASC 없음)"),*GetNameSafe(Owner), *GetNameSafe(Target));
	//	return;
	//}
	//
	//UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	//
	//// 자신에게 ASC 가 없으면 데미지를 못 준다. 판정이 나갔다는 것만 남긴다.
	//if (!SourceASC || !ActiveDamageEffect)
	//{
	//	UE_LOG(LogEnemyAI, Warning, TEXT("[HIT] %s DamageEffect 또는 ASC 없음"), *GetNameSafe(Owner));
	//	return;
	//}
	//
	//// 내 ASC 로 스펙을 만들고 대상에게 적용한다.
	//FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	//Context.AddSourceObject(Owner);
	//Context.AddHitResult(Hit);
	//
	//FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(ActiveDamageEffect, 1.f, Context);
	//if (!Spec.IsValid()) return;
	//
	//SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	//
	//UE_LOG(LogEnemyAI, Log, TEXT("[HIT] %s → %s"), *GetNameSafe(Owner), *GetNameSafe(Target));

	float AttackPower = 0.f;
	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		bool bFound = false;
		AttackPower = OwnerASC->GetGameplayAttributeValue(UEnemyAttributeSetBase::GetAttackPowerAttribute(), bFound);
	}

	// 대상이 인터페이스를 가진지 확인한다.
	if (!Target->Implements<UDamageableInterface>())
	{
		UE_LOG(LogEnemyAI, Log, TEXT("[HIT] %s → %s (Damageable 아님)"), *GetNameSafe(Owner), *GetNameSafe(Target));
		return;
	}

	// 맞을 수 있는 상태인지는 대상이 판단
	if (!IDamageableInterface::Execute_CanBeDamaged(Target, Owner))
	{
		UE_LOG(LogEnemyAI, Log, TEXT("[HIT] %s → %s (거부됨)"), *GetNameSafe(Owner), *GetNameSafe(Target));
		return;
	}

	// 수치만 넘긴다. GE 를 만드는 건 맞는 쪽의 몫.
	IDamageableInterface::Execute_ApplyMeleeHit(Target, Owner, AttackPower * DamageMultiplier, Hit);

	UE_LOG(LogEnemyAI, Log, TEXT("[HIT] %s → %s (%.0f)"), *GetNameSafe(Owner), *GetNameSafe(Target), AttackPower);
}
