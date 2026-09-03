#include "StashComponent.h"
#include "TimerManager.h"
#include "StashStorage.h"

UStashComponent::UStashComponent()
{
	SetGridSize(StashGridSize.X, StashGridSize.Y);
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

void UStashComponent::HandleStashChanged()
{
	if (GetOwnerRole() != ROLE_Authority || !bIsLoaded)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(StashSaveTimer);
	World->GetTimerManager().SetTimer(
		StashSaveTimer, this, &UStashComponent::SaveStash, 0.2f, false);
}

void UStashComponent::BeginPlay()
{
	Super::BeginPlay();

	SetGridSize(StashGridSize.X, StashGridSize.Y);

	if (GetOwnerRole() == ROLE_Authority)
	{
		OnInventoryChanged.AddUniqueDynamic(this, &UStashComponent::HandleStashChanged);
	}
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
