#include "HB_PlayerState.h"

#include "Inventory/StashComponent.h"
#include "Core/DataManagement/StashStorageSubsystem.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/Job/HBJobData.h"
#include "Combat/Ability/HBPlayerSkillGABase.h"
#include "Combat/Ability/HBAugmentLibrary.h"
#include "Combat/Ability/HBCommonAugmentSet.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "InventoryComponent.h"
#include "EquipmentComponent.h"
#include "Inventory/QuickSlotComponent.h"
#include "Player/HBPlayerCorpseInventory.h"
#include "Shop/ShopComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DataTable.h"
#include "Core/DataManagement/LoadoutStorage.h"
#include "Core/DataManagement/HBPlayerSaveComponent.h"
#include "Core/DataManagement/LoadoutStorageSubsystem.h"
#include "TimerManager.h"
#include "CraftComponent.h"
#include "OnlineSubsystem.h"

namespace
{
	// 스킬 하나의 GA 클래스에서 CDO 를 꺼낸다.
	//
	// 왜 CDO 인가: 증강 목록은 GA 애셋의 클래스 기본값이라 인스턴스가 필요 없다.
	// 후보를 뽑는 시점에는 그 스킬이 아직 부여되지 않았을 수도 있다.
	//
	// 쳐낼 조건: 증강 목록이 인스턴스마다 달라지면 CDO 로는 못 읽는다.
	// 지금은 애셋에 박힌 값이라 안전하다.
	const UHBPlayerSkillGABase* GetSkillCDO(const UHBJobData* JobData, const FName SkillId)
	{
		if (!JobData)
		{
			return nullptr;
		}

		const FHBJobSkillData* Skill = JobData->FindSkillAnywhere(SkillId);
		if (!Skill)
		{
			return nullptr;
		}
		if (!Skill->AbilityClass)
		{
			return nullptr;
		}

		return Cast<UHBPlayerSkillGABase>(Skill->AbilityClass->GetDefaultObject());
	}

	// 단계가 있는 증강은 카드 제목에 단계를 붙인다. 전용 위젯을 만들지 않고
	// 제목에 넣는 이유는 WBP 수정 없이 돌아가게 하려는 것이다.
	// UHB_AugmentCard 의 바인딩이 BindWidgetOptional 이라 나중에 전용 위젯으로
	// 옮기는 길은 열려 있다.
	FText MakeTieredDisplayName(const FText& Base, const int32 Tier, const int32 MaxTier)
	{
		if (MaxTier <= 1)
		{
			return Base;   // 단계 없는 증강은 그대로
		}
		return FText::Format(NSLOCTEXT("HBAugment", "TieredName", "{0} ({1}단계)"),
			Base, FText::AsNumber(Tier));
	}

	// 증강 하나를 후보 한 장으로 만들어 통에 넣는다. 최대 단계면 아무것도 안 넣는다.
	//
	// 왜 뺐나: 스킬 증강과 공통 강화의 수집 코드가 단계 필터 아래로는 글자 하나까지
	// 같았다. 다른 건 어느 통에서 증강을 꺼내는가(GA CDO / DataAsset)와 그래서
	// TargetSkillId 에 뭘 넣는가 뿐이고, 그건 이 함수를 부르는 쪽 반복문에 남아 있다.
	// 붙여넣기로 두면 카드 표기 규칙이 바뀔 때마다 두 곳을 같이 고쳐야 하고,
	// 한쪽만 고치면 공통 강화 카드만 다르게 보이는 식으로 어긋난다.
	//
	// 멤버가 아니라 자유 함수인 이유: 필요한 상태를 전부 인자로 받는다.
	// PlayerState 를 만들지 않아도 부를 수 있어야 나중에 테스트가 붙는다.
	void AddAugmentCandidate(
		const FHBSkillAugment& Augment,
		const FName TargetSkillId,
		const TArray<FHBAugmentStack>& TakenAugments,
		const UHBCommonAugmentSet* IconSet,
		const TSoftObjectPtr<UTexture2D>& FallbackIcon,
		TArray<FHBUpgradeCandidate>& OutPool)
	{
		if (!Augment.AugmentTag.IsValid())
		{
			return;
		}

		const int32 CurrentTier = HBAugment::FindTier(TakenAugments, Augment.AugmentTag);
		const int32 MaxTier = HBAugment::GetMaxTier(Augment);
		if (CurrentTier >= MaxTier)
		{
			return;   // 최대 단계에 도달했다. 후보에서 사라진다
		}

		FHBUpgradeCandidate& Candidate = OutPool.AddDefaulted_GetRef();
		Candidate.Tier = CurrentTier + 1;
		Candidate.CandidateId = FName(*FString::Printf(
			TEXT("Augment.%s.T%d"), *Augment.AugmentTag.GetTagName().ToString(), Candidate.Tier));
		Candidate.Type = EHBUpgradeCandidateType::Augment;
		// 공통 강화면 호출부가 NAME_None 을 넘긴다. 그게 "공통" 의 표시다.
		Candidate.TargetSkillId = TargetSkillId;
		Candidate.AugmentTag = Augment.AugmentTag;
		Candidate.DisplayName = MakeTieredDisplayName(Augment.DisplayName, Candidate.Tier, MaxTier);
		Candidate.Description = Augment.GetFormattedDescription();
		Candidate.Icon = Augment.Icon;
		if (Candidate.Icon.IsNull() && IconSet != nullptr)
		{
			if (const TSoftObjectPtr<UTexture2D>* Found = IconSet->AugmentIcons.Find(Augment.AugmentTag.GetTagName()))
			{
				Candidate.Icon = *Found;
			}
		}
		if (Candidate.Icon.IsNull())
		{
			Candidate.Icon = FallbackIcon;
		}
	}
}

