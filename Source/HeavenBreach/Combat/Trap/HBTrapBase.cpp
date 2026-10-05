#include "Combat/Trap/HBTrapBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Components/PrimitiveComponent.h"
#include "Combat/Interface/DamageableInterface.h"
#include "AI/EnemyCharacterBase.h"
#include "Player/HBPlayerCharacter.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AHBTrapBase::AHBTrapBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// BP 에서 모델 · 충돌 박스를 붙일 자리.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	AbilitySystemComp = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	// 타격 큐가 이 ASC 로 나간다. 복제돼야 다른 화면에서도 이펙트가 보인다.
	AbilitySystemComp->SetIsReplicated(true);
	AbilitySystemComp->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

UAbilitySystemComponent* AHBTrapBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void AHBTrapBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHBTrapBase, Phase);
}

void AHBTrapBase::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComp->InitAbilityActorInfo(this, this);

	// 충돌 설정은 모든 화면에서 같아야 한다. 서버만 바꾸면 클라에서 몸이 막혀 이동 보정이 튄다.
	PrepareCollision();

	if (!HasAuthority())
	{
		return;
	}

	// 피격 쪽이 공격자 ASC 의 태그로 가드 불가 · 경직 · 타격 큐를 고른다.
	if (bUnblockable)
	{
		AbilitySystemComp->AddLooseGameplayTag(StateTag::TAG_State_Attacking_Unblockable);
	}

	if (bStagger)
	{
		AbilitySystemComp->AddLooseGameplayTag(StateTag::TAG_State_Attacking_Stagger);
	}

	if (HitCueTag.IsValid())
	{
		AbilitySystemComp->AddLooseGameplayTag(HitCueTag);
	}

	BindCollision();

	if (TriggerMode == EHBTrapTriggerMode::Always)
	{
		BeginActive();
		return;
	}

	if (TriggerMode == EHBTrapTriggerMode::Interval)
	{
		GetWorldTimerManager().SetTimer(IntervalTimer, this, &AHBTrapBase::RequestActivation, Interval, true);
	}
}

void AHBTrapBase::PrepareCollision()
{
	TArray<UPrimitiveComponent*> Primitives;
	GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		const bool bHitbox = Primitive->ComponentHasTag(HitboxTag);
		const bool bTrigger = Primitive->ComponentHasTag(TriggerVolumeTag);

		if (!bHitbox)
		{
			if (!bTrigger)
			{
				continue;
			}
		}

		// 판정용이라 아무것도 막지 않고 캐릭터만 겹침으로 잡는다.
		// 몸으로 막아야 하는 부분(벽 · 발판 바닥)은 태그 없는 충돌체를 따로 둔다.
		Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Primitive->SetCollisionResponseToAllChannels(ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Primitive->SetGenerateOverlapEvents(true);

		if (bHitbox)
		{
			Hitboxes.Add(Primitive);
		}

		if (bTrigger)
		{
			TriggerVolumes.Add(Primitive);
		}
	}
}

void AHBTrapBase::BindCollision()
{
	for (UPrimitiveComponent* Hitbox : Hitboxes)
	{
		Hitbox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AHBTrapBase::HandleHitboxBeginOverlap);
	}

	for (UPrimitiveComponent* Volume : TriggerVolumes)
	{
		Volume->OnComponentBeginOverlap.AddUniqueDynamic(this, &AHBTrapBase::HandleTriggerBeginOverlap);
	}

	if (Hitboxes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Trap] %s 판정 충돌체가 없다. 충돌 박스에 컴포넌트 태그 '%s' 를 달 것."),
			*GetName(), *HitboxTag.ToString());
	}
}

void AHBTrapBase::Activate()
{
	if (!HasAuthority())
	{
		return;
	}

	RequestActivation();
}

void AHBTrapBase::Deactivate()
{
	if (!HasAuthority())
	{
		return;
	}

	if (Phase != EHBTrapPhase::Active)
	{
		return;
	}

	EndActive();
}

void AHBTrapBase::RequestActivation()
{
	if (Phase != EHBTrapPhase::Idle)
	{
		return;
	}

	if (WarningDuration <= 0.f)
	{
		BeginActive();
		return;
	}

	SetPhase(EHBTrapPhase::Warning);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AHBTrapBase::BeginActive, WarningDuration, false);
}

