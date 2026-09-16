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
#include "Components/SkeletalMeshComponent.h"
#include "Core/Tag/Team/HB_TeamTag.h"
#include "Core/Tag/Cue/HB_CueTag.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"

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

	const float TargetDefense = ArchetypeData
		? ArchetypeData->GetStats().Defense
		: 0.f;
	const float FinalDamage = HBDamage::CalculateFinalDamage(
		RawDamage, TargetDefense, PartMult);

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
		// 사망이 아닌 상태라면 피격 어빌리티를 발동 시킨다.
		AbilitySystemComp->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag::TAG_Ability_HitReact));

		// 때린상대의 위치로 가기위한 위치를 감각관리컴포넌트에 건네준다.
		if (InstigatorActor)
		{
			if (AEnemyAIControllerBase* AIC = Cast<AEnemyAIControllerBase>(GetController()))
			{
				if (UAlertComponent* Alert = AIC->GetAlertComponent())
				{
					Alert->DamageSensed(InstigatorActor->GetActorLocation());
				}
			}
		}
	}
	
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
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	// 파밍될 아이템들을 인벤토리에 생성한다.
	if (LootComponent)
	{
		LootComponent->GenerateLoot();
	}

	// 보상처리
	GiveExpToKiller();

	// 서버에서도 직접불러준다. Multicast로 해보려햇는데 반응속도문제땜시...
	OnRep_Dead();
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

