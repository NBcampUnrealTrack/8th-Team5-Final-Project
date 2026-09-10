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
#include "Core/Tag/Team/HB_TeamTag.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Player/HBPlayerCharacter.h"
#include "EquipmentComponent.h"

// Sets default values for this component's properties
UMeleeTraceComponent::UMeleeTraceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UMeleeTraceComponent::BeginTrace(const TArray<FName>& InSockets, TSubclassOf<UGameplayEffect> InDamageEffect, float InRadius, float InDamageMultiplier, bool bInUseHitStop, float InHitStopDuration, float InHitStopPlayRate)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	float Pos = -1.f;
	UAnimMontage* CurMontage = nullptr;
	void* InstPtr = nullptr;

	if (UAnimInstance* Anim = CachedMesh->GetAnimInstance())
	{
		CurMontage = Anim->GetCurrentActiveMontage();
		if (CurMontage)
		{
			Pos = Anim->Montage_GetPosition(CurMontage);
		}
		InstPtr = Anim->GetActiveMontageInstance();
	}

	UE_LOG(LogEnemyAI, Warning, TEXT("[Trace] Begin  Pos=%.3f  Inst=%p"), Pos, InstPtr);

	ActiveSockets = InSockets;
	TraceRadius = InRadius;
	ActiveDamageEffect = InDamageEffect;
	DamageMultiplier = InDamageMultiplier;
	bUseHitStop = bInUseHitStop;

	HitStopDuration = InHitStopDuration > 0.f ? InHitStopDuration : 0.4;
	HitStopPlayRate = InHitStopPlayRate > 0.f ? InHitStopPlayRate : 0.05;

	// 실제 사용될 소켓
	ActiveSocketSource = ResolveSocketSource();

	// 현재 소유자의 팀소속확인
	OwnerTeamTag.Reset();
	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		FGameplayTagContainer All;
		OwnerASC->GetOwnedGameplayTags(All);

		FGameplayTagContainer TeamTags;
		TeamTags.AddTag(TeamTag::TAG_Team_Player);
		TeamTags.AddTag(TeamTag::TAG_Team_Enemy);

		OwnerTeamTag = All.Filter(TeamTags);
	}

	// 이전 공격의 기록이 남으면 안 된다.
	HitActors.Empty();
	bTracing = true;
	bDurabilityConsumedThisSwing = false;

	UE_LOG(LogEnemyAI, Warning, TEXT("[Trace] %s Begin"), *GetNameSafe(Owner));
}

void UMeleeTraceComponent::UpdateTrace()
{
	AActor* Owner = GetOwner();

	// 히트 스톱중에는 판정중지
	if (!Owner || !Owner->HasAuthority() || !bTracing || bHitStopActive)
		return;

	// 소켓을 가진 컴포넌트. 없으면 작동불가.
	USceneComponent* SocketSource = ActiveSocketSource.Get();
	if (!SocketSource || ActiveSockets.Num() < 2)
		return;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Owner);   // 자기 자신은 제외

	const EDrawDebugTrace::Type DrawType = bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	// 인접 소켓끼리 이어서 트레이스검사
	for (int32 i = 1; i < ActiveSockets.Num(); ++i)
	{
		const FName& StartSocket = ActiveSockets[i - 1];
		const FName& EndSocket = ActiveSockets[i];

		// 소켓이 없으면 무시함
		if (!SocketSource->DoesSocketExist(StartSocket) || !SocketSource->DoesSocketExist(EndSocket))
		{
			UE_LOG(LogEnemyAI, Warning, TEXT("[HIT] 소켓 없음: %s 또는 %s"), *StartSocket.ToString(), *EndSocket.ToString());
			continue;
		}

		float MontagePos = -1.f;
		float MontageRate = -1.f;
		if (CachedMesh)
		{
			if (UAnimInstance* Anim = CachedMesh->GetAnimInstance())
			{
				if (UAnimMontage* Cur = Anim->GetCurrentActiveMontage())
				{
					MontagePos = Anim->Montage_GetPosition(Cur);
					MontageRate = Anim->Montage_GetPlayRate(Cur);
				}
			}
		}

		const FVector SweepStart = SocketSource->GetSocketLocation(StartSocket);
		const FVector SweepEnd = SocketSource->GetSocketLocation(EndSocket);

		UE_LOG(LogEnemyAI, Warning,
			TEXT("[Sweep] 소스=%s(%s) %s→%s 길이=%.0f 몸까지=%.0f/%.0f R=%.0f 몽타주 Pos=%.3f Rate=%.2f"),
			*GetNameSafe(SocketSource),
			*GetNameSafe(SocketSource->GetClass()),
			*StartSocket.ToString(), *EndSocket.ToString(),
			(SweepEnd - SweepStart).Size(),
			FVector::Dist(Owner->GetActorLocation(), SweepStart),
			FVector::Dist(Owner->GetActorLocation(), SweepEnd),
			TraceRadius,
			MontagePos, MontageRate);

		TArray<FHitResult> OutHits;

		// SphereTrace로 검사
		UKismetSystemLibrary::SphereTraceMulti(
			SocketSource,                                          // World Context
			SweepStart,
			SweepEnd,
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

			// 타겟이 적인지 확인
			// 태그를 감별하여 공격자가 보유중인 팀 태그들중에서 하나라도 있는 적은 스킵한다.
			UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
			if (TargetASC && !OwnerTeamTag.IsEmpty() && TargetASC->HasAnyMatchingGameplayTags(OwnerTeamTag))
			{
				continue;
			}

			// 구간 동안 매틱 도므로, 없으면 한 번 휘두르고 수십 대를 때린다.
			if (HitActors.Contains(Target))
				continue;

			HitActors.Add(Target);

			ApplyDamage(Target, Hit);

			// 적중은 한번에 하나씩. 히트스탑이 끝나고나서 진행되어야함.
			if (bUseHitStop)
			{
				TriggerHitStop();
				return;
			}
			else
			{
				UE_LOG(LogEnemyAI, Warning, TEXT("[HitStop] bUseHitStop 꺼져있음"));
			}
		}

	}
}