void AHBTrapBase::BeginActive()
{
	// 켜질 때마다 새로 센다. 지난번에 맞은 대상도 바로 다시 맞는다.
	LastHitTimes.Reset();
	SetPhase(EHBTrapPhase::Active);

	// 이미 판정 안에 서 있던 대상. 겹침 시작 이벤트는 들어올 때만 오므로 직접 훑는다.
	HitAllOverlapping();

	// 머무는 대상을 HitInterval 마다 다시 때린다. 확인은 더 촘촘히 해서 간격이 밀리지 않게.
	GetWorldTimerManager().SetTimer(OverlapCheckTimer, this, &AHBTrapBase::HitAllOverlapping, OverlapCheckRate, true);

	if (TriggerMode == EHBTrapTriggerMode::Always)
	{
		return;
	}

	if (ActiveDuration <= 0.f)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AHBTrapBase::EndActive, ActiveDuration, false);
}

void AHBTrapBase::EndActive()
{
	GetWorldTimerManager().ClearTimer(OverlapCheckTimer);
	GetWorldTimerManager().ClearTimer(PhaseTimer);

	if (RearmDelay <= 0.f)
	{
		FinishCooldown();
		return;
	}

	SetPhase(EHBTrapPhase::Cooldown);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AHBTrapBase::FinishCooldown, RearmDelay, false);
}

void AHBTrapBase::FinishCooldown()
{
	SetPhase(EHBTrapPhase::Idle);

	if (TriggerMode != EHBTrapTriggerMode::Volume)
	{
		return;
	}

	// 발판 위에 계속 서 있으면 다시 켜진다. 겹침 시작 이벤트는 다시 오지 않으므로 직접 본다.
	for (UPrimitiveComponent* Volume : TriggerVolumes)
	{
		if (!IsValid(Volume))
		{
			continue;
		}

		TArray<AActor*> Overlapping;
		Volume->GetOverlappingActors(Overlapping);

		for (AActor* Actor : Overlapping)
		{
			if (!CanPressTrigger(Actor))
			{
				continue;
			}
			RequestActivation();
			return;
		}
	}
}

void AHBTrapBase::HitAllOverlapping()
{
	for (UPrimitiveComponent* Hitbox : Hitboxes)
	{
		if (!IsValid(Hitbox))
		{
			continue;
		}

		TArray<AActor*> Overlapping;
		Hitbox->GetOverlappingActors(Overlapping);

		for (AActor* Target : Overlapping)
		{
			TryHit(Target, MakeHitFor(Target, Hitbox));
		}
	}
}

void AHBTrapBase::HandleHitboxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!OtherActor)
	{
		return;
	}

	TryHit(OtherActor, MakeHitFor(OtherActor, OverlappedComp));
}

void AHBTrapBase::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (TriggerMode != EHBTrapTriggerMode::Volume)
	{
		return;
	}

	if (!CanPressTrigger(OtherActor))
	{
		return;
	}

	RequestActivation();
}

void AHBTrapBase::TryHit(AActor* Target, const FHitResult& Hit)
{
	if (Phase != EHBTrapPhase::Active)
	{
		return;
	}

	if (!IsTargetAllowed(Target))
	{
		return;
	}

	// 캡슐과 메쉬가 둘 다 겹치면 한 프레임에 두 번 들어온다. 대상 단위로 간격을 잰다.
	const double Now = GetWorld()->GetTimeSeconds();
	const TWeakObjectPtr<AActor> Key(Target);

	if (const double* LastHit = LastHitTimes.Find(Key))
	{
		if (Now - *LastHit < HitInterval)
		{
			return;
		}
	}

	LastHitTimes.Add(Key, Now);
	ApplyEffectTo(Target, Hit);
}

void AHBTrapBase::ApplyEffectTo(AActor* Target, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(Target))
	{
		return;
	}

	// 맞을 수 없는 대상(죽은 대상 등)은 피해도 상태이상도 건너뛴다.
	if (!Target->Implements<UDamageableInterface>())
	{
		return;
	}

	if (!IDamageableInterface::Execute_CanBeDamaged(Target, this))
	{
		return;
	}

	if (Damage > 0.f)
	{
		// 방어력 · 피격 모션 · 타격 큐 · HUD 피격 연출이 다른 공격과 똑같이 적용된다.
		IDamageableInterface::Execute_ApplyMeleeHit(Target, this, Damage, Hit, EHBDamageSource::Trap);
	}

	// 이번 피해로 죽었으면 상태이상은 걸지 않는다.
	if (!IDamageableInterface::Execute_CanBeDamaged(Target, this))
	{
		return;
	}

	ApplyStatusEffects(Target, Hit);
}

