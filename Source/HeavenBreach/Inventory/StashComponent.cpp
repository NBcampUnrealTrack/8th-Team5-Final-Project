#include "StashComponent.h"

#include "StashStorage.h"

UStashComponent::UStashComponent()
{
	SetGridSize(10, 15);
}

void UStashComponent::SetStorage(TSharedPtr<IStashStorage> InStorage)
{
	Storage = InStorage;
}

void UStashComponent::LoadStash(const FString& InAccountId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (!Storage.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Stash: storage is not set"));
		return;
	}

	AccountId = InAccountId;

	FOnStashLoaded Callback;
	Callback.BindUObject(this, &UStashComponent::HandleLoadCompleted);

	Storage->LoadStashAsync(AccountId, Callback);
}

void UStashComponent::HandleLoadCompleted(bool bSuccess, const TArray<FInventoryEntry>& InEntries)
{
	if (bSuccess)
	{
		SetEntries(InEntries);
		bIsLoaded = true;
	}

	OnStashLoadCompleted.Broadcast(bSuccess);
}

void UStashComponent::SaveStash()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (!Storage.IsValid() || AccountId.IsEmpty())
	{
		return;
	}

	Storage->SaveStashAsync(AccountId, GetEntries(), FOnStashSaved());
}

void UStashComponent::RequestSave_Implementation()
{
	SaveStash();
}
