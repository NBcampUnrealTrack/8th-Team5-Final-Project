#pragma once

#include "CoreMinimal.h"
#include "Door/DoorActor.h"
#include "HBBossDoor.generated.h"

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBBossDoor : public ADoorActor
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Boss|Door")
	void SetDoorState(bool bOpen, bool bLock, bool bInstant);

	UFUNCTION(BlueprintPure, Category = "Boss|Door")
	bool IsLocked() const { return bLocked; }

protected:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Door")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Door")
	FText LockedPrompt = NSLOCTEXT("BossDoor", "LockedPrompt", "잠김");
};
