// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnemyCharacterBase.h"
#include "AI/EnemyAIControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "AI/Enum/EAlertState.h"
#include "Components/CapsuleComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "AI/AlertComponent.h"
#include "AI/LootComponent.h"
#include "AI/DataRow/EnemyArchetypeData.h"
#include "Combat/Damage/HBDamageCalculation.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "AIController.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Tag/Team/HB_TeamTag.h"
#include "Core/Tag/Cue/HB_CueTag.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "EngineUtils.h"

// Sets default values
AEnemyCharacterBase::AEnemyCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// 멀티플레이만들거니까 레플리케이션을 킨다
	bReplicates = true;

	// 기본 값. 자식상속된 컨트롤러를 쓴다면 블루프린트에서 연결필요.
	AIControllerClass = AEnemyAIControllerBase::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false; // 컨트롤러는 몸 회전에 손대지 않는다
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true; // 몸이 이동하는 방향으로 회전 한다
		Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
	}

	// ----- GAS -----
	
	AbilitySystemComp = CreateDefaultSubobject<UEnemyAbilitySystemComponent>(TEXT("AbilitySystemComp"));
	// 어트리뷰트와 태그를 클라이언트에 복제하려면 필요하다.
	AbilitySystemComp->SetIsReplicated(true);
	// AI적은 레플리 케이션모드가 Minimal이 적합하다. 서버 위주 로직이기에 다른 클라가 자세히 알피룡없음
	AbilitySystemComp->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// GAS스텟 담당 어트리뷰트 셋
	AttributeSet = CreateDefaultSubobject<UEnemyAttributeSetBase>(TEXT("AttributeSet"));

	// 루팅 인벤토리
	LootComponent = CreateDefaultSubobject<ULootComponent>(TEXT("LootInven"));

	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));

}

void AEnemyCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnemyCharacterBase, bDead);
}

