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
 * 후딜 구간에 무엇을 할지. PickTacticalMove 가 정하고 GetStrafePoint 가 읽는다.
 */
UENUM(BlueprintType)
enum class EEnemyTactic : uint8
{
	Orbit    UMETA(DisplayName = "선회"),
	Backstep UMETA(DisplayName = "후퇴"),
};

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
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "AI|Sleep")
	bool IsSleeping() const { return bSleeping; }

	void WakeUp();


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

	UFUNCTION(BlueprintPure, Category = "AI")
	float GetTurnSpeed() const;

	UFUNCTION(BlueprintPure, Category = "AI")
	bool HasRangedAttack() const;

	UFUNCTION(BlueprintPure, Category = "AI")
	bool ShouldStayHome() const;

	UFUNCTION(BlueprintPure, Category = "AI")
	bool ShouldUseSpacing() const;

	// 타겟과의 평면 거리. 타겟이 없으면 -1.
	// 전술 선택이 거리를 보게 되면서 필요해졌다
	UFUNCTION(BlueprintPure, Category = "AI")
	float GetDistanceToTarget() const;

	// 지금 정해진 전술에 맞는 다음 이동 지점.
	// StateTree 측에서 짧은 딜레이(0.3초 정도)로 반복 호출해 MoveTo 로 이동할 것.
	// 길게 잡으면 도착해서 멈췄다 다시 출발해 뚝뚝 끊긴다.
	UFUNCTION(BlueprintPure, Category = "AI")
	FVector GetStrafePoint() const;

	// 이동 중에 타겟을 바라보게 한다. 이동 태스크가 EnterState/ExitState 에서 부른다.
	//
	// 왜 회전 태스크를 안 쓰나: SetActorRotation 은 이동 중에 무브먼트의 회전과
	// 매 틱 싸우고, 이겨도 몸이 항상 속도 방향을 향하게 되어 AnimBP 의 Direction 이
	// 0 으로 고정된다. 그러면 좌우 이동 블렌드스페이스가 영영 안 나온다.
	// 포커스로 돌리면 이동과 회전이 한 시스템 안에서 정리되고 Direction 이 산다.
	//
	// 쳐낼 조건: 좌우 이동 애니메이션을 안 쓰기로 하면 이 함수는 필요 없다.
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetStrafeFacing(bool bEnable);

	// 전술을 새로 고른다. 후딜 구간에 들어올 때 한 번 부른다.
	// 지점 갱신(GetStrafePoint)보다 훨씬 긴 주기로 불러야 한다.
	// 같은 주기로 부르면 후퇴와 선회가 매번 뒤집혀 제자리에서 떤다.
	UFUNCTION(BlueprintCallable, Category = "AI")
	void PickTacticalMove();

	// 스탯의 RetargetChance 로 굴려서 타겟을 다시 고른다.
	// 공격이 정상적으로 끝난 시점에 한 번만 부를 것. (ST_Attack 의 Retarget 상태)
	//
	// 왜 전술 갱신에서 분리했나: 2초 주기에 얹어두니 후딜 한 번에 여러 번 굴려서
	// 타겟이 두 사람 사이를 왔다갔다 했고, 선회 중심이 그때마다 순간이동해
	// 이동이 망가졌다. 호출 시점을 한 곳으로 좁힌다.
	//
	// 왜 확률을 트리 조건이 아니라 여기서 굴리나: StateTree 조건은 선택 · 재선택 ·
	// 활성화 검사에서 여러 번 평가될 수 있다. 조건 안에서 굴리면 한 공격에
	// 두세 번 굴려져 같은 문제가 재현된다.
	//
	// 쳐낼 조건: 어그로 수치 시스템이 생기면 이 함수는 그쪽으로 흡수된다.
	UFUNCTION(BlueprintCallable, Category = "AI")
	void TryRetarget();

	// 같은 경계 팀원이 맞았거나 플레이어를 발견했다. 맞은 본인과 같은 반응으로 그 위치를 수색하러 간다.
	// AEnemyCharacterBase::AlertTeamMates 가 부른다.
	void OnTeamAlerted(AActor* Player);

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

	// 네비 투사 허용 오차. 좁은 방이면 줄인다
	UPROPERTY(EditDefaultsOnly, Category = "AI|Tactics")
	FVector StrafeNavExtent = FVector(150.f, 150.f, 300.f);

	// 계산한 지점을 네비메시 위로 끌어올린다. 못 올리면 false.
	// 이게 없으면 벽 안이나 절벽 밖이 나와 MoveTo 가 즉시 실패한다
	bool ProjectStrafePoint(const FVector& RawPoint, FVector& OutPoint) const;

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
	// 기본은 꺼 둔다. 플레이 중 화면을 가린다는 팀 요청.
	// 경계 상태 전이를 확인할 때만 BP 의 Class Defaults 에서 켠다.
	UPROPERTY(EditDefaultsOnly, Category = "AI|Debug")
	bool bDrawAlertStateDebug = false;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sleep")
	bool bAllowSleep = true;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sleep", meta = (ClampMin = "0"))
	float WakeDistance = 3500.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sleep", meta = (ClampMin = "0"))
	float SleepDistance = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sleep", meta = (ClampMin = "0.1"))
	float SleepCheckInterval = 1.f;



private:
	// 선회 방향. 개체마다 고정한다. 매번 고르면 좌우로 덜덜 떤다
	bool bOrbitClockwise = true;

	// 지금 무슨 전술인가. PickTacticalMove 가 정하고 GetStrafePoint 가 읽는다
	EEnemyTactic CurrentTactic = EEnemyTactic::Orbit;

	// 선회 조준 중인가. 중복 진입 시 원래 값을 덮어쓰지 않으려고 둔다
	bool bStrafeFacing = false;

	// 켜기 전의 회전 속도. 끌 때 그대로 되돌린다
	FRotator SavedRotationRate = FRotator(0.f, 360.f, 0.f);

	void EvaluateSleep();
	void EnterSleep();
	void RestoreFromSleep(bool bStartLogic);
	void HandleAlertForSleep();
	float GetNearestPlayerDistSq() const;

	bool bSleeping = false;
	FTimerHandle SleepTimerHandle;
	FDelegateHandle AlertSleepHandle;
};
