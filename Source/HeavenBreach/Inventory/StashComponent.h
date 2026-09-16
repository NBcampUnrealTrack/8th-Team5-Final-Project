#pragma once

#include "CoreMinimal.h"
#include "InventoryComponent.h"
#include "StashComponent.generated.h"

class IStashStorage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStashLoadCompleted, bool, bSuccess);

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UStashComponent : public UInventoryComponent
{
	GENERATED_BODY()

public:
	UStashComponent();

	UPROPERTY(BlueprintAssignable, Category = "Stash")
	FOnStashLoadCompleted OnStashLoadCompleted;

	void SetStorage(TSharedPtr<IStashStorage> InStorage);

	UFUNCTION(BlueprintCallable, Category = "Stash")
	void LoadStash(const FString& InAccountId);

	UFUNCTION(BlueprintCallable, Category = "Stash")
	void SaveStash();

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void RequestSave();

	UFUNCTION(BlueprintPure, Category = "Stash")
	bool IsLoaded() const { return bIsLoaded; }

	UFUNCTION()
	void HandleStashChanged();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Stash")
	FIntPoint StashGridSize = FIntPoint(10, 35);

private:
	void TryLoadPendingStash();
	bool ResolveServerAccountId(
		const FString& RequestedAccountId,
		FString& OutAccountId
		) const;
	void HandleLoadCompleted(bool bSuccess, const TArray<FInventoryEntry>& InEntries);

	TSharedPtr<IStashStorage> Storage;

	UPROPERTY()
	FString AccountId;

	/** Keeps the existing LoadStash API while waiting for PlayerState's server-assigned Steam identity. */
	FString PendingRequestedAccountId;
	int32 IdentityRetryCount = 0;

	bool bIsLoaded = false;

	FTimerHandle StashSaveTimer;
	FTimerHandle StashIdentityRetryTimer;

	void HandleStashSaved(bool bSuccess);
	bool bStashSaveInFlight = false;
	bool bStashSavePending = false;
};
