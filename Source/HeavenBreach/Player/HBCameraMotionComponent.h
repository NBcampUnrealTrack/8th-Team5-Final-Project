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
};
