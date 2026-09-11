// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GrabComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AI/Enum/EAlertState.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Core/Tag/Team/HB_TeamTag.h"

// Sets default values for this component's properties
UGrabComponent::UGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UGrabComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소유자의 메쉬 기억하기. 쓸 곳이많으니까.
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		CachedMesh = OwnerCharacter->GetMesh();
	}
	CacheOwnerTeamTag();
}

bool UGrabComponent::TryGrab(const FGrabSettings& InSettings)
{
	// 당연하지만 서버에서만 작동
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	Settings = InSettings;

	// 이미 잡고 있으면 다시 판정하지 않는다.
	if (GrabbedActor.IsValid())
	{
		return true;
	}

	// 이거 판정하려면 소켓이 최소 2개이상이어야함
	if (!CachedMesh || Settings.Sockets.Num() < 2)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Grab] 소켓이 2개 미만이거나 메쉬가 없다"));
		return false;
	}

	// 그랩 GE가 없다면 할필요가있나??
	if (!Settings.GrabbedEffect)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Grab] GrabbedEffect 가 비어있다"));
		return false;
	}

	// 혹시모르니 팀태그 초기화.
	CacheOwnerTeamTag();

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Owner);

	const EDrawDebugTrace::Type DrawType = Settings.bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	// 인접 소켓끼리 이어서 트레이스 검사
	for (int32 i = 1; i < Settings.Sockets.Num(); ++i)
	{
		const FName& StartSocket = Settings.Sockets[i - 1];
		const FName& EndSocket = Settings.Sockets[i];

		if (!CachedMesh->DoesSocketExist(StartSocket) || !CachedMesh->DoesSocketExist(EndSocket))
		{
			UE_LOG(LogEnemyAI, Warning, TEXT("[Grab] 소켓 없음: %s 또는 %s"), *StartSocket.ToString(), *EndSocket.ToString());
			continue;
		}

		TArray<FHitResult> OutHits;

		UKismetSystemLibrary::SphereTraceMulti(
			CachedMesh,
			CachedMesh->GetSocketLocation(StartSocket),
			CachedMesh->GetSocketLocation(EndSocket),
			Settings.TraceRadius,
			UEngineTypes::ConvertToTraceType(Settings.TraceChannel),
			false,
			ActorsToIgnore,
			DrawType,
			OutHits,
			true,
			FLinearColor::Green,
			FLinearColor::Red,
			Settings.DebugDrawTime
		);

		for (const FHitResult& Hit : OutHits)
		{
			AActor* Target = Hit.GetActor();

			// 타겟이 없거나 히트 대상이 자기자신은 좀...
			if (!Target || Target == Owner)
			{
				continue;
			}

			// 당연히 대상은 적이어야함.
			if (!Isenemy(Target))
			{
				continue;
			}

			// 데미지를 가할수 있는 대상이어야함
			if (!Target->Implements<UDamageableInterface>())
			{
				continue;
			}

			// 잡힐 수 있는 상태(공격에 적중이 가능한 상태)인지 판단
			if (!IDamageableInterface::Execute_CanBeDamaged(Target, Owner))
			{
				continue;
			}

			UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
			if (!TargetASC)
			{
				continue;
			}

			FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(Settings.GrabbedEffect, 1.f, TargetASC->MakeEffectContext());
			if (!Spec.IsValid())
			{
				continue;
			}
			// 대상에게 적용되는(구속) GE handle과 대상 액터, 히트정보 저장
			GrabbedHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			GrabbedActor = Target;
			GrabHit = Hit;

			// 부위 배율을 쓰지 않기위해 비움
			GrabHit.BoneName = NAME_None;
			GrabHit.MyBoneName = NAME_None;

			// 우치옳겨짐 옵션이 있다면 소켓사이로 위치옮김
			if (Settings.bSnapToHoldLocation)
			{
				FVector HoldLocation;
				if (GetHoldLocation(HoldLocation))
				{
					// 들어올리지 않으면 대상의 원래 높이를 유지한다. 발이 뜨면 어색하다.
					if (!Settings.bMatchSocketHeight)
					{
						HoldLocation.Z = Target->GetActorLocation().Z;
					}
					Target->SetActorLocation(HoldLocation, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}

			UE_LOG(LogEnemyAI, Log, TEXT("[Grab] %s → %s 붙잡음"), *GetNameSafe(Owner), *GetNameSafe(Target));
			return true;
		}
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Grab] %s 잡기 실패"), *GetNameSafe(Owner));
	return false;
}

void UGrabComponent::BiteGrabbed(float DamageMultiplier)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	AActor* Target = GrabbedActor.Get();
	if (!Target)
	{
		return;   // 잡은 게 없다. 실패 분기로 갔거나 대상이 사라졌다.
	}

	// 물기 중간에 무적이 되거나 죽었으면 더 물지 않는다.
	if (!IDamageableInterface::Execute_CanBeDamaged(Target, Owner))
	{
		return;
	}

	const float FinalDamage = GetOwnerAttackPower() * DamageMultiplier;

	// 잡을 때의 히트를 재사용하고 위치는 그 중앙으로 갱신한다.
	FHitResult Hit = GrabHit;
	FVector BitePoint;
	if (GetHoldLocation(BitePoint))
	{
		Hit.ImpactPoint = BitePoint;
		Hit.Location = BitePoint;
	}

	// 실제 적용량을 재기 위해 피해 전 체력을 기록한다.
	bool bMeasured = false;
	const float HealthBefore = GetTargetHealth(Target, bMeasured);

	// 수치만 넘긴다. GE 를 만드는 건 맞는 쪽의 몫.
	// 물기 공격이라 근접이다.
	IDamageableInterface::Execute_ApplyMeleeHit(Target, Owner, FinalDamage, Hit, EHBDamageSource::Melee);

	// 방어,경감이 반영된 값. 측정할 수 없으면 계산값을 그대로 쓴다.
	float AppliedDamage = FinalDamage;
	if (bMeasured)
	{
		bool bFoundAfter = false;
		const float HealthAfter = GetTargetHealth(Target, bFoundAfter);
		if (bFoundAfter)
		{
			AppliedDamage = FMath::Max(0.f, HealthBefore - HealthAfter);
		}
	}

	// 타격시 연출
	FGameplayTagContainer ImpactTags;
	if (Settings.ImpactTypeTag.IsValid())
	{
		ImpactTags.AddTag(Settings.ImpactTypeTag);
	}
	HitCuePlay::PlayImpact(Owner, Target, Hit, AppliedDamage, ImpactTags);

	// 흡혈. 실제로 들어간 만큼만 회복한다.
	const float HealAmount = AppliedDamage * Settings.LifestealRatio;
	float OwnerHPBefore = 0.f;
	float OwnerHPAfter = 0.f;

	if (Settings.BiteHealEffect && Settings.HealMagnitudeTag.IsValid() && HealAmount > 0.f)
	{
		if (UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			bool bFound = false;
			OwnerHPBefore = OwnerASC->GetGameplayAttributeValue(UEnemyAttributeSetBase::GetHPAttribute(), bFound);

			FGameplayEffectSpecHandle HealSpec = OwnerASC->MakeOutgoingSpec(Settings.BiteHealEffect, 1.f, OwnerASC->MakeEffectContext());
			if (HealSpec.IsValid())
			{
				HealSpec.Data->SetSetByCallerMagnitude(Settings.HealMagnitudeTag, HealAmount);
				OwnerASC->ApplyGameplayEffectSpecToSelf(*HealSpec.Data.Get());
			}

			OwnerHPAfter = OwnerASC->GetGameplayAttributeValue(UEnemyAttributeSetBase::GetHPAttribute(), bFound);
		}
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Bite] %s → %s  계산=%.0f 적용=%.0f 회복=%.0f  내HP %.0f→%.0f"), *GetNameSafe(Owner), *GetNameSafe(Target), FinalDamage, AppliedDamage, HealAmount, OwnerHPBefore, OwnerHPAfter);
}