bool AEnemyCharacterBase::CanBeDamaged_Implementation(AActor* InstigatorActor) const
{
	if (!AbilitySystemComp)
	{
		return false;
	}

	// 이미 죽었으면 더 맞지 않는다.
	if (AbilitySystemComp->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
	{
		return false;
	}

	// 무적 같은 조건이 생기면 여기에 추가한다.
	return true;
}

void AEnemyCharacterBase::ApplyMeleeHit_Implementation(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit, EHBDamageSource Source)
{
	// 데미지 확정은 서버 전용.
	if (!HasAuthority() || !AbilitySystemComp || !IncomingDamageEffect)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComp->MakeEffectContext();
	Context.AddInstigator(InstigatorActor, InstigatorActor);   // 누가 때렸나
	Context.AddHitResult(Hit);

	FGameplayEffectSpecHandle Spec = AbilitySystemComp->MakeOutgoingSpec(IncomingDamageEffect, 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	// 이놈은 진짜로 날공격하는 올바른대상이맞음
	LastDamageInstigator = InstigatorActor;

	// BP에서 지정한 부위 배율을 우선 사용하고, 별도 지정이 없는 머리는 기본 2배다.
	float PartMult = HBDamage::IsHeadshot(Hit) ? 2.f : 1.f;
	if (const float* Found = BoneDamageMultipliers.Find(Hit.BoneName))
	{
		PartMult = *Found;
	}

	// 머리면 때린 사람의 헤드샷 옵션을 곱한다. BP 에서 지정한 부위 배율 위에 얹는다.
	// 적끼리 때렸거나 옵션이 없으면 보너스 0 이라 그대로다.
	if (HBDamage::IsHeadshot(Hit))
	{
		const float Bonus = HBDamage::GetAttackerHeadshotBonus(InstigatorActor);
		PartMult *= FMath::Max(0.f, 1.f + Bonus);
	}

	// DT 가 아니라 어트리뷰트에서 읽는다. 스폰 시 InitStats 가 DT 값을 넣어주고,
	// 그 뒤로는 GE 가 올리고 내릴 수 있다. 방어구 파괴 · 페이즈별 방어력이 여기로 온다.
	//
	// 쳐낼 조건: 없다. DT 직독으로 돌아가면 GE 를 못 쓴다.
	const float TargetDefense = AbilitySystemComp
		? AbilitySystemComp->GetNumericAttribute(UEnemyAttributeSetBase::GetDefenseAttribute())
		: 0.f;
	const float FinalDamage = HBDamage::CalculateFinalDamage(RawDamage, TargetDefense, PartMult);

	UE_LOG(LogEnemyAI, Warning,
		TEXT("[Hit] Bone=%s Comp=%s Raw=%.2f Defense=%.2f HitMult=%.2f Final=%.2f"),
		*Hit.BoneName.ToString(), *GetNameSafe(Hit.GetComponent()),
		RawDamage, TargetDefense, PartMult, FinalDamage);

	// GE 애셋의 Modifier 가 이태그로 값을 받는다.
	Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Damage, -FinalDamage);

	AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	// 타격 연출. 큐를 쏘는 책임이 맞는 쪽에 있다.
	//
	// 왜 여기로 옮겼나: 플레이어는 막기·패링에 따라 큐가 갈리는데, 그 판단은
	// 맞는 쪽만 할 수 있다. 때리는 쪽(MeleeTraceComponent)에서 쏘던 것을
	// 양쪽 다 맞는 쪽으로 통일했다. 적은 막기가 없어서 성질 판정만 한다.
	//
	// 쳐낼 조건: 적에게도 방어가 생기면 플레이어처럼 결과 우선 분기를 넣는다.
	HitCuePlay::PlayImpact(InstigatorActor, this, Hit, FinalDamage);

	// 피격 반응
	if (!AbilitySystemComp->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
	{
		// 일반 타격은 피격 모션만 준다 (Ability.Hit — 목 슬롯 몽타주. 공격 · 이동을 끊지 않는다).
		// 공격을 끊는 진짜 경직(Ability.HitReact)은 패링만 준다 (AHBPlayerCharacter::ApplyStaggerTo).
		// 왜: 플레이어 연계 공격이 빨라서, 매 타격 경직이면 적이 아무것도 못 한다.
		// Ability.Hit GA 가 없는 적은 모션 없이 소리 · 이펙트만 난다.
		//
		// 쳐낼 조건: 특정 스킬이 경직을 줘야 하면 그 스킬에서 Ability.HitReact 를 따로 부른다.
		AbilitySystemComp->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag::TAG_Ability_Hit));

		// 때린상대의 위치로 가기위한 위치를 감각관리컴포넌트에 건네준다.
		// 폰이 아닌 것(트랩 등)에 맞았으면 수색하지 않는다. 그 자리를 뒤지러 가 봐야 아무도 없다.
		if (Cast<APawn>(InstigatorActor))
		{
			if (AEnemyAIControllerBase* AIC = Cast<AEnemyAIControllerBase>(GetController()))
			{
				if (UAlertComponent* Alert = AIC->GetAlertComponent())
				{
					Alert->DamageSensed(InstigatorActor->GetActorLocation());
				}
			}

			// 같은 경계 팀원도 때린 사람 위치로 수색하러 온다.
			AlertTeamMates(InstigatorActor);
		}
	}

}

void AEnemyCharacterBase::AlertTeamMates(AActor* Player)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AlertTeam.IsNone())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 연타 때마다 훑지 않는다. 간격이 지나면 그때의 플레이어 위치로 다시 알린다.
	const double Now = World->GetTimeSeconds();
	if (bTeamAlertSent)
	{
		if (Now - LastTeamAlertTime < TeamAlertInterval)
		{
			return;
		}
	}
	bTeamAlertSent = true;
	LastTeamAlertTime = Now;

	const ULevel* MyLevel = GetLevel();
	int32 AlertedCount = 0;

	for (TActorIterator<AEnemyCharacterBase> It(World); It; ++It)
	{
		AEnemyCharacterBase* Mate = *It;
		if (Mate == this)
		{
			continue;
		}

		if (Mate->AlertTeam != AlertTeam)
		{
			continue;
		}

		// 같은 방 모듈이 여러 번 나와도 방끼리는 섞지 않는다.
		if (Mate->GetLevel() != MyLevel)
		{
			continue;
		}

		if (Mate->IsDead())
		{
			continue;
		}

		// 아직 활성화 전(컨트롤러 없음)인 적은 건너뛴다.
		AEnemyAIControllerBase* MateAI = Cast<AEnemyAIControllerBase>(Mate->GetController());
		if (!MateAI)
		{
			continue;
		}

		MateAI->OnTeamAlerted(Player);
		++AlertedCount;
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Team] %s (%s) → 팀원 %d 명에게 알림"),
		*GetName(), *AlertTeam.ToString(), AlertedCount);
}

