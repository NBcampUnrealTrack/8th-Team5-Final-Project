// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnemyAIControllerBase.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "AI/AlertComponent.h"
#include "AI/Enum/BBKey.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/EnemyCharacterBase.h"
#include "AI/DataRow/EnemyArchetypeData.h"

DEFINE_LOG_CATEGORY(LogEnemyAI);

AEnemyAIControllerBase::AEnemyAIControllerBase()
{
	// 디버그 표기를 위해 true로 변경.
	PrimaryActorTick.bCanEverTick = true;

	// 감각 등록
	// AIControllor자체에서 감각 컴포넌트가 있다. 거기에 등록
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	SetPerceptionComponent(*PerceptionComp);

	// 시야 등록
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());

	AlertComponent = CreateDefaultSubobject<UAlertComponent>(TEXT("AlertComponent"));
}

void AEnemyAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 캐릭터가 들고있는 데이터를 가져온다.
	AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(InPawn);
	const UEnemyArchetypeData* Data = Enemy ? Enemy->GetArchetypeData() : nullptr;

	// 폰이 붙은 뒤여야 감각의 기준이 정해진다.
	// 데이터 에셋 기반으로 가게되는데 여기서 시야 거리까지 넣게된다면 여기서 해야한다.
	ApplyArchetype(Enemy);

	if (PerceptionComp)
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIControllerBase::OnTargetPerceptionUpdated);
	}

	// RunBehaviorTree() 가 트리 애셋에 지정된 블랙보드로 블랙보드를 초기화한다.
	// 엔진 에디터에서 BehaviorTree에 블랙보드가 지정되어야한다.
	if (BehaviorTreeAsset && RunBehaviorTree(BehaviorTreeAsset))
	{
		if (UBlackboardComponent* BB = GetBlackboardComponent())
		{
			// 배회의 중심. 스폰 위치에 앵커를 박는다.
			BB->SetValueAsVector(FBBKey::HomeLocation, InPawn->GetActorLocation());
			// 배회의 중심으로 부터 최대 이동거리
			BB->SetValueAsFloat(FBBKey::MaxMoveDistance,Data ? Data->GetStats().MaxMoveDistance : 1000.f);
		}

		if (AlertComponent)
		{
			// AddUObject 는 중복을 검사하지 않는다. 컨트롤러가 재사용되어 다시 OnPossess 될 때 이중 바인딩되지 않게 먼저 정리한다.
			AlertComponent->OnAlertUpdated.RemoveAll(this);
			AlertComponent->OnAlertUpdated.AddUObject(this, &AEnemyAIControllerBase::HandleAlertUpdated);

			// 트리 시작 전에 발생한 상태 변화를 한 번 밀어준다.
			HandleAlertUpdated();
		}
	}
	else
	{
		UE_LOG(LogEnemyAI, Error, TEXT("[AI] %s 트리 실행 실패 — BehaviorTreeAsset 확인"),
			*GetNameSafe(InPawn));
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[AI] %s 준비 완료"), *GetNameSafe(InPawn));
}

void AEnemyAIControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 셰이핑 빌드에서 이 블록이 통째로 사라진다. 나중에 지우러 다니지 않기 위한 습관.
#if ENABLE_DRAW_DEBUG
	if (!bDrawAlertStateDebug || !AlertComponent)
	{
		return;
	}

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	const EAlertState CurrentState = AlertComponent->GetState();

	// 색을 다르게 두면 문자를 읽지 않고도 상태 전이가 눈에 들어온다.
	FColor Color = FColor::White;
	switch (CurrentState)
	{
	case EAlertState::Idle:    Color = FColor::Silver; break;
	case EAlertState::Alerted: Color = FColor::Yellow; break;
	case EAlertState::Engaged: Color = FColor::Red;    break;
	case EAlertState::Lost:    Color = FColor::Orange; break;
	case EAlertState::Dead:    Color = FColor::Black;  break;
	}

	// TestBaseActor 를 넘기면 첫 인자가 "액터 기준 상대 오프셋"이 되어 문자가 폰을 따라간다.
	// Duration 0 + 매 틱: 같은 액터의 기존 문자를 덮어쓰므로 누적되지 않는다.
	DrawDebugString(GetWorld(), FVector(0.f, 0.f, 100.f), LexToString(CurrentState),
		MyPawn, Color, /*Duration*/ 0.f, /*bDrawShadow*/ true);
#endif
}