AHB_PlayerState::AHB_PlayerState()
{
#pragma region NetworkSettings
	bReplicates = true;

	// APlayerState 의 엔진 기본값은 1Hz 다 (AActor 는 100). 이름/점수처럼 자주 바뀌지 않는
	// 데이터를 전제한 값인데, 우리는 여기에 AttributeSet(HP/마나)을 얹어놨다.
	// 그대로 두면 전투 자원이 최대 1초 늦게 도착해서, 달리기 소모처럼 연속적인 변화가
	// 화면에 거의 반영되지 않는다.
	SetNetUpdateFrequency(30.0f);
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
	BagInventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("BagInventory"));
	BagInventory->SetBagStorage(true);
	OpenedBagInventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("OpenedBagInventory"));
	OpenedBagInventory->SetBagStorage(true);
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

#pragma region Craft
	Craft = CreateDefaultSubobject<UCraftComponent>(TEXT("Craft"));
	static ConstructorHelpers::FObjectFinder<UDataTable> CraftRecipeFinder(TEXT("/Game/Craft/DT_CraftRecipe.DT_CraftRecipe"));
	if (CraftRecipeFinder.Succeeded())
	{
		Craft->SetRecipeTable(CraftRecipeFinder.Object);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Craft: recipe table not found"));
	}
	if (ItemTableFinder.Succeeded())
	{
		FDataTableRowHandle CraftGoldRow;
		CraftGoldRow.DataTable = ItemTableFinder.Object;
		CraftGoldRow.RowName = TEXT("ITM_002");
		Craft->SetGoldItemRow(CraftGoldRow);
	}
#pragma endregion

#pragma region Save
	// 로컬 저장 흐름과 저장 방식 칸(Local / Backend). 로비 · 매치 PlayerState 가 같이 갖는다.
	SaveComponent = CreateDefaultSubobject<UHBPlayerSaveComponent>(TEXT("Save"));
#pragma endregion
}
void AHB_PlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (Equipment != nullptr && BagInventory != nullptr)
	{
		Equipment->SetBagInventory(BagInventory);
	}

	if (Equipment != nullptr && OpenedBagInventory != nullptr)
	{
		Equipment->SetOpenedBagInventory(OpenedBagInventory);
	}

	if (Craft != nullptr && Stash != nullptr)
	{
		Craft->SetTargetStash(Stash);
	}

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

	// 외부 서버 저장 모드일 때만 서버가 불러온다.
	// 로컬 모드는 각자 PC 가 파일을 읽어 보낸다 (UHBPlayerSaveComponent). 서버는 기다리기만 한다.
	if (UsesBackendSave())
	{
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

void AHB_PlayerState::SaveLoadout()
{
	if (!HasAuthority() || !LoadoutStorage.IsValid())
	{
		return;
	}

	if (bLoadoutSaveInFlight)
	{
		bLoadoutSavePending = true;
		return;
	}

	FPlayerLoadout Loadout;
	// 손실 뒤 상자·시체에서 가방으로 다시 옮겨도(시간 종료 직후, 조작된 클라) 그 아이템은 저장에 싣지 않는다.
	if (!bCarriedItemsForfeited)
	{
		if (Inventory) { Loadout.InventoryEntries = Inventory->GetEntries(); }
		if (Equipment) { Loadout.EquipmentSlots = Equipment->GetSlots(); }
		if (QuickSlot) { Loadout.QuickSlots = QuickSlot->GetSlots(); }
	}

	bLoadoutSaveInFlight = true;
	bLoadoutSavePending = false;

	FOnLoadoutSaved Callback;
	Callback.BindUObject(this, &AHB_PlayerState::HandleLoadoutSaved);
	LoadoutStorage->SaveLoadoutAsync(LoadoutAccountId, Loadout, Callback);
}

void AHB_PlayerState::HandleLoadoutSaved(bool bSuccess)
{
	bLoadoutSaveInFlight = false;
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout: save failed"));
	}
	if (bLoadoutSavePending)
	{
		bLoadoutSavePending = false;
		SaveLoadout();
	}
}

