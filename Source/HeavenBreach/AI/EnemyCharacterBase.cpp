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
#include "AI/EnemyAIControllerBase.h"
#include "AI/AlertComponent.h"

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


	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));

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

void AEnemyCharacterBase::ApplyMeleeHit_Implementation(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit)
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

	// GE 애셋의 Modifier 가 이태그로 값을 받는다.
	Spec.Data->SetSetByCallerMagnitude(AbilityTag::TAG_SetByCaller_Damage, -RawDamage);

	AbilitySystemComp->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

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
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = bRun ? RunSpeed : WalkSpeed;
	}
}

// Called when the game starts or when spawned
void AEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// 서버에서는 해당 옵션을 킨다
	// 엔진 자체에서 최적화를 위해 뼈대의 세세한 움직임을 기록하지않은데 현재 뼈대의 위치를 알필요가있기때문에 강제로 켜준다.
	// GetSocketLocation() = 액터의 월드 트랜스폼 * 본 트랜스폼(여기가 갱신 안 됨) * 소켓 상대 오프셋
	if(HasAuthority())
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	
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

	// 인자가 둘 다 this 인 이유: Owner와 Avatar가 AI 에서는 같은 액터다.
	AbilitySystemComp->InitAbilityActorInfo(this, this);

	UE_LOG(LogEnemyAI, Log, TEXT("[GAS] %s ASC 초기화 완료"), *GetName());
}

UAbilitySystemComponent* AEnemyCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

// Called every frame
void AEnemyCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