void AEnemyCharacterBase::SetMoveSpeed(bool bRun)
{
	// 이동속도 변경은 다른 하수로 이관됨
	//if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	//{
	//	Movement->MaxWalkSpeed = bRun ? RunSpeed : WalkSpeed;
	//}

	bRunning = bRun;
	RefreshMoveSpeed();
}

void AEnemyCharacterBase::SetLootRow(const FDataTableRowHandle& InRow)
{
	if (!LootComponent)
	{
		return;
	}

	LootComponent->SetLootRow(InRow);
}

void AEnemyCharacterBase::BecomeDead()
{
	// 이미 죽거나 서버가아니면 실행x
	if (!HasAuthority() || bDead)
	{
		return;
	}

	bDead = true;

	// 시체는 더 이상 움직이지 않도록 막아준다.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}

	// Moveto로 움직이고 잇엇다면 멈춘다.
	// 이동은 지금 끊어야 한다. 시체가 미끄러지면 안 된다.
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	// AI 자체는 잠시 뒤에 끊는다.
	//
	// 왜 즉시가 아닌가: 이 시점에 사망 몽타주가 막 시작했고, 죽기 직전에 쓰던
	// 공격 몽타주도 아직 돌고 있을 수 있다. 그 몽타주의 노티파이가 컨트롤러를
	// 거쳐 타겟을 조회하므로(EnemyProjGABase 등) 지금 끊으면 널을 잡는다.
	//
	// 왜 끊긴 해야 하나: 시체는 되살아나지 않는데 StateTree 와 지각이 계속 돈다.
	// 방마다 5마리씩 죽어 쌓이면 그대로 서버 부하가 된다.
	//
	// 쳐낼 조건: 부활이나 시체 조종 기믹이 생기면 빙의만 풀고 컨트롤러는 남겨야 한다.
	GetWorldTimerManager().SetTimer(ControllerCleanupTimer, this, &AEnemyCharacterBase::ShutdownAI, ControllerCleanupDelay, false);

	// 임시 진단. 시체가 왜 움직이는지 찾으면 지운다
	GetWorldTimerManager().SetTimer(DeadDebugTimer, this, &AEnemyCharacterBase::DebugDeadTick, 1.f, true);

	// 파밍될 아이템들을 인벤토리에 생성한다.
	// 소환체는 만들지 않는다. 만들면 보스를 안 잡고 소환만 반복하는 파밍이 생긴다.
	if (LootComponent && !bSummoned)
	{
		LootComponent->GenerateLoot();
	}

	// 보상처리
	GiveExpToKiller();

	// 서버에서도 직접불러준다. Multicast로 해보려햇는데 반응속도문제땜시...
	OnRep_Dead();

	// 소환 명부 같은 구독자에게 알린다.
	// OnRep_Dead 뒤인 이유는 구독자가 이 시체의 최종 상태를 보게 하려는 것이다.
	OnDied.Broadcast(this);

	// 소환체는 시체를 남기지 않는다
	if (bSummoned)
	{
		GetWorldTimerManager().SetTimer(DespawnTimer, this, &AEnemyCharacterBase::DespawnSelf, DespawnDelay, false);
	}
}

// Called when the game starts or when spawned
void AEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서는 해당 옵션을 킨다
	// 엔진 자체에서 최적화를 위해 뼈대의 세세한 움직임을 기록하지않은데 현재 뼈대의 위치를 알필요가있기때문에 강제로 켜준다.
	// GetSocketLocation() = 액터의 월드 트랜스폼 * 본 트랜스폼(여기가 갱신 안 됨) * 소켓 상대 오프셋
	if (HasAuthority())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		// ASC의 구독도 서버에서만 해준다.
		if (AbilitySystemComp)
		{
			// 이동속도 배율이 변경 시 호출
			AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(UEnemyAttributeSetBase::GetMoveSpeedRateAttribute()).AddUObject(this, &AEnemyCharacterBase::OnMoveSpeedRateChanged);
		}
	}
	else
	{
		GetMesh()->bEnableUpdateRateOptimizations = true;
	}

	UE_LOG(LogEnemyAI, Warning, TEXT("[Net] Mesh=%d Capsule=%d"),
		GetMesh()->GetIsReplicated() ? 1 : 0,
		GetCapsuleComponent()->GetIsReplicated() ? 1 : 0);
	
	// 이동속도 초기 설정은 false
	SetMoveSpeed(false);
}

void AEnemyCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (!AbilitySystemComp)
	{
		return;
	}

	if (HasAuthority())
	{
		AbilitySystemComp->AddLooseGameplayTag(TeamTag::TAG_Team_Enemy);
	}

	// 인자가 둘 다 this 인 이유: Owner와 Avatar가 AI 에서는 같은 액터다.
	AbilitySystemComp->InitAbilityActorInfo(this, this);

	UE_LOG(LogEnemyAI, Log, TEXT("[GAS] %s ASC 초기화 완료"), *GetName());
}

UAbilitySystemComponent* AEnemyCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

bool AEnemyCharacterBase::CanInteract_Implementation(AActor* Interactor) const
{
	// 소환체는 열 수 없다. 시체는 보이되 F 가 뜨지 않는다
	if (bSummoned)
	{
		return false;
	}

	// 상호작용 조건 : 죽었으며 인벤이 존재해야한다.
	return Interactor != nullptr && bDead && LootComponent != nullptr;
}

void AEnemyCharacterBase::Interact_Implementation(AActor* Interactor)
{
	// 서버권한에서만 상호작용 가능
	if (HasAuthority() && LootComponent)
	{
		// 상호작용을 한 플레이어에게 루팅될 인벤토리를 연다
		LootComponent->HandleInteract(Interactor);
	}
}

FText AEnemyCharacterBase::GetInteractPrompt_Implementation() const
{
	return LootComponent ? LootComponent->GetPrompt() : FText::GetEmpty();
}

void AEnemyCharacterBase::MarkAsSummoned(float InDespawnDelay)
{
	bSummoned = true;
	DespawnDelay = FMath::Max(0.f, InDespawnDelay);
}

void AEnemyCharacterBase::StartRagdoll()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// 프로파일을 통째로 갈지 않고 응답을 직접 적는다.
	//
	// 왜: "Ragdoll" 프로파일을 쓰면 OnRep_Dead 가 걸어둔 Visibility=Block 이 덮여
	// 시체를 조준해도 루팅이 안 잡힌다. 그리고 공격 채널을 막게 되면
	// SphereTraceMulti 가 첫 블로킹 히트에서 멈추므로 시체가 방패가 되어
	// 뒤에 있는 적을 못 때린다.
	//
	// 쳐낼 조건: 시체를 밀치거나 타격하는 연출이 생기면 그 채널만 열어준다.
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionObjectType(ECC_PhysicsBody);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);    // 바닥에 눕는다
	MeshComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);     // 루팅 트레이스

	MeshComp->SetSimulatePhysics(true);
	MeshComp->WakeAllRigidBodies();
	MeshComp->SetPhysicsBlendWeight(1.f);

	// ----- 임시 진단 -----
	// 사지가 안 움직이는 원인을 찾기 위한 것. 잡으면 지운다.
	//
	// SetSimulatePhysics 는 PhysicsType 이 Default 인 바디만 켜준다.
	// Kinematic 으로 박힌 바디는 애니메이션 자세를 그대로 붙들고 있는다.
	UPhysicsAsset* PhysAsset = MeshComp->GetPhysicsAsset();

	UE_LOG(LogEnemyAI, Warning, TEXT("[Ragdoll] %s  RootSim=%d  PhysAsset=%s  Auth=%d"),
		*GetName(),
		MeshComp->IsSimulatingPhysics() ? 1 : 0,
		*GetNameSafe(PhysAsset),
		HasAuthority() ? 1 : 0);

	if (!PhysAsset)
	{
		return;
	}

	// Type: 0=Default 1=Kinematic 2=Simulated
	for (int32 i = 0; i < PhysAsset->SkeletalBodySetups.Num(); ++i)
	{
		USkeletalBodySetup* Setup = PhysAsset->SkeletalBodySetups[i];
		if (!Setup)
		{
			continue;
		}

		FBodyInstance* Inst = MeshComp->Bodies.IsValidIndex(i) ? MeshComp->Bodies[i] : nullptr;

		UE_LOG(LogEnemyAI, Warning, TEXT("[Ragdoll]   %-24s Type=%d  Sim=%d"),
			*Setup->BoneName.ToString(),
			static_cast<int32>(Setup->PhysicsType),
			Inst ? (Inst->bSimulatePhysics ? 1 : 0) : -1);
	}
}