void AHB_PlayerState::ForfeitCarriedItems(UHBPlayerCorpseInventory* Corpse)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Corpse)
	{
		TArray<FItemInstance> Equipped;
		if (Equipment)
		{
			for (uint8 Index = 0; Index < static_cast<uint8>(EEquipSlotType::MAX); ++Index)
			{
				FItemInstance Item;
				if (Equipment->GetItemInSlot(static_cast<EEquipSlotType>(Index), Item))
				{
					Equipped.Add(Item);
				}
			}
		}

		Corpse->FillFrom(Equipped, Inventory ? Inventory->GetEntries() : TArray<FInventoryEntry>());
	}

	if (Equipment)
	{
		for (uint8 Index = 0; Index < static_cast<uint8>(EEquipSlotType::MAX); ++Index)
		{
			Equipment->ClearSlot(static_cast<EEquipSlotType>(Index));
		}
	}

	if (Inventory)
	{
		Inventory->ClearAll();
	}

	if (QuickSlot)
	{
		TArray<FGuid> EmptySlots;
		EmptySlots.SetNum(UQuickSlotComponent::NumQuickSlots);
		QuickSlot->ApplyLoadedSlots(EmptySlots);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadoutSaveTimer);
	}

	bCarriedItemsForfeited = true;

	UE_LOG(LogTemp, Log, TEXT("Loadout: forfeited carried items (corpse=%s, inFlight=%d)"),
		Corpse ? TEXT("yes") : TEXT("no"), bLoadoutSaveInFlight ? 1 : 0);

	if (!LoadoutStorage.IsValid() || LoadoutAccountId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Loadout: forfeit save skipped (storage=%d, account=%s)"),
			LoadoutStorage.IsValid() ? 1 : 0, *LoadoutAccountId);
		return;
	}

	// 전송 중이어도 기다리지 않고 바로 빈 상태를 보낸다. 이 PlayerState 는 곧 파괴될 수 있어
	// 기존 콜백 경로(BindUObject)에 맡기면 빈 저장이 누락된다. 그래서 콜백은 수명과 무관한 로그만.
	FOnLoadoutSaved Callback = FOnLoadoutSaved::CreateLambda([](bool bSuccess)
		{
			if (!bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("Loadout: forfeit save failed"));
			}
		});
	LoadoutStorage->SaveLoadoutAsync(LoadoutAccountId, FPlayerLoadout(), Callback);

	// 앞선 전송이 이 빈 저장보다 늦게 도착할 수 있다. 살아 있으면 그 콜백에서 한 번 더 빈 상태로 덮는다.
	if (bLoadoutSaveInFlight)
	{
		bLoadoutSavePending = true;
	}
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

	LoadoutAccountId = ResolveAccountId();

	if (LoadoutAccountId.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Loadout: steam identity unavailable; load skipped"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Loadout: account=%s"), *LoadoutAccountId);

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

	bool bNewlyEngraved = false;
	ApplyLoadout(InLoadout, bNewlyEngraved);

	// 외부 서버 저장 연결. 아이템을 다 넣은 뒤에 묶어서, 넣는 도중엔 저장이 안 나간다.
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &AHB_PlayerState::RequestSaveLoadout);
	}
	if (BagInventory)
	{
		BagInventory->OnInventoryChanged.AddUniqueDynamic(this, &AHB_PlayerState::RequestSaveLoadout);
	}
	if (Equipment)
	{
		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &AHB_PlayerState::RequestSaveLoadout);
	}
	if (QuickSlot)
	{
		QuickSlot->OnQuickSlotChanged.AddUObject(this, &AHB_PlayerState::HandleQuickSlotChanged);
	}

	// 새로 발린 게 있으면 저장한다 — 안 하면 다음 접속 때 다시 굴려져 값이 바뀐다.
	if (bNewlyEngraved)
	{
		RequestSaveLoadout();
	}

	UE_LOG(LogTemp, Log, TEXT("Loadout: applied inv=%d"), InLoadout.InventoryEntries.Num());
}