void UMeleeTraceComponent::EndTrace()
{
	bTracing = false;
	ActiveSockets.Reset();
	ActiveSocketSource.Reset();
	ActiveDamageEffect = nullptr;

	// 스윙이 끝났는데 느린 상태로 굳으면 안 된다.
	if (bHitStopActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HitStopTimer);
		}
		EndHitStop();
	}
	bUseHitStop = false;

	// 지워야 다음 공격에서 같은 대상을 다시 때릴 수 있다.
	HitActors.Empty();
}

void UMeleeTraceComponent::ClientHitStop_Implementation(float Duration, float PlayRate)
{
	UWorld* World = GetWorld();
	if (!CachedMesh || !World) return;

	UAnimInstance* AnimInstance = CachedMesh->GetAnimInstance();
	if (!AnimInstance) return;

	UAnimMontage* Montage = AnimInstance->GetCurrentActiveMontage();
	if (!Montage) return;

	// 여기서 ASC 를 쓰면 서버로 RPC 가 되돌아가 서버 값을 덮는다. 직접 건다.
	SavedPlayRate = AnimInstance->Montage_GetPlayRate(Montage);
	HitStopMontage = Montage;
	bHitStopActive = true;

	AnimInstance->Montage_SetPlayRate(Montage, PlayRate);
	World->GetTimerManager().SetTimer(HitStopTimer, this, &UMeleeTraceComponent::EndHitStop, Duration, false);

	UE_LOG(LogEnemyAI, Warning, TEXT("[HitStop] 클라 적용 %s  %.2f -> %.2f"),
		*GetNameSafe(Montage), SavedPlayRate, PlayRate);
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

	// 플레이어는 무기 공격력과 버프까지 반영된 자기 함수를 쓴다.
	// 적에는 그 계산이 없어서 어트리뷰트를 직접 읽는다.
	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(Owner);

	float AttackPower = 0.f;
	if (Player)
	{
		// 플레이어의 공격력 가져옴
		AttackPower = Player->GetAttackPower();
	}
	else if (UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
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

	// 데미지값 배율합해서 연산
	float FinalDamage = AttackPower * DamageMultiplier;

	// 플레이어 패시브 보정 (연속공격 / 약점간파 / 복수 / 매의눈...)
	if (Player)
	{
		FinalDamage = Player->ModifyOutgoingDamage(Target, FinalDamage);
	}

	// 수치만 넘긴다. GE 를 만드는 것도 타격 연출도 맞는 쪽의 몫.
	//
	// 큐를 여기서 안 쏜다: 막았는지 패링했는지는 맞는 쪽만 안다.
	// 여기서 쏘면 무기 성질밖에 몰라서 막아도 살점 소리가 난다.
	IDamageableInterface::Execute_ApplyMeleeHit(Target, Owner, FinalDamage, Hit, EHBDamageSource::Melee);

	// 플레이어가 흡혈이나 사망 판정, 보수를 여기서해야한다...
	if (Player)
	{
		const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		const bool bKilled = TargetASC && TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
		Player->NotifyDamageDealt(Target, FinalDamage, bKilled);

		// 스윙 1회당 내구도 1씩 감소. 여러 명을 맞혀도 한 번만 깎이고, 아무도 못 맞히면 안 깎인다.
		if (!bDurabilityConsumedThisSwing)
		{
			if (UEquipmentComponent* Equipment = Player->GetEquipment())
			{
				bool bJustBroke = false;
				Equipment->DamageEquippedItemDurability(EEquipSlotType::PrimaryWeapon, 1.f, bJustBroke);
			}
			bDurabilityConsumedThisSwing = true;
		}
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[HIT] %s → %s (%.0f)"), *GetNameSafe(Owner), *GetNameSafe(Target), FinalDamage);
}

USceneComponent* UMeleeTraceComponent::ResolveSocketSource() const
{
	AActor* Owner = GetOwner();
	if (!Owner || ActiveSockets.Num() < 2 || WeaponComponentTag.IsNone())
	{
		return CachedMesh;
	}

	// 무기를 본다.
	TArray<UActorComponent*> Found = Owner->GetComponentsByTag(USceneComponent::StaticClass(), WeaponComponentTag);

	for (UActorComponent* Component : Found)
	{
		USceneComponent* Scene = Cast<USceneComponent>(Component);
		if (!Scene)
		{
			continue;
		}

		// 소켓이 전부 있어야 그 무기로 판정한다. 하나라도 없으면 캐릭터 메쉬로 간다.
		bool bHasAll = true;
		for (const FName& SocketName : ActiveSockets)
		{
			if (!Scene->DoesSocketExist(SocketName))
			{
				bHasAll = false;
				break;
			}
		}

		if (bHasAll)
		{
			return Scene;
		}
	}

	// 여까지오면 캐릭터 메쉬로 본다.
	//
	// 조용히 떨어지면 안 된다. 캐릭터 메쉬에는 무기 소켓이 없으므로
	// 소켓 조회가 엉뚱한 위치를 돌려주고, 트레이스 길이가 애니메이션 따라
	// 출렁이면서 사거리가 반토막 난다. "매우 가까이 가야 맞는" 증상이 이것이다.
	//
	// 2026-09-09 데디 로그에서 실제로 이 일이 있었다. 무기 메시 애셋이 서버
	// 빌드에 없어서(쿠킹 누락) 무기 컴포넌트가 서지 못했고, 몇 주간 원인을
	// 못 찾았다. 로그가 없어서였다.
	//
	// 쳐낼 조건: 무기 없는 맨손 공격을 정식으로 지원하게 되면, 그때는
	// 맨손 전용 소켓 세트를 따로 두고 이 경고를 없앤다.
	FString SocketList;
	for (const FName& SocketName : ActiveSockets)
	{
		if (!SocketList.IsEmpty())
		{
			SocketList.Append(TEXT("/"));
		}
		SocketList.Append(SocketName.ToString());
	}

	UE_LOG(LogEnemyAI, Warning,
		TEXT("[Trace] %s 무기 컴포넌트(태그=%s)를 못 찾아 캐릭터 메쉬로 폴백한다. 필요 소켓=%s")
		TEXT(" → 사거리가 짧아진다. 무기 메시가 로드됐는지 확인할 것"),
		*GetNameSafe(Owner),
		*WeaponComponentTag.ToString(),
		*SocketList);

	return CachedMesh;
}

void UMeleeTraceComponent::TriggerHitStop()
{
	UWorld* World = GetWorld();
	if (!CachedMesh || !World)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[HitStop] 메쉬 또는 월드 없음"));
		return;
	}

	UAnimInstance* AnimInstance = CachedMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[HitStop] AnimInstance 없음"));
		return;
	}

	UAnimMontage* Montage = AnimInstance->GetCurrentActiveMontage();
	if (!Montage)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[HitStop] 재생 중인 몽타주 없음"));
		return;
	}

	// 복구할 속도를 기억한다. 기존 속도를 유지시켜야 한다.
	SavedPlayRate = AnimInstance->Montage_GetPlayRate(Montage);
	HitStopMontage = Montage;
	bHitStopActive = true;
	// 히트스탑이 끝나는것 타이머 설정.
	World->GetTimerManager().SetTimer(HitStopTimer, this, &UMeleeTraceComponent::EndHitStop, HitStopDuration, false);

	// AnimInstance->Montage_SetPlayRate안하는이유 : 서버-클라동기화이슈. ASC에 더 좋은거잇으니 그거씀
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->CurrentMontageSetPlayRate(HitStopPlayRate);
	}

	// ASC 는 소유자에게 안 보낸다. 소유자는 몽타주를 예측 중이라 서버가 덮으면 화면이 튄다. 그래서 때린 사람에게만 따로 보낸다.
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->IsLocallyControlled())
	{
		ClientHitStop(HitStopDuration, HitStopPlayRate);
	}

	UE_LOG(LogEnemyAI, Warning, TEXT("[HitStop] 적용 %s  %.2f -> %.2f  %.3fs  Owner=%s"),
		*GetNameSafe(Montage), SavedPlayRate, HitStopPlayRate, HitStopDuration, *GetNameSafe(GetOwner()));
}

void UMeleeTraceComponent::EndHitStop()
{
	bHitStopActive = false;

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// 서버 + 구경하는 클라
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
		{
			ASC->CurrentMontageSetPlayRate(SavedPlayRate);
		}
	}
	else if (CachedMesh)
	{
		// 때린 클라는 자기 것만 되돌린다. 타이머가 각자 돌아서 패킷 유실 걱정이 없다.
		if (UAnimInstance* AnimInstance = CachedMesh->GetAnimInstance())
		{
			if (UAnimMontage* Montage = HitStopMontage.Get())
			{
				AnimInstance->Montage_SetPlayRate(Montage, SavedPlayRate);
			}
		}
	}


	HitStopMontage.Reset();
}