void UGrabComponent::ReleaseGrab()
{
	if (GrabbedHandle.IsValid())
	{
		// 대상의 ASC 에서 GE(그랩)을 지운다.
		if (AActor* Target = GrabbedActor.Get())
		{
			if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
			{
				TargetASC->RemoveActiveGameplayEffect(GrabbedHandle);
			}
		}
		GrabbedHandle.Invalidate();
	}

	// 정보 초기화.
	GrabbedActor.Reset();
	GrabHit = FHitResult();
	OwnerTeamTag.Reset();
}

void UGrabComponent::CacheOwnerTeamTag()
{
	OwnerTeamTag.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 현재 소유자의 팀 소속 확인하여 저장
	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		FGameplayTagContainer All;
		OwnerASC->GetOwnedGameplayTags(All);

		FGameplayTagContainer TeamTags;
		TeamTags.AddTag(TeamTag::TAG_Team_Player);
		TeamTags.AddTag(TeamTag::TAG_Team_Enemy);

		OwnerTeamTag = All.Filter(TeamTags);
	}
}

bool UGrabComponent::Isenemy(const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	// 공격자가 보유한 팀 태그를 하나라도 가진 대상은 같은 편이므로 제외한다.
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (TargetASC && !OwnerTeamTag.IsEmpty() && TargetASC->HasAnyMatchingGameplayTags(OwnerTeamTag))
	{
		return false;
	}

	// 유효한 대상
	return true;
}

float UGrabComponent::GetOwnerAttackPower() const
{
	float AttackPower = 0.f;
	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		bool bFound = false;
		AttackPower = OwnerASC->GetGameplayAttributeValue(UEnemyAttributeSetBase::GetAttackPowerAttribute(), bFound);
	}
	return AttackPower;
}

float UGrabComponent::GetTargetHealth(const AActor* Target, bool& bOutFound) const
{
	bOutFound = false;

	if (!Target || !Settings.TargetHealthAttribute.IsValid())
	{
		return 0.f;
	}

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
	{
		return TargetASC->GetGameplayAttributeValue(Settings.TargetHealthAttribute, bOutFound);
	}

	return 0.f;
}

bool UGrabComponent::GetHoldLocation(FVector& OutLocation) const
{
	if (!CachedMesh)
	{
		return false;
	}

	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;

	// 소켓의 위치값 합산
	for (const FName& SocketName : Settings.Sockets)
	{
		if (!CachedMesh->DoesSocketExist(SocketName))
		{
			continue;
		}
		Sum += CachedMesh->GetSocketLocation(SocketName);
		++Count;
	}

	if (Count == 0)
	{
		return false;
	}

	// 소켓만큼 나눠 평균 연산
	OutLocation = Sum / (float)Count;

	// 소켓 중점에서 소유자 정면으로 밀어낸다.
	if (!FMath::IsNearlyZero(Settings.HoldForwardOffset))
	{
		if (const AActor* Owner = GetOwner())
		{
			FVector Forward = Owner->GetActorForwardVector();
			Forward.Z = 0.f;
			Forward = Forward.GetSafeNormal();

			OutLocation += Forward * Settings.HoldForwardOffset;
		}
	}

	return true;
}

