// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnemyCharacterBase.h"
#include "AI/EnemyAIControllerBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"

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

// Called every frame
void AEnemyCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AEnemyCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

