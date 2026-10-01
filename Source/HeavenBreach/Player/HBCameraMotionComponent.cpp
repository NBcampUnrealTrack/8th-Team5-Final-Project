#include "Player/HBCameraMotionComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PawnMovementComponent.h"
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

void UHBCameraMotionComponent::NotifyLanded(const float Impact)
{
	if (const UWorld* World = GetWorld())
	{
		LandTime = World->GetTimeSeconds();
	}
	LandImpact = FMath::Clamp(Impact, 0.f, 1.f);

	// 아래로 튕기는 속도를 준다. 스프링이 알아서 눌렸다 돌아온다.
	// 목표 깊이 LandDipMax×Impact 가 되도록, 임계 감쇠 스프링에서 최대 변위 ≈ v / (e·ω) 관계를 쓴다.
	if (Impact <= 0.f || LandDipMax <= 0.f)
	{
		return;
	}
	const float Omega = FMath::Sqrt(LandDipStiffness);
	DipVelocity -= LandDipMax * Impact * Omega * UE_EULERS_NUMBER;
}

float UHBCameraMotionComponent::GetLandingSuppression() const
{
	const UWorld* World = GetWorld();
	if (!World || LandingWindow <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - LandTime);
	if (Elapsed < 0.f || Elapsed >= LandingWindow)
	{
		return 0.f;
	}
	// 착지 순간 가장 세게, 구간 끝으로 갈수록 부드럽게 0. 충격이 약할수록 세게.
	const float TimeWeight = FMath::SmoothStep(0.f, 1.f, 1.f - Elapsed / LandingWindow);
	return TimeWeight * (1.f - LandImpact);
}

void UHBCameraMotionComponent::UpdateDip(const float DeltaTime)
{
	if (FMath::IsNearlyZero(DipOffset, 0.01f) && FMath::IsNearlyZero(DipVelocity, 0.1f))
	{
		DipOffset = 0.f;
		DipVelocity = 0.f;
		return;
	}

	// 반암시적 오일러. 프레임이 길어도 폭주하지 않게 잘게 나눈다.
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / 0.008f), 1, 8);
	const float Dt = DeltaTime / Steps;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float Accel = -LandDipStiffness * DipOffset - LandDipDamping * DipVelocity;
		DipVelocity += Accel * Dt;
		DipOffset += DipVelocity * Dt;
	}
}

float UHBCameraMotionComponent::UpdateAirScale(const APawn* Pawn, const float DeltaTime)
{
	const UWorld* World = GetWorld();
	const UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;
	if (!World || !Movement)
	{
		return 1.f;
	}

	// 공중에 있는 동안만 줄인다. 땅에 닿는 순간부터 고개 회전이 돌아와 착지 덜컥임이 그 자리에서 난다.
	// (착지 후 잠깐 더 기다리면 한참 뒤에 고개가 올라오는 것처럼 보였다.)
	const bool bWantAir = Movement->IsFalling();
	AirBlend = FMath::FInterpTo(AirBlend, bWantAir ? 1.f : 0.f, DeltaTime, AirBlendSpeed);

	// 끝없이 0 에 가까워지기만 하면 "세기 1" 판정에 안 걸려 계속 덮어쓴다. 충분히 작으면 0 으로 끊는다.
	if (!bWantAir && AirBlend < 0.001f)
	{
		AirBlend = 0.f;
	}

	return FMath::Lerp(1.f, AirMotionScale, AirBlend);
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

	// 평소 세기(개발자 값 × 설정 메뉴)는 위치·회전 둘 다에 건다.
	const float Scale = GetEffectiveMotionScale();

	// 공중·착지 직후에는 애니가 만든 고개 회전을 이만큼만 남긴다(0 = 조준 방향만).
	// 늦게 따라가게 하는 방식은 착지 후 쌓인 차이를 따라잡느라 덜컥거려서, 회전을 빼는 방식으로 한다.
	// 위치는 건드리지 않는다. 착지할 때 위아래로 내려앉는 느낌은 남기고, 머리 메시 안쪽이 보이는 일도 없다.
	const float AirRotationKeep = UpdateAirScale(Pawn, DeltaTime);
	UpdateDip(DeltaTime);

	const float LandingSuppression = GetLandingSuppression();

	// 세기 1 은 머리에 그대로 붙은 원래 상태다. 덮어쓰던 게 있으면 원래 상대 위치로 돌려놓는다.
	if (Scale >= 1.f - KINDA_SMALL_NUMBER && AirRotationKeep >= 1.f - KINDA_SMALL_NUMBER && DipOffset == 0.f
		&& LandingSuppression <= 0.f)
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
	FVector FinalLocal = FMath::Lerp(SmoothedLocalLocation, LocalLocation, Scale);
	const FQuat GroundDelta = FQuat::Slerp(SmoothedAimDelta, AimDelta, Scale);

	// 공중이면 고개 회전을 빼고 조준 방향 쪽으로 붙인다. AirBlend 가 부드럽게 바뀌어 들어가고 나올 때 튀지 않는다.
	FQuat FinalDelta = FQuat::Slerp(FQuat::Identity, GroundDelta, AirRotationKeep);

	// 착지 애니(JumpEnd)의 머리 움직임을 충격에 맞춘다. 보통 점프는 거의 지우고, 높은 낙하는 그대로 둔다.
	// 무게감은 아래의 코드 눌림(충격 비례)이 맡는다.
	if (LandingSuppression > 0.f)
	{
		// 위치: 머리가 내려앉는 만큼을 최대 LandingMaxLocationLag 까지 덜어낸다.
		const float RawDz = LocalLocation.Z;
		const float LagDz = FMath::Clamp(SmoothedLocalLocation.Z - RawDz, -LandingMaxLocationLag, LandingMaxLocationLag);
		FinalLocal.Z = RawDz + LagDz * LandingSuppression;

		// 회전: 고개 위아래만 덜어낸다. 좌우·기울기는 그대로.
		FRotator DeltaRot = FinalDelta.Rotator();
		DeltaRot.Pitch *= 1.f - LandingPitchReduce * LandingSuppression;
		FinalDelta = DeltaRot.Quaternion();
	}

	// 착지 눌림. 캐릭터 기준 아래로 내린다.
	FinalLocal.Z += DipOffset;

	Camera->SetWorldLocationAndRotation(ActorTransform.TransformPosition(FinalLocal), AimQuat * FinalDelta);
	bOverriding = true;
}
