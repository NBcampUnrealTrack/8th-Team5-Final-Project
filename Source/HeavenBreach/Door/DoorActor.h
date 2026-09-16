// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "DoorActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;

/**
 * 눌러서 여닫는 일반 쌍문.
 *
 * 열쇠도 등급도 없고 방 내용물도 만들지 않는다 — 상호작용 키를 한 번 누를 때마다
 * 열림/닫힘이 번갈아 바뀌는 통로용 문이다. 등급·열쇠·보상방이 필요하면
 * ASealedDoorActor 를 쓴다.
 *
 * 문짝이 도는 연출은 C++ 이 하지 않는다. BP_OnDoorStateChanged 에서 Timeline 으로
 * 처리하고, 여기서는 상태·복제·콜리전만 책임진다 (봉인문과 같은 분담).
 */
UCLASS(Blueprintable)
class HEAVENBREACH_API ADoorActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:

	ADoorActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** 홀드는 쓰지 않는다. GetInteractHoldSeconds 를 재정의하지 않으므로 한 번 누르면 즉시 토글된다 */

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bIsOpen; }

	/** 문짝이 도는 중인지. 이 동안에는 상호작용을 받지 않는다 */
	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsSwinging() const;

	UFUNCTION(BlueprintPure, Category = "Door|Visual")
	float GetSwingDurationSeconds() const { return SwingDurationSeconds; }

	UFUNCTION(BlueprintPure, Category = "Door|Visual")
	float GetOpenYawLeft() const { return OpenYawLeft; }

	UFUNCTION(BlueprintPure, Category = "Door|Visual")
	float GetOpenYawRight() const { return OpenYawRight; }

	/**
	 * 두 문짝을 지정한 상태의 각도로 즉시 세팅한다. 연출 없이 결과 자세만 맞춘다.
	 *
	 * 뒤늦게 이 문이 relevant 해진 클라이언트를 위해 C++ 이 이미 호출해 주므로
	 * (BP_OnDoorStateChanged 의 bInstant 가 true 인 경우), BP 에서 따로 부를 필요는 없다.
	 * Timeline 을 중간에 끊고 자세를 맞춰야 하는 경우를 위해 열어둔다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Door|Visual")
	void ApplyAngleInstant(bool bOpen);

protected:

	/** 한 번도 상태가 바뀐 적 없음을 나타내는 시각. 실제 서버 시각과 겹치지 않을 만큼 과거로 잡는다 */
	static constexpr float NeverChangedServerTime = -1000.0f;

	/** GetTimeSinceStateChange 가 "아주 오래됨"을 뜻할 때 돌려주는 값 */
	static constexpr float LongAgoSeconds = 1.0e6f;

	/** 문틀. 루트이며 움직이지 않는다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorFrame;

	/** 왼쪽 경첩. 회전축이다. 문짝 메시의 피벗이 중앙에 있어도 이 컴포넌트를 돌리면 제대로 열린다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HingeLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorMeshLeft;

	/** 오른쪽 경첩 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HingeRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorMeshRight;

	/** 왼쪽 문짝이 열렸을 때의 Yaw. 문틀 안쪽에 걸리면 더 크게 준다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Visual")
	float OpenYawLeft = -80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Visual")
	float OpenYawRight = 80.0f;

	/**
	 * 문짝이 열리거나 닫히는 데 걸리는 시간(초). BP 의 Timeline 길이를 여기에 맞춘다.
	 *
	 * 세 가지를 동시에 결정한다:
	 *  - 연출 중 재입력을 막는 잠금 시간 (IsSwinging)
	 *  - 닫을 때 문짝 콜리전을 되살리는 시점
	 *  - 뒤늦게 이 문을 보게 된 클라이언트가 연출을 재생할지, 결과 자세만 적용할지의 판정 기준
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Visual", meta = (ClampMin = 0, Units = "s"))
	float SwingDurationSeconds = 1.0f;

	/** 프롬프트에 쓰는 문 이름. 예: "창고 문" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FText DoorDisplayName;

	UPROPERTY(ReplicatedUsing = OnRep_IsOpen, VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	bool bIsOpen = false;

	/**
	 * 상태가 마지막으로 바뀐 서버 시각(AGameStateBase::GetServerWorldTimeSeconds() 기준).
	 * bIsOpen 과 같은 리플리케이션 묶음에 실려 전달되도록 상태 대입 직전에 설정한다.
	 *
	 * 기본값이 NeverChangedServerTime 인 이유: 0 으로 두면 레벨이 막 시작된 시점
	 * (서버 시각도 0 근처)에 "방금 막 움직였다"로 오판해서, 아직 아무도 건드리지 않은 문이
	 * SwingDurationSeconds 동안 상호작용을 거부한다.
	 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	float StateChangeServerTime = NeverChangedServerTime;

	UFUNCTION()
	void OnRep_IsOpen();

	/**
	 * 연출 훅. 문이 열리거나 닫힐 때, 그리고 이미 열려 있던 문이 뒤늦게 relevant 해졌을 때
	 * 호출된다.
	 *
	 *  - bInstant = false : 방금 움직이기 시작했다. 문짝이 도는 Timeline 을 재생한다.
	 *  - bInstant = true  : 이 상태로 있은 지 오래된 문을 지금 처음 본다. C++ 이 이미
	 *                       결과 각도를 맞춰 두었으므로 연출을 재생하면 안 된다. 한참 전에
	 *                       열린 문이 눈앞에서 다시 열리는 것처럼 보인다. 사운드도 마찬가지다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Door", meta = (DisplayName = "On Door State Changed"))
	void BP_OnDoorStateChanged(bool bOpen, bool bInstant);

	/**
	 * 문짝 콜리전을 현재 상태에 맞춘다.
	 *
	 * 열 때는 즉시 푼다 — 다 열린 뒤에 풀면 이미 열려 보이는 문에서 잠깐 막힌다.
	 * 닫을 때는 반대로 문짝이 다 닫힌 뒤에 되살린다. 닫히기 시작하자마자 막으면
	 * 문틀을 지나가던 플레이어가 아직 열려 있는 문에 끼인다.
	 */
	void ApplyCollisionForState(bool bInstant);

	UFUNCTION()
	void RestoreDoorCollision();

	/** 마지막 상태 변경 이후 흐른 시간(초). 판정할 수 없거나 한 번도 안 바뀌었으면 아주 큰 값 */
	float GetTimeSinceStateChange() const;

	/** 상태가 SwingDurationSeconds 이내에 바뀌었는지. BP_OnDoorStateChanged 의 bInstant 판정 */
	bool WasJustChanged() const;

	FTimerHandle CollisionRestoreTimer;
};