void AHB_PlayerState::ApplyLoadout(const FPlayerLoadout& InLoadout, bool& bOutNewlyEngraved)
{
	// 각인 입구 (불러오기). 각인이 생기기 전에 저장된 장비는 여기서 한 번 발린다.
	// 새로 발린 게 있는지는 부르는 쪽이 저장할지 정하려고 받는다.
	bOutNewlyEngraved = false;

	if (Inventory)
	{
		Inventory->ClearAll();

		for (const FInventoryEntry& Entry : InLoadout.InventoryEntries)
		{
			FItemInstance Item = Entry.Item;
			if (Inventory->EnsureEngraved(Item))
			{
				bOutNewlyEngraved = true;
			}
			Inventory->TryAddItemAt(Item, Entry.Position);
		}
	}

	if (Equipment)
	{
		// 장착 장비는 인벤토리 입구를 안 거친다 (ApplyLoadedSlots 가 칸에 바로 넣는다). 여기서 바른다.
		// 표는 같은 플레이어의 인벤토리 것을 쓴다 — 장비 컴포넌트에 표를 하나 더 두지 않으려고.
		TArray<FItemInstance> LoadedSlots = InLoadout.EquipmentSlots;
		if (Inventory)
		{
			for (FItemInstance& Slot : LoadedSlots)
			{
				if (Inventory->EnsureEngraved(Slot))
				{
					bOutNewlyEngraved = true;
				}
			}
		}

		Equipment->ApplyLoadedSlots(LoadedSlots);
	}

	if (QuickSlot)
	{
		QuickSlot->ApplyLoadedSlots(InLoadout.QuickSlots);
	}
}

void AHB_PlayerState::ApplySavedData(const TArray<FInventoryEntry>& InStash, const FPlayerLoadout& InLoadout,
	const FHBStartingLoadout& InStartingLoadout)
{
	if (!HasAuthority())
	{
		return;
	}

	// 각인이 새로 발려도 여기서 저장을 부르지 않는다. 로컬 모드는 복제가 내 PC 에 도착하면 그쪽이 저장한다.
	if (Stash)
	{
		Stash->ApplyLoadedEntries(InStash);
	}
	GrantTestGoldIfNeeded();

	bool bNewlyEngraved = false;
	ApplyLoadout(InLoadout, bNewlyEngraved);

	// 로비에서 등록한 스킬. 비어 있으면(첫 실행) 지금 값을 둔다 — 로비에서 넘어온 값이나 기본 구성.
	if (InStartingLoadout.Job)
	{
		FText Error;
		if (!TrySetStartingLoadout(InStartingLoadout, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Save] 저장된 스킬 구성이 안 맞아 무시한다 | %s"), *Error.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Save] 적용 %s | 창고 %d 인벤 %d"),
		*GetNameSafe(this), InStash.Num(), InLoadout.InventoryEntries.Num());
}

bool AHB_PlayerState::UsesBackendSave() const
{
	if (!SaveComponent)
	{
		return true;
	}
	return SaveComponent->GetSaveMode() == EHBSaveMode::Backend;
}

FString AHB_PlayerState::ResolveAccountId() const
{
	const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	const bool bSteamIsActive = OnlineSubsystem != nullptr &&
		OnlineSubsystem->GetSubsystemName() == FName(TEXT("STEAM"));

	if (!bSteamIsActive)
	{
		return TEXT("TestAccount");
	}

	const FUniqueNetIdRepl& UniqueNetId = GetUniqueId();

	if (UniqueNetId.IsValid() &&
		UniqueNetId.GetType() == FName(TEXT("STEAM")))
	{
		return UniqueNetId.ToString();
	}

	return FString();
}
void AHB_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHB_PlayerState, PlayerLevel);
	DOREPLIFETIME(AHB_PlayerState, CurrentExperience);
	DOREPLIFETIME(AHB_PlayerState, TeamId);
	DOREPLIFETIME(AHB_PlayerState, TakenAugments);
	DOREPLIFETIME(AHB_PlayerState, UpgradeHistory);
	DOREPLIFETIME(AHB_PlayerState, PendingUpgradeChoices);
	DOREPLIFETIME(AHB_PlayerState, CurrentUpgradeCandidates);
	DOREPLIFETIME(AHB_PlayerState, StartingLoadout);

}

bool AHB_PlayerState::TrySetStartingLoadout(const FHBStartingLoadout& InLoadout, FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!HasAuthority())
	{
		OutError = FText::FromString(TEXT("서버에서만 시작 구성을 확정할 수 있습니다."));
		return false;
	}

	if (!ValidateStartingLoadout(InLoadout, OutError))
	{
		return false;
	}

	// 같은 값이면 복제도 알림도 일으키지 않는다.
	// 자동 저장이 같은 구성을 반복해서 보내도 조용히 넘어간다.
	if (StartingLoadout == InLoadout)
	{
		return true;
	}

	StartingLoadout = InLoadout;

	HandleStartingLoadoutChanged();
	ForceNetUpdate();
	return true;
}

void AHB_PlayerState::HandleStartingLoadoutChanged()
{
	OnStartingLoadoutChanged.Broadcast();
}

void AHB_PlayerState::OnRep_StartingLoadout()
{
	HandleStartingLoadoutChanged();
}

