// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIControllerBase.generated.h"


class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAlertComponent;
struct FAIStimulus;
class UEnemyArchetypeData;
struct FEnemyStatRow;
class AEnemyCharacterBase;
class UStateTreeAIComponent;

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AEnemyAIControllerBase : public AAIController
{
	GENERATED_BODY()
public:
	AEnemyAIControllerBase();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;


	// getter
	UFUNCTION(BlueprintPure, Category = "AI")
	UAlertComponent* GetAlertComponent() const { return AlertComponent; }

	UFUNCTION(BlueprintPure, Category = "AI")
	FVector GetHomeLocation() const { return HomeLocation; }

	UFUNCTION(BlueprintPure, Category = "AI")
	float GetMaxMoveDistance() const;

	UFUNCTION(BlueprintPure, Category = "AI")
	float GetAcceptanceRadius() const;

	UFUNCTION(BlueprintPure, Category = "AI")
	float GetAttackRange() const;

protected:
	// 데이터 에셋. 값을 복사하는 게 아니라 애셋 포인터만 들고 있는다.
	UPROPERTY()
	TObjectPtr<const UEnemyArchetypeData> CachedArchetype;

	// 컴포넌트, 캐릭터등등...흩어져 사용되는 변수들을 각각에게 뿌리는 함수 
	// 이 함수 하나를 읽으면 어떤 값이 어디로 가는지 보인다.
	void ApplyArchetype(AEnemyCharacterBase* Char);

	// 올바른 타겟인지 검증
	virtual bool IsValidTarget(const AActor* Actor) const;

	// 감각 컴포넌트에 감각들을 연결한다.
	// 혹시 나중에 시야나 청각이 필요하거나 없늕적이 나올지도모르니까
	void ApplySenses(const FEnemyStatRow* Stats);

	// 감각컴포넌트에서 누군가 발견시 델리게이트 구독시키는 함수
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	UFUNCTION()
	void OnTargetPerceptionForgotten(AActor* Actor);

	// 알람 컴포넌트 구독용
	void HandleAlertUpdated();

	UPROPERTY(VisibleAnywhere, Category = "AI")
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	UPROPERTY(VisibleAnywhere, Category = "AI")
	TObjectPtr<UAlertComponent> AlertComponent;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	// 청각은 필요없을것 같아 제외
	//UPROPERTY()
	//TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	// 비헤이비어 트리
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	// 스테이트 트리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	// -------------
	// 감각 스펙
	// -------------

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense")
	float SightRadius = 1500.f;

	// SightRadius 이하면 경계선에서 발각/해제
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense")
	float LoseSightRadius = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense")
	float PeripheralVisionAngle = 60.f;

	// 시야 수명
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense")
	float SightMaxAge = 5.f;

	// 청각용...이지만 아마안쓸거같음. 더미데이터
	//UPROPERTY(EditDefaultsOnly, Category = "AI|Sense")
	//bool bUseHearing = true;
	//
	//UPROPERTY(EditDefaultsOnly, Category = "AI|Sense")
	//float HearingRange = 1500.f;
	//
	//// 청각 수명
	//UPROPERTY(EditDefaultsOnly, Category = "AI|Sense")
	//float HearingMaxAge = 3.f;

	// 배회의 중심점. 애셋에 없는 런타임 값이라 여기 둔다
	FVector HomeLocation = FVector::ZeroVector;

	/** 머리 위 상태 표시. 패키징 빌드에서는 컴파일 자체가 제거됩니다. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Debug")
	bool bDrawAlertStateDebug = true;



private:

};