void AHBTrapBase::ApplyStatusEffects(AActor* Target, const FHitResult& Hit)
{
	if (StatusEffects.IsEmpty())
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		return;
	}

	// 건 쪽을 트랩으로 남긴다. GE 안에서 "누가 걸었나" 를 볼 때 트랩이 나온다.
	FGameplayEffectContextHandle Context = AbilitySystemComp->MakeEffectContext();
	Context.AddInstigator(this, this);
	Context.AddHitResult(Hit);

	for (const TSubclassOf<UGameplayEffect>& Effect : StatusEffects)
	{
		if (!Effect)
		{
			continue;
		}

		const FGameplayEffectSpecHandle Spec = AbilitySystemComp->MakeOutgoingSpec(Effect, 1.f, Context);
		if (!Spec.IsValid())
		{
			continue;
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

bool AHBTrapBase::MatchesTargetKind(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (Actor->IsA<AEnemyCharacterBase>())
	{
		return Targets != EHBTrapTargets::PlayersOnly;
	}

	if (Actor->IsA<AHBPlayerCharacter>())
	{
		return Targets != EHBTrapTargets::EnemiesOnly;
	}

	// 상자 · 아이템 등은 대상이 아니다.
	return false;
}

bool AHBTrapBase::IsTargetAllowed(const AActor* Target)
{
	if (!MatchesTargetKind(Target))
	{
		return false;
	}

	// 살아 있는 대상만 맞는다. 죽은 적 · 플레이어는 여기서 빠진다.
	if (!IDamageableInterface::Execute_CanBeDamaged(Target, this))
	{
		return false;
	}

	// 접속이 끊겨 남은 플레이어 캐릭터처럼 아무도 조종하지 않는 캐릭터도 맞지 않는다.
	if (Target->IsA<AHBPlayerCharacter>())
	{
		if (!Cast<APawn>(Target)->IsPlayerControlled())
		{
			return false;
		}
	}

	return true;
}

bool AHBTrapBase::CanPressTrigger(const AActor* Actor)
{
	// 감지: 살아 있는 대상만. 무게: 올라선 캐릭터면 시체도 누른다.
	if (TriggerSense == EHBTrapTriggerSense::Detection)
	{
		return IsTargetAllowed(Actor);
	}

	return MatchesTargetKind(Actor);
}

FHitResult AHBTrapBase::MakeHitFor(const AActor* Target, const UPrimitiveComponent* Hitbox) const
{
	// 겹침은 맞은 자리를 주지 않는다. 대상 위치를 맞은 자리로 써서 타격 이펙트가 대상 몸에서 터지게 한다.
	FHitResult Hit;
	Hit.Location = Target->GetActorLocation();
	Hit.ImpactPoint = Hit.Location;
	Hit.ImpactNormal = (Target->GetActorLocation() - Hitbox->GetComponentLocation()).GetSafeNormal();
	Hit.Normal = Hit.ImpactNormal;
	return Hit;
}

void AHBTrapBase::SetPhase(EHBTrapPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}

	const EHBTrapPhase OldPhase = Phase;
	Phase = NewPhase;
	ForceNetUpdate();

	// 서버는 OnRep 가 안 불린다. 리슨 방장 화면에도 연출이 나와야 하니 직접 부른다.
	BroadcastPhase(OldPhase);
}

void AHBTrapBase::OnRep_Phase(EHBTrapPhase OldPhase)
{
	BroadcastPhase(OldPhase);
}

void AHBTrapBase::BroadcastPhase(EHBTrapPhase OldPhase)
{
	if (Phase == EHBTrapPhase::Warning)
	{
		BP_OnTrapWarning();
		return;
	}

	if (Phase == EHBTrapPhase::Active)
	{
		BP_OnTrapActivated();
		return;
	}

	if (OldPhase == EHBTrapPhase::Active)
	{
		BP_OnTrapDeactivated();
	}
}
