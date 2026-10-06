// Copyright HeavenBreach Team5. All Rights Reserved.


#include "InteractionComponent.h"
#include "InteractableInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Player/HBInteractAnimComponent.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr)
	{
		return;
	}

	// 서버는 홀드 중인 대상이 여전히 유효한 거리·상태인지 감시한다.
	// 클라이언트가 키를 누른 채 걸어나가도 서버가 끊어야 하기 때문이다.
	if (GetOwnerRole() == ROLE_Authority && ServerHeldTarget != nullptr)
	{
		if (!IsTargetInteractable(ServerHeldTarget))
		{
			CancelHoldOnServer();
		}
	}

	if (!Pawn->IsLocallyControlled())
	{
		return;
	}

	UpdateFocus();

	// 보고 있던 대상에서 시선이 벗어나면 홀드를 놓은 것으로 본다.
	if (HeldTarget != nullptr && FocusedInteractable != HeldTarget)
	{
		EndHold();
	}
}

void UInteractionComponent::UpdateFocus()
{
	// 인벤토리 / 상자 / 상점이 열려 있는 동안에는 탐색을 멈추고 포커스를 놓는다.
	// 포커스를 nullptr 로 밀면 OnFocusChanged 를 타고 프롬프트 위젯이 스스로 숨는다.
	// (프롬프트는 AddToViewport 로 붙어 있어 UI 레이어에 가려지지 않으므로,
	//  여기서 끄지 않으면 상자 화면 위에 "F 상자 오픈"이 계속 남는다)
	if (IsInputOwnedByMenu())
	{
		SetFocusedInteractable(nullptr);
		return;
	}

	FVector ViewLocation;
	FVector ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		SetFocusedInteractable(nullptr);
		return;
	}

	const FVector TraceEnd = ViewLocation + ViewDirection * InteractionDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionTrace), false);
	Params.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, TraceChannel, Params);

	AActor* Candidate = nullptr;
	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->Implements<UInteractableInterface>()
			&& IInteractableInterface::Execute_CanInteract(HitActor, GetOwner()))
		{
			Candidate = HitActor;
		}
	}

	SetFocusedInteractable(Candidate);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		const FColor LineColor = (Candidate != nullptr) ? FColor::Green : FColor::Red;
		DrawDebugLine(GetWorld(), ViewLocation, TraceEnd, LineColor, false, -1.0f, 0, 1.0f);
	}
#endif
}

void UInteractionComponent::SetFocusedInteractable(AActor* NewInteractable)
{
	if (FocusedInteractable == NewInteractable)
	{
		return;
	}

	FocusedInteractable = NewInteractable;
	OnFocusChanged.Broadcast(NewInteractable);
}

void UInteractionComponent::TryInteract()
{
	if (FocusedInteractable == nullptr)
	{
		return;
	}

	Server_Interact(FocusedInteractable);
}

void UInteractionComponent::BeginHold()
{
	if (FocusedInteractable == nullptr || !FocusedInteractable->Implements<UInteractableInterface>())
	{
		return;
	}

	const float HoldSeconds = IInteractableInterface::Execute_GetInteractHoldSeconds(FocusedInteractable);

	// 홀드가 필요 없는 대상은 기존 단발 경로 그대로. 상자·탈출 지점이 여기로 온다.
	if (HoldSeconds <= 0.0f)
	{
		Server_Interact(FocusedInteractable);
		return;
	}

	// 클라가 알 수 있는 조건(열쇠 등)으로 먼저 거른다. 안 되면 동작·진행 바를 켜지 않는다.
	// 서버도 OnHoldBegin 으로 다시 판정하므로 여기를 속여도 문은 안 열린다.
	if (!IInteractableInterface::Execute_CanBeginHold(FocusedInteractable, GetOwnerPawn()))
	{
		return;
	}

	HeldTarget = FocusedInteractable;
	HoldDuration = HoldSeconds;
	HoldStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// 뒤지기 시작하는 동작. 끄는 건 EndHold(취소) 또는 상자 창이 닫힐 때(컨트롤러)다.
	if (UHBInteractAnimComponent* InteractAnim = GetOwner()->FindComponentByClass<UHBInteractAnimComponent>())
	{
		InteractAnim->SetInteracting(true);
	}

	Server_BeginHold(FocusedInteractable);
}

void UInteractionComponent::EndHold()
{
	if (HeldTarget == nullptr)
	{
		return;
	}

	HeldTarget = nullptr;
	HoldDuration = 0.0f;
	HoldStartTime = 0.0f;

	// 뒤지는 동작은 누르고 있는 동안만이다. 손을 뗐든, 멀어졌든, 다 뒤져서 상자 창이 열렸든 여기서 끈다.
	if (UHBInteractAnimComponent* InteractAnim = GetOwner()->FindComponentByClass<UHBInteractAnimComponent>())
	{
		InteractAnim->SetInteracting(false);
	}

	Server_EndHold();
}

void UInteractionComponent::Client_HoldRejected_Implementation(AActor* Target)
{
	// 그사이 다른 대상을 누르기 시작했으면 그건 건드리지 않는다.
	if (HeldTarget == nullptr || HeldTarget != Target)
	{
		return;
	}
	AbortHold();
}

void UInteractionComponent::AbortHold()
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		CancelHoldOnServer();
	}

	if (HeldTarget == nullptr)
	{
		return;
	}

	HeldTarget = nullptr;
	HoldDuration = 0.0f;
	HoldStartTime = 0.0f;

	if (UHBInteractAnimComponent* InteractAnim = GetOwner()->FindComponentByClass<UHBInteractAnimComponent>())
	{
		InteractAnim->SetInteracting(false);
	}
}

