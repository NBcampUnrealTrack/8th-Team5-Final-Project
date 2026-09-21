// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnemyAIControllerBase.h"

#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "AI/AlertComponent.h"
#include "AI/Enum/BBKey.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/EnemyCharacterBase.h"
#include "AI/DataRow/EnemyArchetypeData.h"
#include "Components/StateTreeAIComponent.h"
#include "AI/GAS/EnemyAbilitySystemComponent.h"
#include "core/Tag/Ability/HB_AbilityTag.h"

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

	// 스테이트 트리
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
}

void AEnemyAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 캐릭터가 들고있는 데이터를 가져온다.
	AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(InPawn);
	CachedArchetype = Enemy ? Enemy->GetArchetypeData() : nullptr;

	HomeLocation = InPawn->GetActorLocation();

	// 선회 방향을 개체마다 고정한다.
	// 호출할 때마다 고르면 지점을 갱신할 때마다 좌우가 뒤집혀 제자리에서 떤다.
	bOrbitClockwise = FMath::RandBool();

	// 폰이 붙은 뒤여야 감각의 기준이 정해진다.
	// 데이터 에셋 기반으로 가게되는데 여기서 시야 거리까지 넣게된다면 여기서 해야한다.
	ApplyArchetype(Enemy);

	if (PerceptionComp)
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIControllerBase::OnTargetPerceptionUpdated);
		PerceptionComp->OnTargetPerceptionForgotten.AddUniqueDynamic(this, &AEnemyAIControllerBase::OnTargetPerceptionForgotten);
	}

	// RunBehaviorTree() 가 트리 애셋에 지정된 블랙보드로 블랙보드를 초기화한다.
	// 엔진 에디터에서 BehaviorTree에 블랙보드가 지정되어야한다.
	//if (BehaviorTreeAsset && RunBehaviorTree(BehaviorTreeAsset))
	//{
	//	if (UBlackboardComponent* BB = GetBlackboardComponent())
	//	{
	//		// 배회의 중심. 스폰 위치에 앵커를 박는다.
	//		BB->SetValueAsVector(FBBKey::HomeLocation, InPawn->GetActorLocation());
	//		// 배회의 중심으로 부터 최대 이동거리
	//		BB->SetValueAsFloat(FBBKey::MaxMoveDistance,Data ? Data->GetStats().MaxMoveDistance : 1000.f);
	//	}
	//
	//	if (AlertComponent)
	//	{
	//		// AddUObject 는 중복을 검사하지 않는다. 컨트롤러가 재사용되어 다시 OnPossess 될 때 이중 바인딩되지 않게 먼저 정리한다.
	//		AlertComponent->OnAlertUpdated.RemoveAll(this);
	//		AlertComponent->OnAlertUpdated.AddUObject(this, &AEnemyAIControllerBase::HandleAlertUpdated);
	//
	//		// 트리 시작 전에 발생한 상태 변화를 한 번 밀어준다.
	//		HandleAlertUpdated();
	//	}
	//}
	//else
	//{
	//	UE_LOG(LogEnemyAI, Error, TEXT("[AI] %s 트리 실행 실패 — BehaviorTreeAsset 확인"),
	//		*GetNameSafe(InPawn));
	//}

	// 빙의가 끝난 뒤에 트리를 다시 시작한다.
	//
	// 왜 필요한가: 런타임에 SpawnActor 로 꺼낸 적은 StateTree 컴포넌트가 빙의보다
	// 먼저 StartTree 를 타서 컨텍스트 액터(EnemyCharacterBase)를 못 찾는다.
	// 그러면 "Context Requirements failed — Component tick is disabled" 가 되고
	// 스스로 다시 시도하지 않아 로직이 영영 안 돈다. 감각은 컨트롤러 소속이라
	// 멀쩡히 돌기 때문에 "플레이어는 보는데 안 움직이는" 상태가 된다.
	//
	// 레벨에 배치된 적은 순서가 맞아떨어져서 지금까지 드러나지 않았다.
	//
	// 쳐낼 조건: 스폰 경로가 전부 "빙의 후 컴포넌트 등록" 으로 바뀌면 필요 없다.
	if (StateTreeComponent)
	{
		StateTreeComponent->RestartLogic();
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
	}

	// TestBaseActor 를 넘기면 첫 인자가 "액터 기준 상대 오프셋"이 되어 문자가 폰을 따라간다.
	// Duration 0 + 매 틱: 같은 액터의 기존 문자를 덮어쓰므로 누적되지 않는다.
	DrawDebugString(GetWorld(), FVector(0.f, 0.f, 100.f), LexToString(CurrentState),
		MyPawn, Color, /*Duration*/ 0.f, /*bDrawShadow*/ true);