void AEnemyAIControllerBase::ApplyArchetype(AEnemyCharacterBase* Char)
{
	const UEnemyArchetypeData* Data = Char ? Char->GetArchetypeData() : nullptr;

	if (!Data)
	{
		// 애셋 미지정. 각 클래스의 UPROPERTY 기본값으로 동작한다.
		// Warning 으로 남기는 이유: 조용히 넘기면 "왜 값을 바꿨는데 안 먹지"를
		// 나중에 추적하게 된다.
		UE_LOG(LogEnemyAI, Warning, TEXT("[AI] %s ArchetypeData 미지정 — 폴백 값 사용"),*GetNameSafe(GetPawn()));
		ApplySenses(nullptr);
		return;
	}

	const FEnemyStatRow& S = Data->GetStats();

	// 어떤 값이 어디로 가는지의 목록
	ApplySenses(&S);

	if (AlertComponent)
	{
		AlertComponent->SetLostMemoryDuration(S.LostMemoryDuration);
	}

	if (Char)
	{
		Char->SetWalkSpeed(S.WalkSpeed);
		Char->SetRunSpeed(S.RunSpeed);
		Char->SetAccRadius(S.AcceptanceRadius);
		//Enemy->SetMovementStats(S.WalkSpeed, S.RunSpeed, S.AcceptanceRadius);
	}
}

bool AEnemyAIControllerBase::IsValidTarget(const AActor* Actor) const
{
	// 유효성 검사
	if (!Actor)
	{
		return false;
	}

	// IsPlayerControlled() 는 AActor로는 알수 없다. APawn으로 캐스팅 시켜 확인하도록한다.
	const APawn* AsPawn = Cast<const APawn>(Actor);

	return AsPawn && AsPawn->IsPlayerControlled();
}

void AEnemyAIControllerBase::ApplySenses(const FEnemyStatRow* Stats)
{
	//감각이 존재해야 진행가능
	if (!PerceptionComp || !SightConfig)
	{
		return;
	}

	// Stats 가 있으면 그 값, 없으면 이 클래스의 기본값 사용.
	const float UseSightRadius = Stats ? Stats->SightRadius : SightRadius;
	const float UseLoseRadius = Stats ? Stats->LoseSightRadius : LoseSightRadius;
	const float UseVisionAngle = Stats ? Stats->PeripheralVisionAngle : PeripheralVisionAngle;
	const float UseMaxAge = Stats ? Stats->SightMaxAge : SightMaxAge;

	// 시야 범위
	SightConfig->SightRadius = UseSightRadius;

	// 대상을 시야에서 잃기 까지 범위
	// 적어도 LoseSightRadius가 SightRadius보다는 커야하니 Max로 작성
	SightConfig->LoseSightRadius = FMath::Max(UseLoseRadius, UseSightRadius + 100.f);

	// 시야 각도. 반지름이니 실제범위는 2배다.
	SightConfig->PeripheralVisionAngleDegrees = UseVisionAngle;
	// 시야에 들어온대상이 벗어날 시 정보를 잃어버리기까지 수명 시간
	SightConfig->SetMaxAge(UseMaxAge);

	// 뒤에서 가까이 붙으면 자동 감지되는 기능을 끈다.
	// 켜두면 시야각 테스트가 무의미해져서 1번 검증이 흐려진다.
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;

	// 팀구별 없이 중립으로만 탐지
	SightConfig->DetectionByAffiliation.bDetectEnemies = false;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	// 이미등록해있어도 옵션값이 수정 이후 다시 등록안해주면 기본값으로 되버림...
	PerceptionComp->ConfigureSense(*SightConfig);

	// 감각정보 수정을 알림
	PerceptionComp->RequestStimuliListenerUpdate();
}

void AEnemyAIControllerBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!AlertComponent)
	{
		return;
	}

	// 청각을 추가할 때 여기
	// const FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();
	// if (Stimulus.Type != SightID) { 청각 처리  return; }

	// AlertComponent 에 닿기 전에 차단
	if (!IsValidTarget(Actor))
	{
		return;
	}

	// 시야내 들어온게 플레이어가 아닌거라도 알람컴포넌트 내부에서 차단
	// 시야내 누군가 들어옴
	if (Stimulus.WasSuccessfullySensed())
	{
		AlertComponent->SightSensed(Actor);
	}
	// 시야내에 있던 누군가가 빠져나감
	else
	{
		// StimulusLocation = 마지막으로 성공 감지된 위치
		AlertComponent->SightLost(Actor, Stimulus.StimulusLocation);
	}
}

void AEnemyAIControllerBase::HandleAlertUpdated()
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	// 당연하지만 블랙보드와 알람컴포넌트는 필수
	// 애초에 이 함수에 도달하려면 알람컴포넌트에 이벤트 구독
	if (!BB || !AlertComponent)
	{
		return;
	}

	// AlertState 를 쓰는 순간 루트 데코레이터의 Observer Abort 가 발동해 브랜치가 즉시 전환된다. 그래서 다른 값을 먼저 채운다.
	BB->SetValueAsObject(FBBKey::TargetActor, AlertComponent->GetCurrentTarget());
	BB->SetValueAsVector(FBBKey::LastKnownLocation, AlertComponent->GetLastKnownLocation());

	// 비헤이비어 트리 전환
	BB->SetValueAsEnum(FBBKey::AlertState, static_cast<uint8>(AlertComponent->GetState()));
}