void AHB_PlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	AHB_PlayerState* NewState = Cast<AHB_PlayerState>(NewPlayerState);
	if (!NewState)
	{
		return;
	}

	NewState->StartingLoadout = StartingLoadout;
	NewState->ForceNetUpdate();

	// 진단 로그. 이 줄이 안 찍히면 CopyProperties 가 실제로는 안 불리는 것이고,
	// 그때는 SeamlessTravelTo 도 같이 오버라이드해야 한다.
	// 트래블 전달이 확인되면 지운다.
	UE_LOG(LogTemp, Log,
		TEXT("[SkillLoadout] CopyProperties | Job=%s Q=%s E=%s P=%s"),
		*GetNameSafe(StartingLoadout.Job),
		*StartingLoadout.SkillQId.ToString(),
		*StartingLoadout.SkillEId.ToString(),
		*StartingLoadout.PassiveSkillId.ToString());
}

void AHB_PlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		UWorld* World = GetWorld();
		if (World && World->GetTimerManager().IsTimerActive(LoadoutSaveTimer))
		{
			World->GetTimerManager().ClearTimer(LoadoutSaveTimer);
			SaveLoadout();
		}
	}

	Super::EndPlay(EndPlayReason);
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
	LoadLoadout();

	if (!bSuccess)
	{
		return;
	}
	GrantTestGoldIfNeeded();
}

