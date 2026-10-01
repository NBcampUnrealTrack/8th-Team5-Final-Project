// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InteractionComponent.generated.h"

class APawn;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableFocusChanged, AActor*, NewInteractable);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRequestOpenContainer, UInventoryComponent*, Container);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 단발 상호작용. 홀드가 필요 없는 대상(홀드 시간 0)에 쓴다.
	 * 플레이어 BP 에서 홀드 방식으로 전환했다면 BeginHold/EndHold 를 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	/**
	 * 상호작용 키를 누른 순간 호출한다 (Enhanced Input 의 Started).
	 *
	 * 대상의 홀드 시간이 0 이하이면 그 자리에서 TryInteract 와 동일하게 동작하므로,
	 * 상자처럼 즉시 반응해야 하는 대상도 이 하나로 처리된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void BeginHold();

	/** 상호작용 키를 뗀 순간 호출한다 (Enhanced Input 의 Completed / Canceled). */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void EndHold();

	/**
	 * 상호작용하던 쪽이 더 이상 행동할 수 없을 때(사망 등) 홀드를 즉시 끊는다.
	 * 서버에서는 대상에 OnHoldCancel 을 알리고, 로컬에서는 진행 바·뒤지는 동작을 끈다.
	 * EndHold 와 달리 서버 RPC 를 보내지 않는다. 빙의가 풀린 폰에서도 부를 수 있게 하기 위해서다.
	 */
	void AbortHold();

	/** 홀드 진행률 0~1. 진행 중이 아니면 0. 프로그레스 바용 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetHoldProgress() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsHolding() const { return HeldTarget != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedInteractable() const { return FocusedInteractable; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetCurrentPrompt() const;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractableFocusChanged OnFocusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnRequestOpenContainer OnRequestOpenContainer;

	UFUNCTION(Client, Reliable)
	void Client_OpenContainer(UInventoryComponent* Container);

protected:

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = 0, Units = "cm"))
	float InteractionDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = 0, Units = "cm"))
	float ServerDistanceTolerance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * UI가 입력을 독점하는 동안(ECommonInputMode::Menu) 상호작용 탐색을 멈춘다.
	 *
	 * 끄면 상자/인벤토리를 열어둔 상태에서도 시선이 대상에 남아 있는 한 포커스가 유지되고,
	 * 그 결과 상호작용 프롬프트("F 상자 오픈")가 UI 위에 계속 표시된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bSuppressWhileMenuInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
	bool bDrawDebug = false;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> FocusedInteractable;

private:

	void UpdateFocus();

	void SetFocusedInteractable(AActor* NewInteractable);

	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	/** 현재 로컬 플레이어의 입력이 UI(Menu 모드)에 넘어가 있는지 */
	bool IsInputOwnedByMenu() const;

	APawn* GetOwnerPawn() const;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact(AActor* Target);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_BeginHold(AActor* Target);

	UFUNCTION(Server, Reliable)
	void Server_EndHold();

	/** 서버 전용. 홀드 시간이 다 찼을 때 */
	void CompleteHold();

	/** 서버 전용. 홀드를 끊고 대상에 OnHoldCancel 을 알린다 */
	void CancelHoldOnServer();

	/** 대상이 여전히 상호작용 가능한 거리·상태인지. 홀드 시작과 진행 중 검증에 함께 쓴다 */
	bool IsTargetInteractable(AActor* Target) const;

	/** 로컬(클라이언트) 홀드 대상. UI 진행률 표시와 포커스 변화 시 취소 판정에 쓴다 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> HeldTarget;

	/** 로컬 홀드 시작 시각과 총 소요 시간. 진행률 계산용 */
	float HoldStartTime = 0.0f;
	float HoldDuration = 0.0f;

	/** 서버 홀드 대상. 클라이언트의 로컬 상태와 별개로 서버가 권한을 갖는다 */
	UPROPERTY()
	TObjectPtr<AActor> ServerHeldTarget;

	FTimerHandle ServerHoldTimerHandle;
};
