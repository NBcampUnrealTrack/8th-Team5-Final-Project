#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuickSlotComponent.generated.h"

class UInventoryComponent;

DECLARE_MULTICAST_DELEGATE(FOnQuickSlotChanged);

UCLASS()
class HEAVENBREACH_API UQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuickSlotComponent();

	static constexpr int32 NumQuickSlots = 4;

	FOnQuickSlotChanged OnQuickSlotChanged;

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool SetSlot(int32 InSlotIndex, FGuid InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UseSlot(int32 InSlotIndex);

	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	FGuid GetSlotInstanceId(int32 InSlotIndex) const;

	UFUNCTION(Server, Reliable)
	void Server_SetSlot(int32 InSlotIndex, FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_UseSlot(int32 InSlotIndex);

	const TArray<FGuid>& GetSlots() const { return Slots; }

	void ApplyLoadedSlots(const TArray<FGuid>& InSlots);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FGuid> Slots;

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void HandleInventoryChanged();

	UInventoryComponent* GetOwnerInventory() const;

public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};