#include "HB_PlayerState.h"

#include "Inventory/StashComponent.h"
#include "Core/DataManagement/StashStorageSubsystem.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "InventoryComponent.h"
#include "EquipmentComponent.h"
#include "Inventory/QuickSlotComponent.h"
#include "Shop/ShopComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DataTable.h"
#include "Core/DataManagement/LoadoutStorage.h"
#include "Core/DataManagement/LoadoutStorageSubsystem.h"
#include "Inventory/QuickSlotComponent.h"
#include "TimerManager.h"

AHB_PlayerState::AHB_PlayerState()
{
#pragma region NetworkSettings
	bReplicates = true;
#pragma endregion 
	
#pragma region AbilitySystemComponent
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
#pragma endregion 
	
#pragma region AttributeSet
	AttributeSet = CreateDefaultSubobject<UHB_PlayerAttributeSet>(TEXT("AttributeSet"));
#pragma endregion

#pragma region Stash
	Stash = CreateDefaultSubobject<UStashComponent>(TEXT("Stash"));
#pragma endregion

#pragma region Inventory
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Equipment = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
	QuickSlot = CreateDefaultSubobject<UQuickSlotComponent>(TEXT("QuickSlot"));
#pragma endregion

#pragma region Shop
	Shop = CreateDefaultSubobject<UShopComponent>(TEXT("Shop"));

	static ConstructorHelpers::FObjectFinder<UDataTable> ShopTableFinder(TEXT("/Game/Shop/DT_ShopList.DT_ShopList"));
	if (ShopTableFinder.Succeeded())
	{
		Shop->SetShopInventoryTable(ShopTableFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> ItemTableFinder(TEXT("/Game/Items/DataTables/DT_Item.DT_Item"));
	if (ItemTableFinder.Succeeded())
	{
		FDataTableRowHandle GoldRow;
		GoldRow.DataTable = ItemTableFinder.Object;
		GoldRow.RowName = TEXT("ITM_002");
		Shop->SetGoldItemRow(GoldRow);
	}
#pragma endregion
}

void AHB_PlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ResetMatchProgression();
	}

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHB_PlayerAttributeSet::GetPlayer_HPAttribute())
			.AddUObject(this, &AHB_PlayerState::OnHealthChanged);

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute())
			.AddUObject(this, &AHB_PlayerState::OnMaxHealthChanged);
	}

	if (HasAuthority() && Stash)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UStashStorageSubsystem* StorageSubsystem = GameInstance->GetSubsystem<UStashStorageSubsystem>())
			{
				Stash->SetStorage(StorageSubsystem->GetStorage());
				Stash->LoadStash(TEXT("TestAccount"));
				Stash->OnStashLoadCompleted.AddUniqueDynamic(this, &AHB_PlayerState::HandleStashLoadCompleted);
			}
		}
	}

	if (HasAuthority())
	{
		LoadLoadout();
	}
}

void AHB_PlayerState::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (OnHPChanged.IsBound() && AttributeSet)
	{
		OnHPChanged.Broadcast(Data.NewValue, AttributeSet->GetPlayer_MaxHP());
	}
}

void AHB_PlayerState::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	if (OnHPChanged.IsBound() && AttributeSet)
	{
		OnHPChanged.Broadcast(AttributeSet->GetPlayer_HP(), Data.NewValue);
	}
}

void AHB_PlayerState::SaveLoadout()
{
	if (!HasAuthority() || !LoadoutStorage.IsValid())
	{
		return;
	}

	FPlayerLoadout Loadout;

	if (Inventory)
	{
		Loadout.InventoryEntries = Inventory->GetEntries();
	}

	if (Equipment)
	{
		Loadout.EquipmentSlots = Equipment->GetSlots();
	}

	if (QuickSlot)
	{
		Loadout.QuickSlots = QuickSlot->GetSlots();
	}

	LoadoutStorage->SaveLoadoutAsync(LoadoutAccountId, Loadout, FOnLoadoutSaved());
}

void AHB_PlayerState::LoadLoadout()
{
	if (!HasAuthority())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	ULoadoutStorageSubsystem* StorageSubsystem = GameInstance->GetSubsystem<ULoadoutStorageSubsystem>();
	if (!StorageSubsystem)
	{
		return;
	}

	LoadoutStorage = StorageSubsystem->GetStorage();
	if (!LoadoutStorage.IsValid())
	{
		return;
	}

	LoadoutAccountId = TEXT("TestAccount");

	FOnLoadoutLoaded Callback;
	Callback.BindUObject(this, &AHB_PlayerState::HandleLoadoutLoaded);
	LoadoutStorage->LoadLoadoutAsync(LoadoutAccountId, Callback);
}