#endif
}

float AEnemyAIControllerBase::GetMaxMoveDistance() const
{
	// 저장하지 않고 애셋에서 읽는다.
	return CachedArchetype ? CachedArchetype->GetStats().MaxMoveDistance : 1000.f;
}

float AEnemyAIControllerBase::GetAcceptanceRadius() const
{
	return CachedArchetype ? CachedArchetype->GetStats().AcceptanceRadius : 100.f;
}

float AEnemyAIControllerBase::GetAttackRange() const
{
	return CachedArchetype ? CachedArchetype->GetStats().AttackRange : 200.f;
}

float AEnemyAIControllerBase::GetTurnSpeed() const
{
	return CachedArchetype ? CachedArchetype->GetStats().TurnSpeed : 360.f;
}

bool AEnemyAIControllerBase::HasRangedAttack() const
{
	AEnemyCharacterBase* Char = Cast<AEnemyCharacterBase>(GetPawn());
	if (!Char)
	{
		return false;
	}

	// ASC에 원거리 공격 태그를 가진 어빌리티가 있나 확인한다.
	UEnemyAbilitySystemComponent* ASC = Char->GetEnemyASC();
	if (ASC && ASC->HasAbilityByTag(AbilityTag::TAG_Ability_Offensive_Ranged))
	{
		return true;
	}

	return false;
}

bool AEnemyAIControllerBase::ShouldStayHome() const
{
	return CachedArchetype ? CachedArchetype->GetStats().bStayAtHome : false;
}

bool AEnemyAIControllerBase::ShouldUseSpacing() const
{
	return CachedArchetype ? CachedArchetype->GetStats().bUseSpacing : false;
}

void AEnemyAIControllerBase::SetStrafeFacing(bool bEnable)
{
	// 스페이싱을 안 쓰는 적은 이 기능 자체가 없다.
	//
	// 왜 여기서도 막나: 이동 태스크는 전 적이 공유한다. StateTree 조건만 믿으면
	// 누군가 다른 상태에 이 태스크를 달았을 때 기존 적의 회전이 조용히 바뀐다.
	if (!ShouldUseSpacing())
	{
		return;
	}

	ACharacter* Char = Cast<ACharacter>(GetPawn());
	if (!Char)
	{
		return;
	}

	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	if (!bEnable)
	{
		// 안 켰으면 되돌릴 것도 없다
		if (!bStrafeFacing)
		{
			return;
		}

		ClearFocus(EAIFocusPriority::Gameplay);
		Move->bUseControllerDesiredRotation = false;
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = SavedRotationRate;
		bStrafeFacing = false;
		return;
	}

	// 이미 켜져 있으면 원래 회전 속도를 덮어쓰지 않는다.
	// 이 가드가 없으면 두 번째 진입에서 SavedRotationRate 에 TurnSpeed 가 들어가
	// 영영 복원이 안 된다.
	if (bStrafeFacing)
	{
		return;
	}

	AActor* Target = AlertComponent ? AlertComponent->GetCurrentTarget() : nullptr;
	if (!Target)
	{
		// 볼 대상이 없으면 평소 동작을 그대로 둔다.
		// 여기서 플래그만 바꾸면 타겟이 생길 때까지 몸이 안 돈다.
		return;
	}

	SavedRotationRate = Move->RotationRate;

	SetFocus(Target, EAIFocusPriority::Gameplay);
	Move->bOrientRotationToMovement = false;
	Move->bUseControllerDesiredRotation = true;
	Move->RotationRate = FRotator(0.f, GetTurnSpeed(), 0.f);
	bStrafeFacing = true;
}

