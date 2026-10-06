#include "Combat/Trap/HBPendulumComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"

UHBPendulumComponent::UHBPendulumComponent()
{
	// 서버(판정)와 모든 클라(화면)에서 같이 돈다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UHBPendulumComponent::BeginPlay()
{
	Super::BeginPlay();

	BaseRelativeRotation = GetRelativeRotation().Quaternion();
}

void UHBPendulumComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 막 접속한 클라는 게임 상태가 아직 없을 수 있다. 그동안은 배치한 자세 그대로 둔다.
	const AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return;
	}

	// 각자 시작 시점부터 세면 화면마다 박자가 어긋난다. 모두가 공유하는 서버 시간으로 센다.
	const double Cycles = GameState->GetServerWorldTimeSeconds() / SwingPeriod + PhaseOffset;
	const double Angle = SwingAngle * FMath::Sin(2.0 * UE_DOUBLE_PI * Cycles);

	// 배치한 회전 기준으로 자기 축을 따라 돌린다. 붙어 있는 도끼 · 판정 박스가 같이 움직인다.
	const FQuat Swing(GetLocalAxis(), FMath::DegreesToRadians(Angle));
	SetRelativeRotation(BaseRelativeRotation * Swing);

	UpdateSwingSound(GameState->GetServerWorldTimeSeconds());
}

void UHBPendulumComponent::UpdateSwingSound(double ServerTime)
{
	if (!SwingSound)
	{
		return;
	}

	// 소리를 들을 사람이 없다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Lead 만큼 앞선 시점의 기울기 방향. 방향이 바뀌는 순간이 "바닥을 지나는" 순간이다.
	const double LeadCycles = (ServerTime + SwingSoundLead) / SwingPeriod + PhaseOffset;
	const bool bPositive = FMath::Sin(2.0 * UE_DOUBLE_PI * LeadCycles) >= 0.0;

	if (!bHasLastSwingSide)
	{
		bHasLastSwingSide = true;
		bLastSwingSidePositive = bPositive;
		return;
	}

	if (bPositive == bLastSwingSidePositive)
	{
		return;
	}

	bLastSwingSidePositive = bPositive;

	const FVector SoundLocation = GetComponentTransform().TransformPosition(SwingSoundOffset);
	UGameplayStatics::PlaySoundAtLocation(this, SwingSound, SoundLocation, SwingVolume);
}

FVector UHBPendulumComponent::GetLocalAxis() const
{
	if (SwingAxis == EHBSwingAxis::Y)
	{
		return FVector::RightVector;
	}

	if (SwingAxis == EHBSwingAxis::Z)
	{
		return FVector::UpVector;
	}

	return FVector::ForwardVector;
}
