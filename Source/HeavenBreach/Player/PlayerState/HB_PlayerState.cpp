#include "HB_PlayerState.h"

#include "Inventory/StashComponent.h"
#include "Core/DataManagement/StashStorageSubsystem.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/Job/HBJobData.h"
#include "Combat/Ability/HBPlayerSkillGABase.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "InventoryComponent.h"
#include "EquipmentComponent.h"
#include "Inventory/QuickSlotComponent.h"
#include "Shop/ShopComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DataTable.h"
#include "Core/DataManagement/LoadoutStorage.h"
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
}
void AHB_PlayerState::BeginPlay()
{
	Super::BeginPlay();


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
	DOREPLIFETIME(AHB_PlayerState, PendingUpgradeChoices);
	DOREPLIFETIME(AHB_PlayerState, CurrentUpgradeCandidates);

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
	RemoveAllAugmentEffects();
	TakenAugments.Reset();

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

	// 정확히 일치하는 것만 본다. 부모 태그로 물으면 다른 증강이 걸린다.
	return TakenAugments.HasTagExact(AugmentTag);
}

void AHB_PlayerState::ApplyAugment(const FName TargetSkillId, const FGameplayTag AugmentTag)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!AugmentTag.IsValid())
	{
		return;
	}
	if (TakenAugments.HasTagExact(AugmentTag))
	{
		return;   // 같은 증강을 두 번 고를 수는 없다
	}

	TakenAugments.AddTag(AugmentTag);

	// 전역 효과가 있으면 여기서 붙인다. 수치만 쓰는 증강은 GrantEffect 가 비어 있다.
	const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	const UHBJobData* JobData = Character ? Character->GetJobData() : nullptr;
	const UHBPlayerSkillGABase* SkillCDO = GetSkillCDO(JobData, TargetSkillId);
	if (!SkillCDO)
	{
		return;
	}

	FHBSkillAugment Augment;
	if (!SkillCDO->FindAugment(AugmentTag, Augment))
	{
		return;
	}
	if (!Augment.GrantEffect)
	{
		return;
	}
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
		Augment.GrantEffect, 1.f, AbilitySystemComponent->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}

	// 핸들을 들고 있는 이유는 개별 취소가 아니라 매치 리셋 때 걷기 위해서다.
	AugmentEffectHandles.Add(
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
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
		ApplyAugment(Candidate->TargetSkillId, Candidate->AugmentTag);
		bApplied = true;
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

	// 앞에서부터 3장을 뽑는다. 뽑을 때마다 마지막 것과 자리를 바꿔 O(1) 로 제거한다.
	const int32 DrawCount = FMath::Min(3, Pool.Num());
	for (int32 Drawn = 0; Drawn < DrawCount; ++Drawn)
	{
		const int32 Index = FMath::RandRange(0, Pool.Num() - 1);
		CurrentUpgradeCandidates.Add(Pool[Index]);
		Pool.RemoveAtSwap(Index);
	}

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
		Candidate.Description = Passive.Description;
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
		if (!Augment.AugmentTag.IsValid())
		{
			continue;
		}
		if (TakenAugments.HasTagExact(Augment.AugmentTag))
		{
			continue;   // 이미 고른 증강은 다시 안 나온다
		}

		FHBUpgradeCandidate& Candidate = OutPool.AddDefaulted_GetRef();
		Candidate.CandidateId = FName(*FString::Printf(
			TEXT("Augment.%s"), *Augment.AugmentTag.GetTagName().ToString()));
		Candidate.Type = EHBUpgradeCandidateType::Augment;
		Candidate.TargetSkillId = SkillId;
		Candidate.AugmentTag = Augment.AugmentTag;
		Candidate.DisplayName = Augment.DisplayName;
		// 설명의 {Cooldown} 같은 치환자를 실제 수치로 바꿔서 넘긴다.
		Candidate.Description = Augment.GetFormattedDescription();
		Candidate.Icon = Augment.Icon;
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
