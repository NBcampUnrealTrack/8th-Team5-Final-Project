// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InteractionComponent.generated.h"

class APawn;
class UInventoryComponent;

/** 조준 중인 상호작용 대상이 바뀔 때 브로드캐스트 (프롬프트 UI 갱신용, 로컬 전용) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractableFocusChanged, AActor*, NewInteractable);

/** [클라] 서버가 "이 컨테이너 인벤토리로 UI를 열어라"고 지시할 때 브로드캐스트. BP에서 InventoryUI.OpenContainer로 연결. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRequestOpenContainer, UInventoryComponent*, Container);

/**
 *  플레이어 캐릭터에 부착하는 상호작용 컴포넌트.
 *  - 로컬 소유 폰에서 매 틱 전방 트레이스로 IInteractableInterface 대상을 찾는다.
 *  - 대상이 바뀌면 OnFocusChanged로 알려 프롬프트 UI를 갱신한다. (로컬)
 *  - TryInteract() 입력 시 Server RPC로 서버에 요청하고, 서버가 거리/유효성을 재검증한 뒤 Interact()를 호출한다.
 *
 *  캐릭터 Blueprint에 컴포넌트로 추가해 사용한다(BlueprintSpawnableComponent).
 *  입력(예: Enhanced Input IA_Interact)에서 TryInteract()를 호출하도록 바인딩한다.
 */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** [입력] 현재 조준 중인 대상에 상호작용을 요청한다. 로컬에서 호출 → 서버로 전달. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	/** 현재 조준 중인 상호작용 대상 (없으면 nullptr) */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedInteractable() const { return FocusedInteractable; }

	/** 현재 대상의 프롬프트 문구 (대상 없으면 빈 문자열) */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetCurrentPrompt() const;

	/** 조준 대상 변경 알림 (프롬프트 위젯이 바인딩) */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractableFocusChanged OnFocusChanged;

	/** 컨테이너 UI 오픈 요청 (클라). BP에서 이 이벤트를 PlayerController의 InventoryUI.OpenContainer(Container)로 연결한다. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnRequestOpenContainer OnRequestOpenContainer;

	/**
	 *  [Client RPC] 서버가 요청 클라에게 컨테이너 인벤토리로 UI를 열라고 지시.
	 *  이 컴포넌트는 소유 폰(클라 커넥션 소유)에 붙어 있어 Client RPC가 해당 클라로 라우팅된다.
	 *  → OnRequestOpenContainer 브로드캐스트 → (BP) InventoryUI.OpenContainer.
	 */
	UFUNCTION(Client, Reliable)
	void Client_OpenContainer(UInventoryComponent* Container);

protected:

	virtual void BeginPlay() override;

	/** 전방 트레이스 최대 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = 0, Units = "cm"))
	float InteractionDistance = 300.0f;

	/** 서버 거리 재검증 허용 오차 (지연/이동 보정) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = 0, Units = "cm"))
	float ServerDistanceTolerance = 100.0f;

	/** 트레이스에 사용할 콜리전 채널 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 트레이스 디버그 라인 표시 (테스트용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
	bool bDrawDebug = false;

	/** 현재 조준 중인 상호작용 대상 (로컬 전용, 리플리케이트 안 함) */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> FocusedInteractable;

private:

	/** 로컬 소유 폰에서 전방 트레이스로 조준 대상을 갱신한다. */
	void UpdateFocus();

	/** 조준 대상을 교체하고 필요 시 OnFocusChanged를 브로드캐스트한다. */
	void SetFocusedInteractable(AActor* NewInteractable);

	/** 폰/컨트롤러 기준 시점(눈 위치/방향)을 계산한다. */
	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	/** 소유 액터를 폰으로 캐스팅해 반환 (아니면 nullptr) */
	APawn* GetOwnerPawn() const;

	/** [서버] 상호작용 요청 처리: 거리/유효성 재검증 후 Interact() 호출 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact(AActor* Target);
};
