// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, meta = (DisplayName = "Interactable Interface"))
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class HEAVENBREACH_API IInteractableInterface
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractPrompt() const;

	/**
	 * 상호작용을 완료하려면 몇 초 동안 키를 누르고 있어야 하는지.
	 *
	 * 0 이하이면 기존처럼 한 번 누르는 즉시 Interact 가 실행된다 — 상자·탈출 지점·시체 등
	 * 기존 구현체는 이 함수를 재정의하지 않으므로 동작이 바뀌지 않는다.
	 *
	 * 0 보다 크면 UInteractionComponent 가 홀드를 관리한다:
	 * OnHoldBegin → (시간 경과) → Interact, 또는 중간에 놓으면 OnHoldCancel.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	float GetInteractHoldSeconds() const;
	virtual float GetInteractHoldSeconds_Implementation() const { return 0.0f; }

	/**
	 * 홀드가 시작됐다. 서버에서만 호출된다. 진행 중임을 다른 플레이어에게 보여줄 상태를 여기서 켠다.
	 *
	 * false 를 돌려주면 홀드를 시작하지 않는다 — 조건을 만족하지 못해 시작 자체를 막고 싶을 때
	 * (예: 봉인문이 필요한 열쇠를 갖고 있지 않은 플레이어를 거절할 때) 쓴다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool OnHoldBegin(AActor* Interactor);
	virtual bool OnHoldBegin_Implementation(AActor* Interactor) { return true; }

	/** 홀드가 완료되지 못하고 끊겼다(키를 놓음 / 시선 이탈 / 거리 이탈 / 사망). 서버에서만 호출된다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnHoldCancel(AActor* Interactor);
	virtual void OnHoldCancel_Implementation(AActor* Interactor) {}
};
