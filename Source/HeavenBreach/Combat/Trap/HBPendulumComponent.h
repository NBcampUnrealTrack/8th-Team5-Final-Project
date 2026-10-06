#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HBPendulumComponent.generated.h"

class USoundBase;

// 어느 축으로 흔드나. 이 컴포넌트 자신의 축 기준이다.
UENUM(BlueprintType)
enum class EHBSwingAxis : uint8
{
	X UMETA(DisplayName = "X축 (Roll)"),
	Y UMETA(DisplayName = "Y축 (Pitch)"),
	Z UMETA(DisplayName = "Z축 (Yaw) · 수평으로 휩쓸기"),
};

/**
 * 진자처럼 좌우로 흔들리는 축. 트랩 BP 에 추가하고, 흔들 것(도끼 메쉬 · 판정 박스)을 이 아래에 붙인다.
 *
 * 왜 컴포넌트인가: 이벤트 그래프 없이 "추가 + 값 입력" 으로 끝내려고. 움직이는 트랩이 늘어도 BP 작업이 늘지 않는다.
 *
 * 작동방식: 매 틱 서버 시간으로 각도를 계산해 자신을 회전시킨다. 모든 화면이 같은 서버 시간을 보므로 같은 각도가 나오고,
 * 판정(서버)과 화면이 어긋나지 않는다. 배치한 회전을 기준으로 그 위에 흔들림을 더한다.
 *
 * 쳐낼 조건: 트랩이 켜졌을 때만 흔들어야 하는 종류가 생기면 AHBTrapBase 단계와 묶는 옵션을 추가한다.
 */
UCLASS(ClassGroup = (Trap), meta = (BlueprintSpawnableComponent, DisplayName = "HB Pendulum"))
class HEAVENBREACH_API UHBPendulumComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHBPendulumComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	// 가운데에서 한쪽으로 기우는 최대 각도.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pendulum", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SwingAngle = 60.f;

	// 한 번 왕복하는 시간(초).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pendulum", meta = (ClampMin = "0.1"))
	float SwingPeriod = 2.5f;

	// 여러 개를 나란히 둘 때 서로 엇갈리게 하는 값. 0.5 면 반대쪽에서 시작한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pendulum", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhaseOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pendulum")
	EHBSwingAxis SwingAxis = EHBSwingAxis::X;

	// ----- 소리 -----

	// 날이 바닥(가장 빠른 지점)을 지날 때마다 낸다. 왕복 한 번에 두 번. 비우면 소리 없음.
	// 거리 감쇠는 사운드 애셋의 Attenuation 설정을 쓴다 (발소리와 같다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pendulum|Sound")
	TObjectPtr<USoundBase> SwingSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pendulum|Sound",
		meta = (ClampMin = "0.0", EditCondition = "SwingSound != nullptr"))
	float SwingVolume = 1.f;

	// 바닥을 지나는 순간보다 몇 초 일찍 소리를 내나. 양수면 일찍, 음수면 지나간 뒤에 낸다.
	// 휙 소리의 가장 큰 부분이 날이 지나갈 때와 맞게 앞뒤로 민다. 반 왕복(SwingPeriod / 2)을 넘기면 다음 통과와 겹친다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pendulum|Sound",
		meta = (ClampMin = "-1.0", ClampMax = "1.0", EditCondition = "SwingSound != nullptr"))
	float SwingSoundLead = 0.f;

	// 소리가 나는 자리. 이 컴포넌트(축) 기준. 기본은 축 아래 2m — 날이 있는 쪽.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pendulum|Sound",
		meta = (EditCondition = "SwingSound != nullptr"))
	FVector SwingSoundOffset = FVector(0.f, 0.f, -200.f);

private:
	FVector GetLocalAxis() const;

	void UpdateSwingSound(double ServerTime);

	// 바닥을 지났는지는 흔들림 방향(부호)이 바뀌는 것으로 안다. 처음 한 번은 기준만 잡는다.
	bool bHasLastSwingSide = false;
	bool bLastSwingSidePositive = false;

	// 배치한 회전. 흔들림은 이 위에 더한다.
	FQuat BaseRelativeRotation = FQuat::Identity;
};
