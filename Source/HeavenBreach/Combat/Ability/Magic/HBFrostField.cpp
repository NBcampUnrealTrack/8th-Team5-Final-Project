// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/HBFrostField.h"

#include "AI/EnemyCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Components/SceneComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Ability/HBSkillTargets.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Core/Tag/Cue/HB_CueTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/HBPlayerCharacter.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	// 연출 BP. 테스트 레벨 에셋이라 하드 참조하지 않고 필요할 때 로드한다.
	const TCHAR* FrostFieldFxPath = TEXT("/Game/Levels/test_gm/MageRetarget/VFX/FrostField/BP_Test_MageFrostFieldFX.BP_Test_MageFrostFieldFX_C");

	// 연출 BP 메시는 반경 200cm 기준이라 Radius / 200 을 XY 배율로 쓴다.
	constexpr float FxBaseRadius = 200.f;
	constexpr float FxExtraLife = 0.15f;
	constexpr float MinDuration = 0.1f;	// GA_MagicFrostField::Duration 의 ClampMin 과 같다
}

AHBFrostField::AHBFrostField()
{
	bReplicates = true;
	SetReplicateMovement(false);	// 스폰 위치는 첫 번들에 실린다. 움직이지 않는다.

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AHBFrostField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHBFrostField, Radius);
	DOREPLIFETIME(AHBFrostField, Duration);
}

void AHBFrostField::Init(AHBPlayerCharacter* InOwner, const float InRadius, const float InDuration, const float InSlow,
	const float InDamageCoef, const float InDamageInterval, const int32 InCastId)
{
	Caster = InOwner;
	Radius = InRadius;
	Slow = InSlow;
	DamageCoef = FMath::Max(0.f, InDamageCoef);
	DamageInterval = FMath::Max(0.1f, InDamageInterval);
	CastId = InCastId;
	// SetLifeSpan(0) 은 "수명 없음" 이라 지대가 영원히 남는다. 0 이하가 들어오면 최소값으로 막는다.
	if (InDuration <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 서리 지대 지속 시간이 0 이하(%.2f) — %.2f초로 보정"), InDuration, MinDuration);
	}
	Duration = FMath::Max(InDuration, MinDuration);
	SetLifeSpan(Duration);
}

void AHBFrostField::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// 첫 판정은 바로, 이후 0.25초마다.
		TickField();
		GetWorldTimerManager().SetTimer(TickTimerHandle, this, &AHBFrostField::TickField, TickInterval, true);

		// 도트: 생기는 순간 한 번, 이후 DamageInterval 마다. 수명(Duration)이 끝나면 액터와 함께 멈춘다.
		if (DamageCoef > 0.f)
		{
			TickDamage();
			GetWorldTimerManager().SetTimer(DamageTimerHandle, this, &AHBFrostField::TickDamage, DamageInterval, true);
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		SpawnLocalVisual();
	}
}

void AHBFrostField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(TickTimerHandle);
		GetWorldTimerManager().ClearTimer(DamageTimerHandle);
		RemoveAllSlows();
	}
	Super::EndPlay(EndPlayReason);
}

void AHBFrostField::TickField()
{
	UWorld* World = GetWorld();
	AHBPlayerCharacter* Player = Caster.Get();
	UAbilitySystemComponent* SourceASC = Player ? Player->GetAbilitySystemComponent() : nullptr;
	if (!World || !HasAuthority())
	{
		return;
	}
	if (!SourceASC)
	{
		// 시전자가 사라졌다. 감속을 남기지 않는다.
		RemoveAllSlows();
		return;
	}

	// 영역 안의 살아 있는 적. 적 채널이 "Enemy" 라 움직이는 오브젝트 전부를 보고 AEnemyCharacterBase 로 거른다(표식과 같다).
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FrostField), false, Player);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects), FCollisionShape::MakeSphere(Radius), QueryParams);

	TSet<TWeakObjectPtr<AEnemyCharacterBase>> Inside;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(Overlap.GetActor());
		if (!Enemy)
		{
			continue;
		}
		UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
		{
			continue;
		}

		const TWeakObjectPtr<AEnemyCharacterBase> Key(Enemy);
		Inside.Add(Key);
		if (SlowHandles.Contains(Key))
		{
			continue;	// 이미 감속 중. 겹치게 걸지 않는다.
		}
		if (TargetASC->HasMatchingGameplayTag(MagicTag::TAG_State_Frosted))
		{
			// 다른 지대(다른 플레이어 포함)가 이미 감속 중. 감속은 겹치지 않는다(같은 GE 를 두 번 걸면 Additive 라 두 배가 된다).
			// 핸들을 기록하지 않으므로, 그쪽 감속이 풀린 뒤에도 안에 있으면 다음 판정에서 이 지대가 건다.
			continue;
		}

		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UGE_FrostSlow::StaticClass(), 1.f, SourceASC->MakeEffectContext());
		if (!Spec.IsValid())
		{
			continue;
		}
		Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_AugmentMagnitude, -Slow);
		const FActiveGameplayEffectHandle Handle = SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		if (Handle.IsValid())
		{
			SlowHandles.Add(Key, Handle);
		}
	}

	// 영역을 벗어났거나 죽었거나 사라진 적은 뗀다.
	for (auto It = SlowHandles.CreateIterator(); It; ++It)
	{
		if (!Inside.Contains(It.Key()))
		{
			RemoveSlow(It.Value());
			It.RemoveCurrent();
		}
	}
}