float AEnemyAIControllerBase::GetDistanceToTarget() const
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return -1.f;
	}

	AActor* Target = AlertComponent ? AlertComponent->GetCurrentTarget() : nullptr;
	if (!Target)
	{
		return -1.f;
	}

	return FVector::Dist2D(MyPawn->GetActorLocation(), Target->GetActorLocation());
}

void AEnemyAIControllerBase::TryRetarget()
{
	if (!AlertComponent)
	{
		return;
	}

	const FEnemyStatRow* S = CachedArchetype ? &CachedArchetype->GetStats() : nullptr;
	const float RetargetChance = S ? S->RetargetChance : 0.f;

	if (FMath::FRand() >= RetargetChance)
	{
		return;
	}

	AlertComponent->RequestRetarget();
}

void AEnemyAIControllerBase::PickTacticalMove()
{
	const FEnemyStatRow* S = CachedArchetype ? &CachedArchetype->GetStats() : nullptr;

	const float RetreatChance = S ? S->RetreatChance : 0.3f;
	const float KeepDistance = S ? S->KeepDistance : 350.f;

	// 너무 붙었으면 확률과 무관하게 물러난다.
	//
	// 왜 거리를 먼저 보나: 확률로만 뽑으면 붙은 채로 계속 도는 구간이 생기고,
	// 그 거리에서는 선회 지점이 AcceptanceRadius 안에 들어와 아예 안 움직인다.
	// 그리고 후퇴와 선회가 번갈아 뽑히면 방향이 정반대라 지그재그로 떤다.
	const float Distance = GetDistanceToTarget();
	if (Distance >= 0.f)
	{
		if (Distance < KeepDistance)
		{
			CurrentTactic = EEnemyTactic::Backstep;
			return;
		}
	}

	if (FMath::FRand() < RetreatChance)
	{
		CurrentTactic = EEnemyTactic::Backstep;
		return;
	}

	CurrentTactic = EEnemyTactic::Orbit;
}

FVector AEnemyAIControllerBase::GetStrafePoint() const
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return FVector::ZeroVector;
	}

	// 해당 기능은 타겟이 있어야함.
	AActor* Target = AlertComponent ? AlertComponent->GetCurrentTarget() : nullptr;
	if (!Target)
	{
		return MyPawn->GetActorLocation();
	}

	const FVector MyLoc = MyPawn->GetActorLocation();
	const FVector TargetLoc = Target->GetActorLocation();

	// 타겟에서 나를 향하는 수평 방향
	FVector FleeDir = MyLoc - TargetLoc;
	FleeDir.Z = 0.f;
	FleeDir = FleeDir.GetSafeNormal();

	// 정확히 겹쳐 서 있으면 방향을 만들 수 없다
	if (FleeDir.IsNearlyZero())
	{
		return MyLoc;
	}

	const FEnemyStatRow* S = CachedArchetype ? &CachedArchetype->GetStats() : nullptr;
	const float StepDegrees = S ? S->OrbitStepDegrees : 35.f;
	const float BackDistance = S ? S->BackstepDistance : 400.f;
	const float KeepDistance = S ? S->KeepDistance : 350.f;

	FVector Projected = FVector::ZeroVector;

	// 후퇴 — 타겟 반대방향으로
	//
	// 왜 여기서 확률을 안 굴리나: 예전에는 여기서 RandRange(0, 1) 로 굴렸는데
	// 이건 정수 버전이라 0 아니면 1만 나왔다. 0 <= 0.3 은 참, 1 <= 0.3 은 거짓이라
	// 적어둔 확률과 무관하게 언제나 50% 였다. 이제 결정은 PickTacticalMove 가 한다.
	if (CurrentTactic == EEnemyTactic::Backstep)
	{
		if (ProjectStrafePoint(MyLoc + FleeDir * BackDistance, Projected))
		{
			return Projected;
		}

		return MyLoc;
	}

	// 선회 — 타겟을 중심으로 StepDegrees 만큼 돈다.
	//
	// 반지름에 하한을 두는 이유: 지금 거리를 그대로 쓰면 붙어 있을 때 선회 지점이
	// AcceptanceRadius 안에 들어와 MoveTo 가 즉시 완료되고 제자리에 선다.
	// 하한을 걸면 붙은 상태에서도 바깥으로 돌아 나가게 된다.
	const float Radius = FMath::Max(FVector::Dist2D(MyLoc, TargetLoc), KeepDistance);
	const float Sign = bOrbitClockwise ? 1.f : -1.f;

	// 한쪽이 막히면 반대쪽으로 한 번만 더 본다.
	// 방향 자체를 뒤집지는 않는다. 매번 뒤집으면 좌우로 흔들린다.
	for (int32 Attempt = 0; Attempt < 2; ++Attempt)
	{
		const float Flip = (Attempt == 0) ? 1.f : -1.f;
		const FVector Dir = FleeDir.RotateAngleAxis(StepDegrees * Sign * Flip, FVector::UpVector);

		if (ProjectStrafePoint(TargetLoc + Dir * Radius, Projected))
		{
			return Projected;
		}
	}

	// 갈 곳이 없다. 제자리를 주면 MoveTo 가 즉시 끝난다
	return MyLoc;
}