void AHB_PlayerState::HandleLoadoutLoaded(bool bSuccess, const FPlayerLoadout& InLoadout)
{
	if (!bSuccess || !HasAuthority())
	{
		return;
	}

	if (Inventory)
	{
		Inventory->ClearAll();

		for (const FInventoryEntry& Entry : InLoadout.InventoryEntries)
		{
			Inventory->TryAddItemAt(Entry.Item, Entry.Position);
		}

		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &AHB_PlayerState::RequestSaveLoadout);
	}

	if (Equipment)
	{
		Equipment->ApplyLoadedSlots(InLoadout.EquipmentSlots);

		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &AHB_PlayerState::RequestSaveLoadout);
	}

	if (QuickSlot)
	{
		QuickSlot->ApplyLoadedSlots(InLoadout.QuickSlots);

		QuickSlot->OnQuickSlotChanged.AddUObject(this, &AHB_PlayerState::HandleQuickSlotChanged);
	}

	UE_LOG(LogTemp, Log, TEXT("Loadout: applied inv=%d"), InLoadout.InventoryEntries.Num());
}

void AHB_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHB_PlayerState, PlayerLevel);
	DOREPLIFETIME(AHB_PlayerState, CurrentExperience);
	DOREPLIFETIME(AHB_PlayerState, TeamId);
}

void AHB_PlayerState::RequestSaveLoadout()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(LoadoutSaveTimer);
	World->GetTimerManager().SetTimer(
		LoadoutSaveTimer, this, &AHB_PlayerState::SaveLoadout, 0.6f, false);
}

void AHB_PlayerState::HandleQuickSlotChanged()
{
	RequestSaveLoadout();
}

void AHB_PlayerState::HandleStashLoadCompleted(bool bSuccess)
{
	if (!bSuccess || !HasAuthority() || !bGrantTestGold || !Stash)
	{
		return;
	}

	UShopComponent* ShopComp = Shop;
	if (!ShopComp)
	{
		return;
	}

	const FDataTableRowHandle GoldRow = ShopComp->GetGoldItemRow();
	if (GoldRow.IsNull())
	{
		return;
	}

	const int32 CurrentGold = Stash->GetTotalGold(GoldRow.RowName);
	if (CurrentGold >= TestGoldMinimum)
	{
		return;
	}

	const int32 GrantAmount = TestGoldMinimum - CurrentGold;
	if (Stash->AddGold(GoldRow, GrantAmount))
	{
		Stash->SaveStash();
		UE_LOG(LogTemp, Log, TEXT("Stash: granted test gold %d (was %d)"), GrantAmount, CurrentGold);
	}
}

void AHB_PlayerState::AddExperience(const int32 Amount)
{
	if (!HasAuthority() || Amount <= 0 || PlayerLevel >= MaxPlayerLevel)
	{
		return;
	}

	CurrentExperience += Amount;
	while (PlayerLevel < MaxPlayerLevel)
	{
		const int32 RequiredExperience = GetExperienceToNextLevel();
		if (CurrentExperience < RequiredExperience)
		{
			break;
		}

		CurrentExperience -= RequiredExperience;
		++PlayerLevel;
	}

	if (PlayerLevel >= MaxPlayerLevel)
	{
		CurrentExperience = 0;
	}

	BroadcastProgression();
	ForceNetUpdate();
}

void AHB_PlayerState::ResetMatchProgression()
{
	if (!HasAuthority())
	{
		return;
	}

	PlayerLevel = 1;
	CurrentExperience = 0;
	BroadcastProgression();
	ForceNetUpdate();
}

int32 AHB_PlayerState::GetExperienceToNextLevel() const
{
	if (PlayerLevel >= MaxPlayerLevel)
	{
		return 0;
	}

	return BaseExperienceToNextLevel + (PlayerLevel - 1) * ExperienceIncreasePerLevel;
}

float AHB_PlayerState::GetExperiencePercent() const
{
	const int32 RequiredExperience = GetExperienceToNextLevel();
	return RequiredExperience > 0
		? FMath::Clamp(static_cast<float>(CurrentExperience) / static_cast<float>(RequiredExperience), 0.0f, 1.0f)
		: 1.0f;
}

void AHB_PlayerState::OnRep_PlayerLevel()
{
	BroadcastProgression();
}

void AHB_PlayerState::OnRep_CurrentExperience()
{
	BroadcastProgression();
}

void AHB_PlayerState::BroadcastProgression()
{
	OnPlayerLevelChanged.Broadcast(PlayerLevel);
	OnPlayerExperienceChanged.Broadcast(CurrentExperience, GetExperienceToNextLevel(), GetExperiencePercent());
}

void AHB_PlayerState::SetTeamId(const int32 NewTeamId)
{
	if (!HasAuthority() || TeamId == NewTeamId)
	{
		return;
	}

	TeamId = NewTeamId;
	OnTeamIdChanged.Broadcast(TeamId);
	ForceNetUpdate();
}

bool AHB_PlayerState::IsSameTeam(const AHB_PlayerState* OtherPlayerState) const
{
	return OtherPlayerState
		&& HasValidTeam()
		&& OtherPlayerState->HasValidTeam()
		&& TeamId == OtherPlayerState->TeamId;
}

void AHB_PlayerState::OnRep_TeamId()
{
	OnTeamIdChanged.Broadcast(TeamId);
}