void AHBFrostField::TickDamage()
{
	AHBPlayerCharacter* Player = Caster.Get();
	if (!HasAuthority() || !Player)
	{
		return;
	}

	// 피해는 몬스터와 팀원 모두(팀킬 규칙은 화살·평타와 같다). 감속은 TickField 가 몬스터에게만 건다.
	TArray<AActor*> Enemies;
	HBSkillTargets::FindInSphere(GetWorld(), GetActorLocation(), Radius, Player, Enemies);

	for (AActor* Enemy : Enemies)
	{
		if (!Enemy->Implements<UDamageableInterface>() || !IDamageableInterface::Execute_CanBeDamaged(Enemy, Player))
		{
			continue;
		}

		// 공격력 × 계수 × 스킬 피해 배율 → 패시브 보정. 지팡이 보너스는 스킬 피해 배율(무기 행 SkillDamageRate)에 들어 있다.
		// 지대는 시전이 끝난 뒤에도 남아 자기 시전 ID 로만 잔류 마력을 알린다.
		float FinalDamage = Player->GetAttackPower() * DamageCoef * Player->GetSkillDamageMultiplier();
		FinalDamage = Player->ModifyOutgoingDamage(Enemy, FinalDamage, /*bCreditCurrentSkillCast*/ false);
		if (CastId > 0)
		{
			Player->NotifySkillHit(CastId);
		}

		FHitResult Hit;
		Hit.ImpactPoint = Enemy->GetActorLocation();
		Hit.Location = Hit.ImpactPoint;
		Hit.ImpactNormal = FVector::UpVector;
		Hit.Normal = Hit.ImpactNormal;
		Hit.TraceStart = GetActorLocation();
		Hit.TraceEnd = Hit.ImpactPoint;
		{
			// 맞는 쪽 큐가 마법 타격음을 고르게 맞는 순간에만 성질을 붙인다(원소 구체와 같다).
			HitCuePlay::FScopedImpactTag ImpactScope(Player, CueTag::TAG_GameplayCue_Hit_Magic);
			IDamageableInterface::Execute_ApplyMeleeHit(Enemy, Player, FinalDamage, Hit, EHBDamageSource::Ranged);
		}

		const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
		const bool bKilled = TargetASC && TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
		Player->NotifyDamageDealt(Enemy, FinalDamage, bKilled);
	}

	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 서리 지대 도트  적중=%d  계수=%.2f"), Enemies.Num(), DamageCoef);
}

void AHBFrostField::RemoveSlow(const FActiveGameplayEffectHandle& Handle)
{
	// 핸들은 주인 ASC 를 약참조로 안다. 적이 이미 사라졌으면 null 이라 건너뛴다.
	if (UAbilitySystemComponent* OwnerASC = Handle.GetOwningAbilitySystemComponent())
	{
		OwnerASC->RemoveActiveGameplayEffect(Handle);
	}
}

void AHBFrostField::RemoveAllSlows()
{
	for (const TPair<TWeakObjectPtr<AEnemyCharacterBase>, FActiveGameplayEffectHandle>& Pair : SlowHandles)
	{
		RemoveSlow(Pair.Value);
	}
	SlowHandles.Reset();
}

void AHBFrostField::SpawnLocalVisual()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const TSoftClassPtr<AActor> FxClassPtr{ FSoftObjectPath(FrostFieldFxPath) };
	UClass* FxClass = FxClassPtr.LoadSynchronous();
	if (!FxClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 서리 지대 연출 BP 를 불러오지 못함  경로=%s"), FrostFieldFxPath);
		return;
	}

	const float XY = Radius / FxBaseRadius;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 로컬 전용(복제 안 함). 지대에 붙이지 않고 수명으로 정리한다.
	AActor* Fx = World->SpawnActor<AActor>(FxClass, FTransform(GetActorRotation(), GetActorLocation(), FVector(XY, XY, 1.f)), SpawnParams);
	if (!Fx)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 서리 지대 연출 스폰 실패"));
		return;
	}
	Fx->SetLifeSpan(Duration + FxExtraLife);

	// BP 의 FieldDuration 변수(있으면)에 지속 시간을 알린다. 연출 쪽 페이드 타이밍용.
	if (FProperty* Prop = FxClass->FindPropertyByName(TEXT("FieldDuration")))
	{
		// 정수 프로퍼티에 SetFloatingPointPropertyValue 를 부르면 엔진이 check 로 멈춘다. 종류를 나눠 쓴다.
		FNumericProperty* Num = CastField<FNumericProperty>(Prop);
		if (Num && Num->IsFloatingPoint())
		{
			Num->SetFloatingPointPropertyValue(Num->ContainerPtrToValuePtr<void>(Fx), static_cast<double>(Duration));
		}
		else if (Num && Num->IsInteger())
		{
			Num->SetIntPropertyValue(Num->ContainerPtrToValuePtr<void>(Fx), static_cast<int64>(FMath::RoundToInt(Duration)));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 서리 지대 연출 BP 의 FieldDuration 이 숫자 변수가 아님"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 서리 지대 연출 BP 에 FieldDuration 변수 없음 (선택 사항)"));
	}
}
