// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacterBase.generated.h"

class UEnemyArchetypeData;

UCLASS()
class HEAVENBREACH_API AEnemyCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyCharacterBase();

	// true = 달리기, false = 걷기
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetMoveSpeed(bool bRun);

	// setter
	void SetWalkSpeed(float InWalkSpeed) { WalkSpeed = InWalkSpeed; }
	void SetRunSpeed(float InRunSpeed) { RunSpeed = InRunSpeed; }
	void SetAccRadius(float InAcceptanceRadius) { AcceptanceRadius = InAcceptanceRadius; }

	// getter
	const UEnemyArchetypeData* GetArchetypeData() { return ArchetypeData; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// character BP에서 데이터 관리
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UEnemyArchetypeData> ArchetypeData;

	// 걷는 속도
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 200.f;
	// 달리는 속도
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 600.f;
	// 목적지까지 도달 허용 범위
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 100.f;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
