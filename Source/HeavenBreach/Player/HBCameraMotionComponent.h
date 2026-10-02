#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HBCameraMotionComponent.generated.h"

class UCameraComponent;

/**
 * 1인칭 카메라 흔들림 세기 조절.
 *
 * 카메라는 머리 뼈에 붙어 있어 애니메이션의 머리 움직임이 100% 화면이 된다.
 * 세기를 낮추면 머리 움직임 중 빠른 흔들림(공격 반동 등)만 걸러내고,
 * 느린 움직임(조준, 포션 마시며 고개 젖히기, 상자 뒤지기)은 그대로 둔다.
 *
 * 조준은 흔들림과 따로 계산한다: 머리 방향을 "조준 방향 + 애니메이션이 만든 차이" 로 나눠
 * 차이만 부드럽게 한다. 그래서 마우스는 세기와 상관없이 늦지 않는다.
 *
 * 최종 세기 = 개발자 기본값(BP) x 플레이어 설정(설정 메뉴의 카메라 움직임).
 * 1 이면 아무것도 안 하고 원래처럼 머리에 붙어 있다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UHBCameraMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHBCameraMotionComponent();

	// 설정 메뉴까지 곱한 최종 세기. 0~1.
	float GetEffectiveMotionScale() const;

	// 착지했을 때 캐릭터가 부른다. 충격(0~1)만큼 카메라가 눌렸다 스프링처럼 돌아온다.
	void NotifyLanded(float Impact);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 개발자 기본 세기. 1 = 머리 움직임 그대로, 0 = 빠른 흔들림 제거.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MotionScale = 1.f;

	// 세기가 낮을 때 남기는 움직임의 빠르기. 클수록 빠른 움직임까지 따라간다.
	// 6 이면 대략 0.2초보다 느린 움직임은 남고 그보다 빠른 흔들림은 줄어든다.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion", meta = (ClampMin = "0.1"))
	float StabilizeSpeed = 6.f;

	// 공중(점프·낙하)에서 애니가 만든 고개 회전을 얼마나 남기나. 0 = 조준 방향만, 1 = 평소와 같음.
	// 점프 애니(3인칭 전신용)의 머리 출렁임이 커서 뺀다. 위치는 건드리지 않는다(늦추면 머리 메시 안쪽이 보인다).
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion|Air", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirMotionScale = 0.f;

	// 공중 보정으로 들어가고 나오는 빠르기. 갑자기 바뀌면 화면이 툭 튄다.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion|Air", meta = (ClampMin = "0.1"))
	float AirBlendSpeed = 8.f;

	// 착지 애니(JumpEnd)의 머리 움직임을 다루는 구간. 착지 순간부터 이 시간 동안 서서히 원래대로 돌아온다.
	// 충격이 약할수록(보통 점프) 애니 움직임을 많이 덜어내고, 충격 1(높은 낙하)이면 애니 그대로 둔다.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion|Landing", meta = (ClampMin = "0.0", Units = "s"))
	float LandingWindow = 0.35f;

	// 충격 0 일 때 애니의 머리 내려앉음을 최대 이만큼 덜어낸다. 크게 하면 머리 메시 안쪽이 보일 수 있다.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion|Landing", meta = (ClampMin = "0.0", Units = "cm"))
	float LandingMaxLocationLag = 5.f;

	// 충격 0 일 때 애니의 고개 위아래(피치)를 이 비율만큼 덜어낸다. 1 = 전부.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion|Landing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LandingPitchReduce = 1.f;

	// 충격 1 일 때 카메라가 눌리는 깊이. 착지 무게감은 애니 대신 이 값이 일정하게 만든다.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion|Landing", meta = (ClampMin = "0.0", Units = "cm"))
	float LandDipMax = 6.f;

	// 눌림 스프링. 강도가 클수록 빨리 돌아오고, 감쇠가 클수록 덜 출렁인다.
	// 120 / 14 면 거의 한 번에 돌아온다(약 0.25초).
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion|Landing", meta = (ClampMin = "1.0"))
	float LandDipStiffness = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Motion|Landing", meta = (ClampMin = "0.0"))
	float LandDipDamping = 14.f;

private:
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	// 카메라가 머리 뼈에 붙은 상대 위치·회전. 흔들림을 줄이는 동안 덮어쓰므로 시작할 때 기억한다.
	FTransform CameraRelativeToHead;

	// 캐릭터 기준 머리 카메라 위치, 조준 방향 기준 머리 카메라 회전 차이의 부드러운 값.
	FVector SmoothedLocalLocation = FVector::ZeroVector;
	FQuat SmoothedAimDelta = FQuat::Identity;
	bool bHasSmoothed = false;

	// 지난 프레임에 덮어썼는지. 1 로 돌아오면 원래 상대 위치로 되돌려 둔다.
	bool bOverriding = false;

	// 공중 세기가 얼마나 걸려 있나. 0 = 평소, 1 = AirMotionScale 그대로.
	float AirBlend = 0.f;

	// 마지막 착지 시각과 충격. 착지 구간에서 애니 움직임을 얼마나 덜어낼지 정한다.
	double LandTime = -1000.0;
	float LandImpact = 1.f;

	// 착지 구간 안이면 0~1(착지 순간 1 → 끝에서 0), 아니면 0. 충격이 셀수록 작아진다.
	float GetLandingSuppression() const;

	// 착지 눌림 스프링 상태. 오프셋은 아래가 음수(cm).
	float DipOffset = 0.f;
	float DipVelocity = 0.f;

	// 스프링을 한 프레임 진행한다. 거의 멈췄으면 0 으로 끊는다.
	void UpdateDip(float DeltaTime);

	// 이번 프레임의 공중 보정 배율(1 ~ AirMotionScale). AirBlend 를 갱신하고 돌려준다.
	float UpdateAirScale(const APawn* Pawn, float DeltaTime);
};
