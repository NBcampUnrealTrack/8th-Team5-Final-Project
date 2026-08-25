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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
	bool bDrawDebug = false;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> FocusedInteractable;

private:

	void UpdateFocus();

	void SetFocusedInteractable(AActor* NewInteractable);

	bool GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	APawn* GetOwnerPawn() const;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact(AActor* Target);
};