void AHB_PlayerState::GrantTestGoldIfNeeded()
{
	// 로컬 모드에서는 아래 SaveStash() 가 저장소가 없어 아무것도 안 한다. 그 경우 내 PC 가 복제를 받아 저장한다.
	if (!HasAuthority() || !bGrantTestGold || !Stash)
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

	const int32 PreviousLevel = PlayerLevel;
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

	PendingUpgradeChoices += PlayerLevel - PreviousLevel;

	BroadcastProgression();
	if (PlayerLevel != PreviousLevel)
	{
		OnSkillUpgradesChanged.Broadcast();
		GenerateUpgradeCandidates();
	}
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
	PendingUpgradeChoices = 0;
	CurrentUpgradeCandidates.Reset();

	// 증강은 이번 판에서만 유효하다. 태그와 그 태그가 붙인 GE 를 같이 걷는다.
	// GE 를 안 걷으면 로비에서도 공격력이 오른 채로 남는다.
	// 핸들 컨테이너 둘은 RemoveAllAugmentEffects 안에서 같이 비워진다.
	RemoveAllAugmentEffects();
	TakenAugments.Reset();
	UpgradeHistory.Reset();

	BroadcastProgression();
	OnSkillUpgradesChanged.Broadcast();
	OnUpgradeCandidatesChanged.Broadcast();
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

bool AHB_PlayerState::HasAugment(const FGameplayTag AugmentTag) const
{
	if (!AugmentTag.IsValid())
	{
		return false;
	}

	// 단계가 1 이상이면 보유다.
	return HBAugment::FindTier(TakenAugments, AugmentTag) > 0;
}

FGameplayTagContainer AHB_PlayerState::GetTakenAugments() const
{
	FGameplayTagContainer Out;
	for (const FHBAugmentStack& Stack : TakenAugments)
	{
		if (Stack.AugmentTag.IsValid())
		{
			Out.AddTag(Stack.AugmentTag);
		}
	}
	return Out;
}

int32 AHB_PlayerState::GetAugmentTier(const FGameplayTag AugmentTag) const
{
	return HBAugment::FindTier(TakenAugments, AugmentTag);
}

bool AHB_PlayerState::FindAugmentAnywhere(
	const FName TargetSkillId, const FGameplayTag AugmentTag, FHBSkillAugment& Out) const
{
	if (!AugmentTag.IsValid())
	{
		return false;
	}

	// 스킬에 딸린 증강이면 그 스킬의 CDO 가 주인이다.
	if (!TargetSkillId.IsNone())
	{
		const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
		const UHBJobData* JobData = Character ? Character->GetJobData() : nullptr;
		if (const UHBPlayerSkillGABase* SkillCDO = GetSkillCDO(JobData, TargetSkillId))
		{
			return SkillCDO->FindAugment(AugmentTag, Out);
		}
		return false;
	}

	// 비어 있으면 공통 강화다.
	if (CommonAugmentSet)
	{
		if (const FHBSkillAugment* Found = CommonAugmentSet->FindAugment(AugmentTag))
		{
			Out = *Found;
			return true;
		}
	}
	return false;
}

bool AHB_PlayerState::ApplyAugment(const FName TargetSkillId, const FGameplayTag AugmentTag)
{
	if (!HasAuthority() || !AugmentTag.IsValid())
	{
		return false;
	}

	FHBSkillAugment Augment;
	if (!FindAugmentAnywhere(TargetSkillId, AugmentTag, Augment))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Augment] %s 를 스킬(%s)도 공통 세트도 갖고 있지 않다"),
			*AugmentTag.ToString(), *TargetSkillId.ToString());
		return false;
	}

	// 단계는 서버가 센다. 후보에 실린 Tier 를 신뢰하지 않는다.
	const int32 MaxTier = HBAugment::GetMaxTier(Augment);
	if (!HBAugment::RaiseTier(TakenAugments, AugmentTag, MaxTier))
	{
		return false;   // 이미 최대 단계다
	}
	const int32 NewTier = HBAugment::FindTier(TakenAugments, AugmentTag);

	// 수치만 쓰는 증강은 GrantEffect 가 비어 있다. 단계는 올랐으니 성공이다.
	if (!Augment.GrantEffect || !AbilitySystemComponent)
	{
		return true;
	}

	// GE 는 있는데 TierMagnitudes 가 비면 GetTierMagnitude 가 1단계에도 0 을 준다.
	// 공통 강화 GE 는 전부 SetByCaller 로 수치를 받으므로 결과는 "0 만큼 강해지는"
	// 무효 증강이다. 그런데 GetMaxTier 가 1 을 돌려주니 바로 최대 단계가 되어
	// 후보에서도 사라진다. 에러 없이 조용히 사라지는 애셋 설정 실수라서,
	// 애셋을 만드는 단계에서 바로 보이도록 시끄럽게 남긴다.
	//
	// 막지는 않는다. "비면 단계 없는 1회성" 이라는 기존 뜻을 그대로 두고
	// 알려만 준다. GE 가 SetByCaller 를 안 쓰는 경우도 있을 수 있다.
	if (Augment.TierMagnitudes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Augment] '%s' 는 GrantEffect 가 있는데 TierMagnitudes 가 비었다. ")
			TEXT("SetByCaller 수치가 0 으로 들어간다. 애셋에 단계 수치를 채워라."),
			*AugmentTag.GetTagName().ToString());
	}

	// 새 스펙을 먼저 만든다. 이전 GE 를 먼저 떼면, 새 스펙이 실패했을 때
	// 증강이 아무 효과도 없이 남는다. 검증된 교체본이 손에 있을 때만 뗀다.
	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
		Augment.GrantEffect, 1.f, AbilitySystemComponent->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return true;   // 단계는 올랐다. GE 만 실패했다. 이전 GE 는 그대로 둔다
	}

	// 이제야 이전 단계 GE 를 뗀다. TierMagnitudes 가 최종 수치라서 겹쳐 붙이면
	// 합산되어 뜻이 달라진다.
	if (const FActiveGameplayEffectHandle* Previous = AugmentEffectByTag.Find(AugmentTag))
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(*Previous);
		AugmentEffectHandles.Remove(*Previous);
		AugmentEffectByTag.Remove(AugmentTag);
	}

	Spec.Data->SetSetByCallerMagnitude(
		AbilityTag::TAG_SetByCaller_AugmentMagnitude,
		HBAugment::GetTierMagnitude(Augment, NewTier));

	const FActiveGameplayEffectHandle Handle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!Handle.IsValid())
	{
		// 붙이기가 거절됐다 (면역, ApplicationTagRequirements 등). 이 시점에는
		// 이전 단계 GE 를 이미 뗐으므로, 그냥 두면 단계는 N 인데 붙은 GE 는
		// 하나도 없는 상태가 된다. 게다가 N 이 최대 미만이어도 다음 후보에는
		// N+1 만 뜨니 없어진 효과를 되찾을 길이 영영 없다.
		//
		// 두 수습안 중 단계 되돌리기를 골랐다. 뗀 GE 를 다시 붙이는 쪽은
		// 방금 실패한 ApplyGameplayEffectSpecToSelf 를 같은 GE 애셋으로 한 번 더
		// 부르는 것이라, 거절 사유(면역·태그 요구)가 그대로면 똑같이 실패한다.
		// 실패의 실패는 알릴 방법도 없다. 단계 되돌리기는 항상 성공한다.
		//
		// 되돌린 뒤 false 를 준다. 호출부(ApplyUpgradeCandidate)가 false 면
		// 선택권을 소모하지 않으므로 플레이어는 카드를 잃지 않고, 증강은
		// 최대 단계 미만이라 다음 레벨업 후보에 다시 뜬다. 거절 사유가
		// 그때 사라져 있으면 그대로 붙는다.
		LowerAugmentTier(AugmentTag);

		UE_LOG(LogTemp, Warning,
			TEXT("[Augment] '%s' %d단계 GE 적용이 거절됐다. 단계를 %d 로 되돌린다."),
			*AugmentTag.GetTagName().ToString(), NewTier, NewTier - 1);
		return false;
	}

	AugmentEffectHandles.Add(Handle);
	AugmentEffectByTag.Add(AugmentTag, Handle);
	return true;
}

void AHB_PlayerState::LowerAugmentTier(const FGameplayTag AugmentTag)
{
	for (int32 Index = 0; Index < TakenAugments.Num(); ++Index)
	{
		if (TakenAugments[Index].AugmentTag != AugmentTag)
		{
			continue;
		}

		// 1단계에서 실패했으면 아예 안 먹은 것으로 되돌린다. Tier 0 짜리 항목을
		// 남기면 GetTakenAugments 가 안 먹은 증강을 보유로 세어 UI 에 뜬다.
		if (--TakenAugments[Index].Tier <= 0)
		{
			TakenAugments.RemoveAt(Index);
		}
		return;
	}
}

