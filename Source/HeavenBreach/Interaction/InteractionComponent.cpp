// Copyright HeavenBreach Team5. All Rights Reserved.


#include "InteractionComponent.h"
#include "InteractableInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 컴포넌트에서 Server RPC를 라우팅하려면 리플리케이트되어야 한다. (소유 폰은 클라 커넥션 소유)
	SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// NOTE: 빙의(possess)가 BeginPlay보다 늦을 수 있어 여기서 틱을 끄면 로컬 폰인데도 영영 안 켜질 수 있다.
	// 그래서 비활성화하지 않고, TickComponent가 매 틱 IsLocallyControlled로 조기 리턴하도록 둔다.
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 소유권이 늦게 확정될 수 있으므로 매 틱 로컬 컨트롤 여부를 확인한다.
	const APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr || !Pawn->IsLocallyControlled())
	{
		return;
	}

	UpdateFocus();
}

void UInteractionComponent::UpdateFocus()
{
	FVector ViewLocation;
	FVector ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		SetFocusedInteractable(nullptr);
		return;
	}

	const FVector TraceEnd = ViewLocation + ViewDirection * InteractionDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionTrace), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, TraceChannel, Params);

	AActor* Candidate = nullptr;
	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		// 대상이 인터페이스를 구현하고, 지금 상호작용 가능한 상태일 때만 후보로 삼는다.
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
	// 조준 대상이 없으면 아무것도 하지 않는다.
	if (FocusedInteractable == nullptr)
	{
		return;
	}

	// 판정은 서버에서. (로컬에서 호출되어 서버로 전달됨)
	Server_Interact(FocusedInteractable);
}

bool UInteractionComponent::Server_Interact_Validate(AActor* Target)
{
	// 기본 검증: 대상이 유효하고 상호작용 인터페이스를 구현해야 한다.
	return Target != nullptr && Target->Implements<UInteractableInterface>();
}

void UInteractionComponent::Server_Interact_Implementation(AActor* Target)
{
	APawn* Pawn = GetOwnerPawn();
	if (Pawn == nullptr || Target == nullptr)
	{
		return;
	}

	// 치팅 방지: 서버에서 거리를 재검증한다.
	const float MaxDistance = InteractionDistance + ServerDistanceTolerance;
	if (FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		return;
	}

	// 대상이 지금 상호작용 가능한지 최종 확인 후 실행한다.
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

	// 컨트롤러의 시점(1인칭 카메라 시점)을 우선 사용한다.
	if (const AController* Controller = Pawn->GetController())
	{
		FVector Location;
		FRotator Rotation;
		Controller->GetPlayerViewPoint(Location, Rotation);
		OutLocation = Location;
		OutDirection = Rotation.Vector();
		return true;
	}

	// 폴백: 폰 위치/전방
	OutLocation = Pawn->GetActorLocation();
	OutDirection = Pawn->GetActorForwardVector();
	return true;
}

APawn* UInteractionComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}

void UInteractionComponent::Client_OpenContainer_Implementation(UInventoryComponent* Container)
{
	// 요청 클라에서 실행됨. 실제 UI 오픈(InventoryUI.OpenContainer)은 BP에서 이 이벤트에 연결한다.
	OnRequestOpenContainer.Broadcast(Container);
}
