#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HBFootstepComponent.generated.h"

class USoundBase;

/**
 * 걷기·달리기 발소리. 땅 위에서 움직인 거리가 보폭만큼 쌓일 때마다 한 번 낸다.
 *
 * 왜 거리 기준인가: 애니메이션마다 노티파이를 심지 않아도 되고, 걷기·달리기가 섞이는
 * 블렌드스페이스에서도 바로 된다. 대신 발이 땅에 닿는 순간과 정확히 맞지는 않는다.
 * 노티파이로 바꾸려면 bUseDistanceSteps 를 끄고 애니메이션 노티파이에서 PlayFootstep 을 부르면 된다.
 *
 * 멀티: 각 화면이 복제된 이동으로 스스로 계산한다. 통신이 없다. 전용 서버에서는 돌지 않는다.
 * 사운드 칸이 비어 있으면 아무 소리도 안 난다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UHBFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHBFootstepComponent();

	// 발소리 한 번. 거리 기준 모드와 애니메이션 노티파이 모두 이 함수를 쓴다.
	UFUNCTION(BlueprintCallable, Category = "Footstep")
	void PlayFootstep(bool bSprinting);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	TObjectPtr<USoundBase> WalkSound;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	TObjectPtr<USoundBase> SprintSound;

	// 발소리 사이 거리(cm). 걷기 300cm/s 면 초당 약 3.5번.
	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "10.0", Units = "cm"))
	float WalkStepDistance = 85.f;

	// 달리기 600cm/s 면 초당 약 4번.
	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "10.0", Units = "cm"))
	float SprintStepDistance = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "0.0"))
	float WalkVolume = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "0.0"))
	float SprintVolume = 1.f;

	// 웅크려 걸을 때 걷기 소리에 곱한다.
	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchVolumeScale = 0.4f;

	// 이보다 느리면 서 있는 것으로 본다. 제자리에서 미끄러지는 관성 끝부분에 소리가 나지 않게 한다.
	UPROPERTY(EditDefaultsOnly, Category = "Footstep", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinStepSpeed = 60.f;

	// 끄면 거리로 세지 않는다. 애니메이션 노티파이에서 PlayFootstep 을 부를 때 쓴다.
	UPROPERTY(EditDefaultsOnly, Category = "Footstep")
	bool bUseDistanceSteps = true;

private:
	float DistanceSinceStep = 0.f;
	bool bWasMoving = false;
};
