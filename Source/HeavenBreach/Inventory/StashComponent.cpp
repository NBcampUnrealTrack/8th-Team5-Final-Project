#include "StashComponent.h"
#include "GameFramework/PlayerState.h"
#include "OnlineSubsystem.h"
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

	PendingRequestedAccountId = InAccountId;
	IdentityRetryCount = 0;
	TryLoadPendingStash();
}

void UStashComponent::TryLoadPendingStash()
{
	if (GetOwnerRole() != ROLE_Authority || !Storage.IsValid())
	{
		return;
	}

	FString ResolvedAccountId;
	if (!ResolveServerAccountId(
		PendingRequestedAccountId,
		ResolvedAccountId))
	{
		// PlayerState BeginPlay can precede RegisterPlayer assigning UniqueId.
		// Wait briefly rather than loading every Steam user into TestAccount.
		constexpr int32 MaxIdentityRetries = 40;
		constexpr float IdentityRetryDelaySeconds = 0.5f;
		++IdentityRetryCount;

		if (IdentityRetryCount < MaxIdentityRetries)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					StashIdentityRetryTimer,
					this,
					&UStashComponent::TryLoadPendingStash,
					IdentityRetryDelaySeconds,
					false
					);
			}
			return;
		}

		UE_LOG(LogTemp, Error,
			TEXT("Stash: authenticated Steam identity unavailable; load skipped"));
		OnStashLoadCompleted.Broadcast(false);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StashIdentityRetryTimer);
	}

	AccountId = MoveTemp(ResolvedAccountId);
	PendingRequestedAccountId.Reset();

	FOnStashLoaded Callback;
	Callback.BindUObject(this, &UStashComponent::HandleLoadCompleted);

	// AccountId came from the authoritative PlayerState when Steam is active;
	// a client never supplies the identity used for persistent storage.
	Storage->LoadStashAsync(AccountId, Callback);
}

bool UStashComponent::ResolveServerAccountId(
	const FString& RequestedAccountId,
	FString& OutAccountId) const
{
	OutAccountId.Reset();

	const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	const bool bSteamIsActive = OnlineSubsystem != nullptr &&
		OnlineSubsystem->GetSubsystemName() == FName(TEXT("STEAM"));

	if (bSteamIsActive)
	{
		const APlayerState* PlayerState = Cast<APlayerState>(GetOwner());
		if (!IsValid(PlayerState))
		{
			return false;
		}

		const FUniqueNetIdRepl& UniqueNetId = PlayerState->GetUniqueId();
		if (!UniqueNetId.IsValid() ||
			UniqueNetId.GetType() != FName(TEXT("STEAM")))
		{
			return false;
		}

		OutAccountId = UniqueNetId.ToString();
		return !OutAccountId.IsEmpty();
	}

#if UE_BUILD_SHIPPING
	// A production build must not silently collapse users into the shared
	// development record if Steam initialization fails.
	return false;
#else
	// Keep the pre-existing TestAccount workflow for offline editor testing;
	// this branch is unreachable when Steam is the active subsystem.
	OutAccountId = RequestedAccountId;
	return !OutAccountId.IsEmpty();
#endif
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
		// 각인 입구 (불러오기). 각인이 생기기 전에 저장된 장비는 여기서 한 번 발린다.
		// SetEntries 도 바르지만, 새로 발린 게 있는지 알아야 저장을 요청할 수 있어서 먼저 바른다.
		TArray<FInventoryEntry> Loaded = InEntries;
		bool bNewlyEngraved = false;
		for (FInventoryEntry& Entry : Loaded)
		{
			if (EnsureEngraved(Entry.Item))
			{
				bNewlyEngraved = true;
			}
		}

		SetEntries(Loaded);
		bIsLoaded = true;

		// SetEntries 의 변경 알림은 bIsLoaded 전이라 저장이 안 나간다.
		// 새로 발린 게 있으면 여기서 저장한다 — 안 하면 다음 접속 때 다시 굴려져 값이 바뀐다.
		if (bNewlyEngraved)
		{
			HandleStashChanged();
		}
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

	if (bStashSaveInFlight)
	{	
		bStashSavePending = true;
		return;
	}

	bStashSaveInFlight = true;
	bStashSavePending = false;

	FOnStashSaved Callback;
	Callback.BindUObject(this, &UStashComponent::HandleStashSaved);
	Storage->SaveStashAsync(AccountId, GetEntries(), Callback);
}

void UStashComponent::HandleStashSaved(bool bSuccess)
{
	bStashSaveInFlight = false;

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stash: save failed"));
	}

	if (bStashSavePending)
	{
		bStashSavePending = false;
		SaveStash();
	}
}

void UStashComponent::RequestSave_Implementation()
{
	SaveStash();
}
