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

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute())
			.AddUObject(this, &AHB_PlayerState::OnManaChanged);

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHB_PlayerAttributeSet::GetPlayer_MaxManaAttribute())
			.AddUObject(this, &AHB_PlayerState::OnMaxManaChanged);
	}

	if (HasAuthority() && Stash)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UStashStorageSubsystem* StorageSubsystem = GameInstance->GetSubsystem<UStashStorageSubsystem>())
			{
				Stash->SetStorage(StorageSubsystem->GetStorage());
				Stash->LoadStash(TEXT("TestAccount"));
			}
		}
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

void AHB_PlayerState::OnManaChanged(const FOnAttributeChangeData& Data)
{
	if (OnMPChanged.IsBound() && AttributeSet)
	{
		OnMPChanged.Broadcast(Data.NewValue, AttributeSet->GetPlayer_MaxMana());
	}
}

void AHB_PlayerState::OnMaxManaChanged(const FOnAttributeChangeData& Data)
{
	if (OnMPChanged.IsBound() && AttributeSet)
	{
		OnMPChanged.Broadcast(AttributeSet->GetPlayer_Mana(), Data.NewValue);
	}
}

void AHB_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHB_PlayerState, PlayerLevel);
	DOREPLIFETIME(AHB_PlayerState, CurrentExperience);
	DOREPLIFETIME(AHB_PlayerState, TeamId);
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