void AEnemyCharacterBase::DespawnSelf()
{
	// 컨트롤러 정리 타이머보다 먼저 사라지는 게 보통이라 여기서도 치운다.
	// 이미 치워졌으면 컨트롤러가 없으니 아무 일도 일어나지 않는다.
	if (AController* C = GetController())
	{
		C->Destroy();
	}

	Destroy();
}

void AEnemyCharacterBase::DebugDeadTick()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	AAIController* AIC = Cast<AAIController>(GetController());
	UPathFollowingComponent* PF = AIC ? AIC->GetPathFollowingComponent() : nullptr;

	// PF: 0=Idle 1=Waiting 2=Paused 3=Moving
	UE_LOG(LogEnemyAI, Warning,
		TEXT("[DeadTick] %s  Mode=%d  Vel=%s  PF=%d  Focus=%s  DeadTag=%d  Orient=%d  Desired=%d"),
		*GetName(),
		Move ? static_cast<int32>(Move->MovementMode.GetValue()) : -1,
		Move ? *Move->Velocity.ToCompactString() : TEXT("-"),
		PF ? static_cast<int32>(PF->GetStatus()) : -1,
		*GetNameSafe(AIC ? AIC->GetFocusActor() : nullptr),
		(AbilitySystemComp && AbilitySystemComp->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead)) ? 1 : 0,
		Move ? (Move->bOrientRotationToMovement ? 1 : 0) : -1,
		Move ? (Move->bUseControllerDesiredRotation ? 1 : 0) : -1);
}

void AEnemyCharacterBase::ShutdownAI()
{
	// Destroy 가 내부에서 UnPossess 까지 한다.
	// 이미 치워진 뒤라면 컨트롤러가 없으니 아무 일도 일어나지 않는다.
	if (AController* C = GetController())
	{
		C->Destroy();
	}

	// 임시 진단 타이머도 같이 끈다
	GetWorldTimerManager().ClearTimer(DeadDebugTimer);

	UE_LOG(LogEnemyAI, Log, TEXT("[AI] %s 컨트롤러 정리"), *GetName());
}

void AEnemyCharacterBase::OnRep_Dead()
{
	// 스켈레톤 메쉬를 블록으로 변경
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// 캡슐콜리전을....쓸필요가있나? 무시하자
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
}

void AEnemyCharacterBase::RefreshMoveSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// 걷거나 뛰는 속도에서 이동속도배율을 곱한다.
	float Final = (bRunning ? RunSpeed : WalkSpeed) * CachedSpeedRate;

	Movement->MaxWalkSpeed = Final;
}

void AEnemyCharacterBase::OnMoveSpeedRateChanged(const FOnAttributeChangeData& Data)
{
	// 해당 데이터 값은 이동속도배율
	CachedSpeedRate = Data.NewValue;
	RefreshMoveSpeed();
}

void AEnemyCharacterBase::GiveExpToKiller()
{
	if (ExpReward <= 0.f)
	{
		return;
	}

	// 막타를 낸 액터. 이미 파괴됐거나 플레이어가 아니면 거절
	AHBPlayerCharacter* Killer = Cast<AHBPlayerCharacter>(LastDamageInstigator.Get());
	if (!Killer)
	{
		return;
	}

	AHB_PlayerState* KillerState = Killer->GetPlayerState<AHB_PlayerState>();
	if (!KillerState)
	{
		return;
	}

	KillerState->AddExperience(ExpReward);

	UE_LOG(LogEnemyAI, Log, TEXT("[Exp] %s → %s  %.0f"), *GetName(), *GetNameSafe(Killer), ExpReward);
}

// Called every frame
void AEnemyCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

