// Copyright HeavenBreach Team5. All Rights Reserved.

#include "DoorActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADoorActor::ADoorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	DoorFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrame"));
	RootComponent = DoorFrame;
	DoorFrame->SetCollisionProfileName(FName("BlockAll"));

	// 경첩을 회전축으로 쓴다. 문짝 메시의 피벗이 중앙에 있어도 제대로 열린다.
	// 경첩 위치는 문틀 메시에 맞춰 BP 에서 조정한다.
	HingeLeft = CreateDefaultSubobject<USceneComponent>(TEXT("HingeLeft"));
	HingeLeft->SetupAttachment(DoorFrame);

	DoorMeshLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMeshLeft"));
	DoorMeshLeft->SetupAttachment(HingeLeft);
	DoorMeshLeft->SetCollisionProfileName(FName("BlockAll"));

	HingeRight = CreateDefaultSubobject<USceneComponent>(TEXT("HingeRight"));
	HingeRight->SetupAttachment(DoorFrame);

	DoorMeshRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMeshRight"));
	DoorMeshRight->SetupAttachment(HingeRight);
	DoorMeshRight->SetCollisionProfileName(FName("BlockAll"));

	DoorDisplayName = NSLOCTEXT("Door", "DefaultDoorName", "문");
}

void ADoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADoorActor, bIsOpen);
	DOREPLIFETIME(ADoorActor, StateChangeServerTime);
}

bool ADoorActor::CanInteract_Implementation(AActor* Interactor) const
{
	// 열린 문도 상호작용 대상이다 — 다시 누르면 닫힌다.
	// 문짝이 도는 중에만 거부해서 연타로 연출이 겹치는 것을 막는다.
	return Interactor != nullptr && !IsSwinging();
}

void ADoorActor::Interact_Implementation(AActor* Interactor)
{
	// UInteractionComponent::Server_Interact 가 거리를 재검증한 뒤 서버에서 호출한다.
	if (!HasAuthority() || Interactor == nullptr)
	{
		return;
	}

	// CanInteract 가 이미 걸러주지만, 클라이언트 시각 추정 오차로 조금 이르게 들어올 수 있다.
	if (IsSwinging())
	{
		return;
	}

	// bIsOpen 과 같은 리플리케이션 묶음에 실려야 하므로 대입 직전에 설정한다.
	// GameState 를 못 가져오면 NeverChangedServerTime 에 남는다 — 문은 정상적으로
	// 여닫히되 연출 없이 결과 자세만 적용되는, 안전한 쪽으로 어긋난다.
	if (const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		StateChangeServerTime = GameState->GetServerWorldTimeSeconds();
	}

	bIsOpen = !bIsOpen;

	// 서버에서는 RepNotify 가 자동 호출되지 않는다 (ASealedDoorActor 와 동일한 처리)
	OnRep_IsOpen();
}

FText ADoorActor::GetInteractPrompt_Implementation() const
{
	// 도는 중에는 프롬프트를 비워 진행 중임을 알린다.
	if (IsSwinging())
	{
		return FText::GetEmpty();
	}

	const FText Action = bIsOpen
		? NSLOCTEXT("Door", "ClosePromptAction", "닫기")
		: NSLOCTEXT("Door", "OpenPromptAction", "열기");

	if (DoorDisplayName.IsEmpty())
	{
		return Action;
	}

	return FText::Format(
		NSLOCTEXT("Door", "DoorPrompt", "{0} {1}"),
		DoorDisplayName,
		Action);
}

void ADoorActor::OnRep_IsOpen()
{
	// 닫히는 도중에 다시 열면 예약된 콜리전 복구가 남아 있을 수 있다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CollisionRestoreTimer);
	}

	const bool bInstant = !WasJustChanged();

	ApplyCollisionForState(bInstant);

	// 뒤늦게 relevant 해진 클라이언트의 자세는 C++ 이 맞춘다. BP 에 맡기면 연결을
	// 빠뜨렸을 때 문짝이 닫힌 채로 남아 통과할 수 있는 벽처럼 보인다.
	if (bInstant)
	{
		ApplyAngleInstant(bIsOpen);
	}

	BP_OnDoorStateChanged(bIsOpen, bInstant);
}

void ADoorActor::ApplyCollisionForState(bool bInstant)
{
	if (bIsOpen)
	{
		// 회전이 시작되는 지금 푼다.
		if (DoorMeshLeft)
		{
			DoorMeshLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (DoorMeshRight)
		{
			DoorMeshRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		return;
	}

	if (bInstant)
	{
		RestoreDoorCollision();
		return;
	}

	const float Remaining = FMath::Max(0.0f, SwingDurationSeconds - GetTimeSinceStateChange());
	UWorld* World = GetWorld();
	if (World == nullptr || Remaining <= KINDA_SMALL_NUMBER)
	{
		RestoreDoorCollision();
		return;
	}

	World->GetTimerManager().SetTimer(
		CollisionRestoreTimer, this, &ADoorActor::RestoreDoorCollision, Remaining, false);
}

void ADoorActor::RestoreDoorCollision()
{
	if (DoorMeshLeft)
	{
		DoorMeshLeft->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (DoorMeshRight)
	{
		DoorMeshRight->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

void ADoorActor::ApplyAngleInstant(bool bOpen)
{
	if (HingeLeft)
	{
		HingeLeft->SetRelativeRotation(FRotator(0.0f, bOpen ? OpenYawLeft : 0.0f, 0.0f));
	}

	if (HingeRight)
	{
		HingeRight->SetRelativeRotation(FRotator(0.0f, bOpen ? OpenYawRight : 0.0f, 0.0f));
	}
}

float ADoorActor::GetTimeSinceStateChange() const
{
	if (StateChangeServerTime <= NeverChangedServerTime)
	{
		return LongAgoSeconds;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		// 판정할 수 없으면 연출을 재생하지 않는 쪽이 안전하다.
		return LongAgoSeconds;
	}

	return GameState->GetServerWorldTimeSeconds() - StateChangeServerTime;
}

bool ADoorActor::IsSwinging() const
{
	return GetTimeSinceStateChange() < SwingDurationSeconds;
}

bool ADoorActor::WasJustChanged() const
{
	// 클라이언트의 서버 시각 추정에는 약간의 오차가 있을 수 있으므로 절댓값으로 비교한다.
	return FMath::Abs(GetTimeSinceStateChange()) <= SwingDurationSeconds;
}
