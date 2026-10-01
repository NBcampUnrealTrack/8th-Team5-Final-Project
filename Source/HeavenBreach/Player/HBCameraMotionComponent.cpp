#include "Player/HBCameraMotionComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "UI/Settings/HBSettingsSubsystem.h"

UHBCameraMotionComponent::UHBCameraMotionComponent()
{
	// 애니메이션이 머리 뼈를 움직인 뒤, 카메라 매니저가 화면을 계산하기 전에 돌아야 한다.
	// 월드 틱 순서가 PostPhysics -> 카메라 갱신 -> PostUpdateWork 라서 PostUpdateWork 는 한 프레임 늦다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UHBCameraMotionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 첫 프레임 전이라 아직 덮어쓰지 않은, 머리 뼈 기준 상대 위치·회전이 들어 있다.
	Camera = GetOwner() ? GetOwner()->FindComponentByClass<UCameraComponent>() : nullptr;
	if (Camera)
	{
		CameraRelativeToHead = Camera->GetRelativeTransform();

		// 머리 뼈를 움직이는 메시 틱이 끝난 뒤에 읽는다.
		if (UPrimitiveComponent* HeadParent = Cast<UPrimitiveComponent>(Camera->GetAttachParent()))
		{
			AddTickPrerequisiteComponent(HeadParent);
		}
	}
}

float UHBCameraMotionComponent::GetEffectiveMotionScale() const
{
	float PlayerScale = 1.f;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UHBSettingsSubsystem* Settings = GameInstance->GetSubsystem<UHBSettingsSubsystem>())
			{
				PlayerScale = Settings->GetCameraMotionScale();
			}
		}
	}
	return FMath::Clamp(MotionScale * PlayerScale, 0.f, 1.f);
}

void UHBCameraMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 화면은 본인에게만 있다. 남의 캐릭터 카메라는 쓰이지 않는다.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	USceneComponent* HeadParent = Camera ? Camera->GetAttachParent() : nullptr;
	if (!Pawn || !Pawn->IsLocallyControlled() || !HeadParent)
	{
		return;
	}

	const float Scale = GetEffectiveMotionScale();

	// 세기 1 은 머리에 그대로 붙은 원래 상태다. 덮어쓰던 게 있으면 원래 상대 위치로 돌려놓는다.
	if (Scale >= 1.f - KINDA_SMALL_NUMBER)
	{
		if (bOverriding)
		{
			Camera->SetRelativeTransform(CameraRelativeToHead);
			bOverriding = false;
		}
		bHasSmoothed = false;
		return;
	}

	// 머리에 그대로 붙었다면 카메라가 있었을 자리. 카메라의 지금 상대 위치는 덮어쓴 값이라 쓰지 않는다.
	const FTransform HeadCamera = CameraRelativeToHead * HeadParent->GetSocketTransform(Camera->GetAttachSocketName());
	const FTransform ActorTransform = Pawn->GetActorTransform();

	// 위치는 캐릭터 기준으로 본다. 월드 기준으로 부드럽게 하면 걷는 것 자체가 늦게 따라온다.
	const FVector LocalLocation = ActorTransform.InverseTransformPosition(HeadCamera.GetLocation());

	// 회전은 조준 방향과의 차이만 본다. 조준까지 부드럽게 하면 마우스가 늦게 따라온다.
	const AController* Controller = Pawn->GetController();
	const FQuat AimQuat = Controller ? Controller->GetControlRotation().Quaternion() : ActorTransform.GetRotation();
	const FQuat AimDelta = AimQuat.Inverse() * HeadCamera.GetRotation();

	if (!bHasSmoothed)
	{
		SmoothedLocalLocation = LocalLocation;
		SmoothedAimDelta = AimDelta;
		bHasSmoothed = true;
	}

	// 프레임률과 상관없이 같은 빠르기로 따라가도록 지수 감쇠를 쓴다.
	const float Follow = 1.f - FMath::Exp(-StabilizeSpeed * DeltaTime);
	SmoothedLocalLocation = FMath::Lerp(SmoothedLocalLocation, LocalLocation, Follow);
	SmoothedAimDelta = FQuat::Slerp(SmoothedAimDelta, AimDelta, Follow);

	// 세기만큼 원래 흔들림을 섞는다. 0 이면 부드러운 값만, 1 이면 머리 그대로.
	const FVector FinalLocal = FMath::Lerp(SmoothedLocalLocation, LocalLocation, Scale);
	const FQuat FinalDelta = FQuat::Slerp(SmoothedAimDelta, AimDelta, Scale);

	Camera->SetWorldLocationAndRotation(ActorTransform.TransformPosition(FinalLocal), AimQuat * FinalDelta);
	bOverriding = true;
}
