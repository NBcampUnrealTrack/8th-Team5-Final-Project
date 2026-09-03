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

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

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
};
