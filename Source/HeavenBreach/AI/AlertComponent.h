// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/Enum/EAlertState.h"
#include "AlertComponent.generated.h"

// 경계 정보에 따른 상태 변화시 발동됨
DECLARE_MULTICAST_DELEGATE(FOnAlertUpdated);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HEAVENBREACH_API UAlertComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAlertComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 시야 발견
	// 되도록 유효성을 위해 Actor는 IsVaild로 유효성 검사를 해둘것
	void SightSensed(AActor* Actor);
	// 시야 놓침
	// 되도록 유효성을 위해 Actor는 IsVaild로 유효성 검사를 해둘것
	void SightLost(AActor* Actor, const FVector& LastSeenLocation);

	// lost상태 기간 설정
	void SetLostMemoryDuration(float Duration) { LostMemoryDuration = FMath::Max(0.f, Duration); }

	// 구독자는 이걸 받아 게터로 현재 값을 읽는다.
	FOnAlertUpdated OnAlertUpdated;

	// getter
	EAlertState GetState() const { return State; }
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }
	FVector GetLastKnownLocation() const { return LastKnownLocation; }

protected:
	// Lost상태 유지 시간
	UPROPERTY(EditDefaultsOnly, Category = "Alert")
	float LostMemoryDuration = 5.f;

	// 상태 변경
	void SetState(EAlertState NewState);

	// 타겟 선택
	AActor* PickTarget() const;

	// 추가 유효성 확인을 위한 함수
	// 만일 시야처리까지 시간이 오래걸렷는데 그 사이에 삭제된 액터라면 배열에서 지워야함.
	void PruneSensedTargets();

	// 현재 지정된 타겟
	AActor* GetSensingActor() const;

	// 타겟을 잃을때 하는것
	void OnLostMemoryExpired();

private:
	// 큰로직 구별을 담당을 이곳에 뺀이유 : 함부로 건들지 말라고
	// 기본적인 큰 틀은 여기서 벗어나지 않도록 막음
	EAlertState State = EAlertState::Idle;

	// 사망한 플레이어를 참조할 수 있으니 약한 참조 사용
	TWeakObjectPtr<AActor> CurrentTarget;

	// 인식된 타겟들 배열. 나중에 우선순위든 뭐든해서 변경
	TArray<TWeakObjectPtr<AActor>> SensedTargets;

	// 마지막으로 기억한 시야까지 추적하게할거라면...
	FVector LastKnownLocation = FVector::ZeroVector;

	FTimerHandle LostMemoryTimer;
		
};