void AHB_PlayerState::RemoveAllAugmentEffects()
{
	if (AbilitySystemComponent)
	{
		for (const FActiveGameplayEffectHandle& Handle : AugmentEffectHandles)
		{
			if (!Handle.IsValid())
			{
				continue;
			}
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}
	AugmentEffectHandles.Reset();

	// 태그->핸들 맵도 같이 비운다. 두 컨테이너는 같은 핸들을 두 모양으로 들고
	// 있는 하나의 상태다. 여기서 안 비우면 죽음/리스폰처럼 호출부가 늘었을 때
	// 이미 떼어낸 핸들이 맵에 남고, 다음 단계 상승이 그 죽은 핸들을 떼려 든다.
	// 호출부가 따로 Reset 해 주기를 기대하는 건 계약이 아니라 우연이었다.
	AugmentEffectByTag.Reset();
}

// ================ 폐기 예정 ================
// 스킬 레벨 모델은 증강 태그로 대체됐다. BP 참조가 없다고 확인되면 지운다.

int32 AHB_PlayerState::GetSkillUpgradeLevel(const FName SkillId) const
{
	return 0;
}

void AHB_PlayerState::RequestUpgradeSkill(const FName SkillId)
{
}

void AHB_PlayerState::OnRep_SkillUpgrades()
{
	OnSkillUpgradesChanged.Broadcast();
}

void AHB_PlayerState::RequestSelectUpgrade(const FName CandidateId)
{
	if (HasAuthority())
	{
		ApplyUpgradeCandidate(CandidateId);
	}
	else
	{
		ServerRequestSelectUpgrade(CandidateId);
	}
}

void AHB_PlayerState::ServerRequestSelectUpgrade_Implementation(const FName CandidateId)
{
	ApplyUpgradeCandidate(CandidateId);
}

void AHB_PlayerState::ApplyUpgradeCandidate(const FName CandidateId)
{
	if (!HasAuthority() || PendingUpgradeChoices <= 0)
	{
		return;
	}

	// 람다 대신 직접 훑는다. 후보가 최대 3장이라 비용이 없다.
	const FHBUpgradeCandidate* Candidate = nullptr;
	for (const FHBUpgradeCandidate& Entry : CurrentUpgradeCandidates)
	{
		if (Entry.CandidateId == CandidateId)
		{
			Candidate = &Entry;
			break;
		}
	}
	if (!Candidate)
	{
		return;   // 클라가 없는 카드를 보냈다
	}

	// 카드 종류에 따라 적용 방식이 갈린다. 성공했을 때만 선택권을 소모한다.
	bool bApplied = false;

	if (Candidate->Type == EHBUpgradeCandidateType::Augment)
	{
		// 실패하면 선택권을 소모하지 않는다. 예전에는 무조건 true 였고,
		// 증강을 못 찾거나 GrantEffect 가 비면 카드 한 장을 잃었다.
		bApplied = ApplyAugment(Candidate->TargetSkillId, Candidate->AugmentTag);
	}
	else
	{
		AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
		if (Character)
		{
			bApplied = Character->AddPassiveSkillFromUpgrade(Candidate->TargetSkillId);
		}
	}

	if (!bApplied)
	{
		return;
	}

	// Candidate 는 아래 GenerateUpgradeCandidates 가 후보 배열을 비우기 전에만 유효하다.
	FHBUpgradePick Pick;
	Pick.Type = Candidate->Type;
	Pick.TargetSkillId = Candidate->TargetSkillId;
	Pick.AugmentTag = Candidate->AugmentTag;
	Pick.Tier = Candidate->Tier;
	UpgradeHistory.Add(Pick);

	--PendingUpgradeChoices;
	OnSkillUpgradesChanged.Broadcast();
	GenerateUpgradeCandidates();
	ForceNetUpdate();
}

void AHB_PlayerState::GenerateUpgradeCandidates()
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentUpgradeCandidates.Reset();
	AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	if (PendingUpgradeChoices <= 0 || !Character)
	{
		OnUpgradeCandidatesChanged.Broadcast();
		ForceNetUpdate();
		return;
	}

	// 후보를 종류별 자리로 나누지 않고 한 통에 모은 뒤 섞는다.
	//
	// 왜 이렇게 바꿨나: 전에는 "패시브 1장 + Q강화 + E강화" 로 자리가 고정이라
	// 액티브 증강이 항상 2장, 패시브가 항상 1장 이상 떴다. 기획은 모든 증강이
	// 똑같은 확률로 섞여 나오는 것이다.
	TArray<FHBUpgradeCandidate> Pool;

	CollectMissingPassives(Pool);
	CollectSkillAugments(Pool, Character->GetSelectedSkillQId());
	CollectSkillAugments(Pool, Character->GetSelectedSkillEId());
	for (const FName PassiveId : Character->GetSelectedPassiveSkillIds())
	{
		// 보유한 패시브의 증강도 같은 통에 들어간다.
		// 패링 증강은 패링 패시브를 얻은 뒤에야 후보로 떠야 한다.
		CollectSkillAugments(Pool, PassiveId);
	}

	// 궁극기는 넣지 않는다. 궁극기 증강은 기획에서 폐기됐다.

	// 공통 강화도 같은 통에 넣어 똑같은 확률로 뽑는다(기획서 28.1).
	CollectCommonAugments(Pool);

	// 씨앗은 매번 새로 만든다. 재현이 필요한 곳이 아니다.
	// DrawCandidates 가 FRandomStream 을 받는 이유는 테스트에서 고정하기
	// 위한 것이고, 실제 플레이에서는 무작위가 맞다.
	FRandomStream Rng(FMath::Rand());
	HBAugment::DrawCandidates(Pool, 3, Rng, CurrentUpgradeCandidates);

	// 뽑을 게 하나도 없으면 선택권을 소모하고 넘긴다.
	// 안 그러면 카드 0장짜리 선택 UI 가 영영 열려 있는다.
	if (CurrentUpgradeCandidates.IsEmpty())
	{
		PendingUpgradeChoices = 0;
		OnSkillUpgradesChanged.Broadcast();
	}

	OnUpgradeCandidatesChanged.Broadcast();
	ForceNetUpdate();
}

