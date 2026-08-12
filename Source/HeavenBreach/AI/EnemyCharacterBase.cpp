// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnemyCharacterBase.h"
#include "AI/EnemyAIControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "AI/GAS/EnemyAttributeSetBase.h"
#include "AI/Enum/EAlertState.h"

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