bool AEnemyAIControllerBase::ProjectStrafePoint(const FVector& RawPoint, FVector& OutPoint) const
{
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Nav)
	{
		// 네비가 없으면 검증할 방법이 없다. 계산값을 그대로 쓴다.
		OutPoint = RawPoint;
		return true;
	}

	FNavLocation Projected;
	if (!Nav->ProjectPointToNavigation(RawPoint, Projected, StrafeNavExtent))
	{
		return false;
	}

	OutPoint = Projected.Location;
	return true;
}

void AEnemyAIControllerBase::ApplyArchetype(AEnemyCharacterBase* Char)
{
	const UEnemyArchetypeData* Data = Char ? Char->GetArchetypeData() : nullptr;

	if (!Data)
	{
		// 애셋 미지정. 각 클래스의 UPROPERTY 기본값으로 동작한다.
		// Warning 으로 남기는 이유: 조용히 넘기면 "왜 값을 바꿨는데 안 먹지"를 나중에 추적하게 된다.
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
		Char->SetReward(S.ExpReward);
		Char->SetLootRow(S.LootRow);

		if (UEnemyAbilitySystemComponent* ASC = Char->GetEnemyASC())
		{
			ASC->InitStats(S.MaxHealth, S.AttackPower,S.AttackSpeedRate);
			ASC->SetupAbilities(Data->Abilities);
		}

		// 데이터 애셋에 적힌 컴포넌트를 붙인다.
		for (const TSubclassOf<UActorComponent>& CompClass : Data->ExtraComponents)
		{
			if (!CompClass) 
				continue;

			// 이미 BP 에 붙어 있으면 중복 생성하지 않는다.
			if (Char->FindComponentByClass(CompClass)) 
				continue;

			UActorComponent* NewComp = NewObject<UActorComponent>(Char, CompClass);
			if (NewComp)
			{
				NewComp->RegisterComponent();
			}
		}
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
	if (!AsPawn || !AsPawn->IsPlayerControlled())
	{
		return false;
	}

	// 죽은 플레이어는 타겟이 아니다.
	if (!AlertComponent->IsTargetAlive(Actor))
	{
		return false;
	}

	return true;
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

	// 뒤에서 가까이 붙으면 자동 감지되는 기능
	// -1이면 비활성화
	SightConfig->AutoSuccessRangeFromLastSeenLocation = Stats ? Stats->AutoSuccessSightRange : -1.f;

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

	// 임시 확인용
	UE_LOG(LogEnemyAI, Warning, TEXT("[Sight] %s sensed=%d  AutoSuccess=%.0f  MaxAge=%.1f"),
		*GetNameSafe(Actor),
		Stimulus.WasSuccessfullySensed() ? 1 : 0,
		SightConfig->AutoSuccessRangeFromLastSeenLocation,
		SightConfig->GetMaxAge());

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

void AEnemyAIControllerBase::OnTargetPerceptionForgotten(AActor* Actor)
{
	if (!AlertComponent || !IsValidTarget(Actor))
	{
		return;
	}
	// 시야밖의 대상을 잊어먹음
	AlertComponent->SightForgotten(Actor);
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