void AHB_PlayerState::CollectMissingPassives(TArray<FHBUpgradeCandidate>& OutPool) const
{
	const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	const UHBJobData* JobData = Character ? Character->GetJobData() : nullptr;
	if (!JobData)
	{
		return;
	}

	const TArray<FName> OwnedPassives = Character->GetSelectedPassiveSkillIds();
	for (const FHBJobSkillData& Passive : JobData->GetPassiveSkillCatalog())
	{
		if (Passive.SkillId.IsNone())
		{
			continue;
		}
		if (!Passive.AbilityClass)
		{
			continue;
		}
		if (OwnedPassives.Contains(Passive.SkillId))
		{
			continue;   // 이미 가졌다
		}

		FHBUpgradeCandidate& Candidate = OutPool.AddDefaulted_GetRef();
		Candidate.CandidateId = FName(*FString::Printf(
			TEXT("Acquire.Passive.%s"), *Passive.SkillId.ToString()));
		Candidate.Type = EHBUpgradeCandidateType::NewPassive;
		Candidate.TargetSkillId = Passive.SkillId;
		Candidate.DisplayName = Passive.DisplayName;
		// 설명의 치환자를 실제 수치로 바꿔서 넘긴다. 아래 증강 카드와 같은 처리다.
		// 안 하면 같은 패시브가 로비 카드와 레벨업 카드에서 다르게 보인다.
		Candidate.Description = Passive.GetFormattedDescription();
		Candidate.Icon = Passive.Icon;
	}
}

void AHB_PlayerState::CollectSkillAugments(TArray<FHBUpgradeCandidate>& OutPool, const FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return;
	}

	const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	const UHBJobData* JobData = Character ? Character->GetJobData() : nullptr;
	const FHBJobSkillData* Skill = JobData ? JobData->FindSkillAnywhere(SkillId) : nullptr;
	if (!Skill)
	{
		return;
	}

	// 증강 목록의 주인은 GA 애셋이다. 인스턴스 없이 CDO 에서 읽는다.
	const UHBPlayerSkillGABase* SkillCDO = GetSkillCDO(JobData, SkillId);
	if (!SkillCDO)
	{
		return;   // 이 스킬은 아직 새 베이스를 안 쓴다
	}

	for (const FHBSkillAugment& Augment : SkillCDO->GetAugments())
	{
		// 이 스킬에 딸린 증강이라 TargetSkillId 에 스킬을 적는다.
		AddAugmentCandidate(Augment, SkillId, TakenAugments, CommonAugmentSet, Skill->Icon, OutPool);
	}
}

void AHB_PlayerState::CollectCommonAugments(TArray<FHBUpgradeCandidate>& OutPool) const
{
	if (!CommonAugmentSet)
	{
		return;   // 애셋이 지정되지 않았다. 공통 강화 없이 동작한다
	}

	for (const FHBSkillAugment& Augment : CommonAugmentSet->Augments)
	{
		// TargetSkillId 를 비운다. 이것이 "공통" 의 표시다.
		AddAugmentCandidate(Augment, NAME_None, TakenAugments, CommonAugmentSet, TSoftObjectPtr<UTexture2D>(), OutPool);
	}
}

void AHB_PlayerState::OnRep_UpgradeCandidates()
{
	OnUpgradeCandidatesChanged.Broadcast();
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