float UInteractionComponent::GetHoldProgress() const
{
	if (HeldTarget == nullptr || HoldDuration <= 0.0f || GetWorld() == nullptr)
	{
		return 0.0f;
	}

	const float Elapsed = GetWorld()->GetTimeSeconds() - HoldStartTime;
	return FMath::Clamp(Elapsed / HoldDuration, 0.0f, 1.0f);
}

bool UInteractionComponent::IsTargetInteractable(AActor* Target) const
{
	const APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr || Target == nullptr || !IsValid(Target))
	{
		return false;
	}

	const float MaxDistance = InteractionDistance + ServerDistanceTolerance;
	if (FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		return false;
	}

	return Target->Implements<UInteractableInterface>()
		&& IInteractableInterface::Execute_CanInteract(Target, const_cast<APawn*>(Pawn));
}

bool UInteractionComponent::Server_BeginHold_Validate(AActor* Target)
{
	return true;
}

void UInteractionComponent::Server_BeginHold_Implementation(AActor* Target)
{
	// 이전 홀드가 남아 있으면 정리하고 시작한다.
	if (ServerHeldTarget != nullptr)
	{
		CancelHoldOnServer();
	}

	if (!IsTargetInteractable(Target))
	{
		Client_HoldRejected(Target);
		return;
	}

	APawn* Pawn = GetOwnerPawn();

	const float HoldSeconds = IInteractableInterface::Execute_GetInteractHoldSeconds(Target);
	if (HoldSeconds <= 0.0f)
	{
		// 클라이언트가 보낸 값이 아니라 서버가 다시 물어본 값이다. 0 이면 즉시 실행.
		IInteractableInterface::Execute_Interact(Target, Pawn);
		return;
	}

	// 대상이 거절하면 홀드를 시작하지 않는다. 봉인문은 필요한 열쇠가 없으면 여기서 거절한다.
	if (!IInteractableInterface::Execute_OnHoldBegin(Target, Pawn))
	{
		Client_HoldRejected(Target);
		return;
	}

	ServerHeldTarget = Target;

	GetWorld()->GetTimerManager().SetTimer(
		ServerHoldTimerHandle,
		this,
		&UInteractionComponent::CompleteHold,
		HoldSeconds,
		false);
}

void UInteractionComponent::Server_EndHold_Implementation()
{
	CancelHoldOnServer();
}

void UInteractionComponent::CompleteHold()
{
	AActor* Target = ServerHeldTarget;
	ServerHeldTarget = nullptr;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ServerHoldTimerHandle);
	}

	// 시간이 다 찼어도 완료 시점에 다시 검증한다. 홀드 도중 상태가 바뀌었을 수 있다.
	if (!IsTargetInteractable(Target))
	{
		if (Target && Target->Implements<UInteractableInterface>())
		{
			IInteractableInterface::Execute_OnHoldCancel(Target, GetOwnerPawn());
		}
		return;
	}

	IInteractableInterface::Execute_Interact(Target, GetOwnerPawn());
}

void UInteractionComponent::CancelHoldOnServer()
{
	if (ServerHeldTarget == nullptr)
	{
		return;
	}

	AActor* Target = ServerHeldTarget;
	ServerHeldTarget = nullptr;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ServerHoldTimerHandle);
	}

	if (IsValid(Target) && Target->Implements<UInteractableInterface>())
	{
		IInteractableInterface::Execute_OnHoldCancel(Target, GetOwnerPawn());
	}
}

bool UInteractionComponent::Server_Interact_Validate(AActor* Target)
{
	return true;
}

void UInteractionComponent::Server_Interact_Implementation(AActor* Target)
{
	APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr || Target == nullptr)
	{
		return;
	}

	const float MaxDistance = InteractionDistance + ServerDistanceTolerance;
	if (FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		return;
	}

	if (Target->Implements<UInteractableInterface>()
		&& IInteractableInterface::Execute_CanInteract(Target, Pawn))
	{
		IInteractableInterface::Execute_Interact(Target, Pawn);
	}
}

FText UInteractionComponent::GetCurrentPrompt() const
{
	if (FocusedInteractable && FocusedInteractable->Implements<UInteractableInterface>())
	{
		return IInteractableInterface::Execute_GetInteractPrompt(FocusedInteractable);
	}
	return FText::GetEmpty();
}

bool UInteractionComponent::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr)
	{
		return false;
	}

	if (const AController* Controller = Pawn->GetController())
	{
		FVector Location;
		FRotator Rotation;
		Controller->GetPlayerViewPoint(Location, Rotation);
		OutLocation = Location;
		OutDirection = Rotation.Vector();
		return true;
	}

	OutLocation = Pawn->GetActorLocation();
	OutDirection = Pawn->GetActorForwardVector();
	return true;
}

bool UInteractionComponent::IsInputOwnedByMenu() const
{
	if (!bSuppressWhileMenuInput)
	{
		return false;
	}

	const APawn* Pawn = GetOwnerPawn();
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC == nullptr)
	{
		return false;
	}

	const UCommonUIActionRouterBase* ActionRouter =
		ULocalPlayer::GetSubsystemFromController<UCommonUIActionRouterBase>(PC);
	if (ActionRouter == nullptr)
	{
		return false;
	}

	// Menu = UI가 입력을 독점한 상태(인벤토리 / 상자 / 상점).
	// All(GameAndMenu)은 게임 입력이 살아 있는 상태이므로 상호작용을 막지 않는다.
	return ActionRouter->GetActiveInputMode() == ECommonInputMode::Menu;
}

APawn* UInteractionComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}

void UInteractionComponent::Client_OpenContainer_Implementation(UInventoryComponent* Container)
{
	OnRequestOpenContainer.Broadcast(Container);
}
