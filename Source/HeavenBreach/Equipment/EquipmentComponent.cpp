#include "EquipmentComponent.h"
#include "UI/Audio/HBUIAudioLibrary.h"

#include "Net/UnrealNetwork.h"
#include "InventoryComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Player/HBPlayerCharacter.h"
#include "Item/Item.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Item/HB_ItemTag.h"

namespace
{
	// 방패와 동시 장착이 불가능한 무기 종류(원거리 무기). 한손검은 여기 해당하지 않으므로
	// 기존처럼 방패와 자유롭게 병행 장착할 수 있다. DT_Item 의 WeaponType 문자열과 그대로 비교한다
	// (HBPlayerCharacter.cpp 가 "Weapon.<WeaponType>" 태그를 만들 때 쓰는 값과 동일한 소스).
	bool IsShieldIncompatibleWeaponType(const FString& InWeaponType)
	{
		return InWeaponType == TEXT("Bow") || InWeaponType == TEXT("Staff");
	}
}

UEquipmentComponent::UEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Slots.SetNum(static_cast<int32>(EEquipSlotType::MAX));
}

UAbilitySystemComponent* UEquipmentComponent::GetOwnerASC() const
{
	// 이 컴포넌트는 AHB_PlayerState 의 서브오브젝트다. GetOwner() 는 PlayerState 이지
	// 캐릭터가 아니다. AHBPlayerCharacter 로 캐스팅하면 항상 실패해서 ASC 를 못 찾았고,
	// 그 탓에 유물 GE 도 장비 스탯 GE 도 조용히 붙지 않았다.

	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

void UEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Slots.Num() != static_cast<int32>(EEquipSlotType::MAX))
	{
		Slots.SetNum(static_cast<int32>(EEquipSlotType::MAX));
	}

	// 델리게이트에 물려 둔다. 장착·해제·교체·내구도·세이브 복원이 전부 이걸 쏘므로
	// 새 경로가 생겨도 자동으로 따라온다. 호출부마다 손으로 부르면 언젠가 하나를 빠뜨린다.
	OnEquipmentChanged.AddUniqueDynamic(this, &UEquipmentComponent::HandleEquipmentChanged);

	// 이미 채워진 채로 시작하는 경우(세이브 복원이 BeginPlay 보다 빨랐을 때)를 위해 한 번 돈다.
	HandleEquipmentChanged();
}

void UEquipmentComponent::HandleEquipmentChanged()
{
	RefreshEquipmentStats();
	RefreshItemEffects();
	SyncBagInventory();
}

void UEquipmentComponent::OnRep_Slots()
{
	OnEquipmentChanged.Broadcast();
}

void UEquipmentComponent::OnRep_OpenedBag()
{
	OnOpenedBagChanged.Broadcast();
}

void UEquipmentComponent::SetBagInventory(UInventoryComponent* InBagInventory)
{
	if (BagInventory == InBagInventory)
	{
		return;
	}

	if (BagInventory != nullptr)
	{
		BagInventory->OnInventoryChanged.RemoveDynamic(this, &UEquipmentComponent::HandleBagInventoryChanged);
	}

	BagInventory = InBagInventory;
	MountedBagId.Invalidate();

	if (BagInventory != nullptr)
	{
		BagInventory->SetBagStorage(true);
		BagInventory->OnInventoryChanged.AddUniqueDynamic(this, &UEquipmentComponent::HandleBagInventoryChanged);
	}

	SyncBagInventory();
}

void UEquipmentComponent::SyncBagInventory()
{
	if (BagInventory == nullptr || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	FItemInstance BagItem;
	const bool bHasBag = GetItemInSlot(EEquipSlotType::Backpack, BagItem);
	const FGuid NewBagId = bHasBag ? BagItem.InstanceId : FGuid();

	if (NewBagId == MountedBagId)
	{
		return;
	}

	MountedBagId.Invalidate();
	bSyncingBag = true;

	if (bHasBag)
	{
		FItemViewData ViewData;
		ResolveItemViewData(BagItem.ItemRow, ViewData);
		BagInventory->SetGridSize(FMath::Max(1, ViewData.BagGridSize.X), FMath::Max(1, ViewData.BagGridSize.Y));

		TArray<FInventoryEntry> Contents;
		Contents.Reserve(BagItem.BagContents.Num());
		for (const FBagItemEntry& BagEntry : BagItem.BagContents)
		{
			Contents.Add(BagItemToInventoryEntry(BagEntry));
		}
		BagInventory->SetEntries(Contents);
	}
	else
	{
		BagInventory->ClearAll();
	}

	bSyncingBag = false;
	MountedBagId = NewBagId;
}

void UEquipmentComponent::HandleBagInventoryChanged()
{
	if (bSyncingBag || BagInventory == nullptr || !MountedBagId.IsValid())
	{
		return;
	}

	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 SlotIndex = SlotToIndex(EEquipSlotType::Backpack);
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].InstanceId != MountedBagId)
	{
		return;
	}

	TArray<FBagItemEntry>& Contents = Slots[SlotIndex].BagContents;
	Contents.Reset();
	for (const FInventoryEntry& Entry : BagInventory->GetEntries())
	{
		Contents.Add(InventoryEntryToBagItem(Entry));
	}
}

void UEquipmentComponent::SetOpenedBagInventory(UInventoryComponent* InOpenedBagInventory)
{
	if (OpenedBagInventory == InOpenedBagInventory)
	{
		return;
	}

	if (OpenedBagInventory != nullptr)
	{
		OpenedBagInventory->OnInventoryChanged.RemoveDynamic(this, &UEquipmentComponent::HandleOpenedBagInventoryChanged);
	}

	OpenedBagInventory = InOpenedBagInventory;

	if (OpenedBagInventory != nullptr)
	{
		OpenedBagInventory->SetBagStorage(true);
		OpenedBagInventory->OnInventoryChanged.AddUniqueDynamic(this, &UEquipmentComponent::HandleOpenedBagInventoryChanged);
	}
}

bool UEquipmentComponent::OpenBagFrom(UInventoryComponent* InSource, FGuid InInstanceId)
{
	if (OpenedBagInventory == nullptr || InSource == nullptr || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (InSource == OpenedBagInventory || InSource == BagInventory)
	{
		return false;
	}

	const int32 EntryIndex = InSource->FindEntryIndexById(InInstanceId);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	const FItemInstance BagItem = InSource->GetEntries()[EntryIndex].Item;
	FItemViewData ViewData;
	if (!ResolveItemViewData(BagItem.ItemRow, ViewData) || !ViewData.IsBag())
	{
		return false;
	}

	if (OpenedBagSource.Get() == InSource && OpenedBagId == InInstanceId)
	{
		return true;
	}

	CloseOpenedBag();

	bSyncingOpenedBag = true;
	OpenedBagInventory->SetGridSize(FMath::Max(1, ViewData.BagGridSize.X), FMath::Max(1, ViewData.BagGridSize.Y));

	TArray<FInventoryEntry> Contents;
	Contents.Reserve(BagItem.BagContents.Num());
	for (const FBagItemEntry& BagEntry : BagItem.BagContents)
	{
		Contents.Add(BagItemToInventoryEntry(BagEntry));
	}
	OpenedBagInventory->SetEntries(Contents);
	bSyncingOpenedBag = false;

	OpenedBagSource = InSource;
	OpenedBagId = InInstanceId;
	OpenedBagRow = BagItem.ItemRow;
	InSource->OnInventoryChanged.AddUniqueDynamic(this, &UEquipmentComponent::HandleOpenedBagSourceChanged);

	OnOpenedBagChanged.Broadcast();
	return true;
}

void UEquipmentComponent::CloseOpenedBag()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!OpenedBagId.IsValid() && !OpenedBagSource.IsValid())
	{
		return;
	}

	UnbindOpenedBagSource();
	OpenedBagId.Invalidate();
	OpenedBagRow = FDataTableRowHandle();

	if (OpenedBagInventory != nullptr)
	{
		bSyncingOpenedBag = true;
		OpenedBagInventory->ClearAll();
		bSyncingOpenedBag = false;
	}

	OnOpenedBagChanged.Broadcast();
}

void UEquipmentComponent::UnbindOpenedBagSource()
{
	if (UInventoryComponent* Source = OpenedBagSource.Get())
	{
		Source->OnInventoryChanged.RemoveDynamic(this, &UEquipmentComponent::HandleOpenedBagSourceChanged);
	}
	OpenedBagSource.Reset();
}

void UEquipmentComponent::HandleOpenedBagInventoryChanged()
{
	if (bSyncingOpenedBag || OpenedBagInventory == nullptr || !OpenedBagId.IsValid())
	{
		return;
	}

	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	TArray<FBagItemEntry> Contents;
	Contents.Reserve(OpenedBagInventory->GetEntries().Num());
	for (const FInventoryEntry& Entry : OpenedBagInventory->GetEntries())
	{
		Contents.Add(InventoryEntryToBagItem(Entry));
	}

	UInventoryComponent* Source = OpenedBagSource.Get();
	bSyncingOpenedBag = true;
	const bool bWritten = Source != nullptr && Source->SetEntryBagContents(OpenedBagId, Contents);
	bSyncingOpenedBag = false;

	if (!bWritten)
	{
		CloseOpenedBag();
	}
}

void UEquipmentComponent::HandleOpenedBagSourceChanged()
{
	if (bSyncingOpenedBag || !OpenedBagId.IsValid())
	{
		return;
	}

	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	UInventoryComponent* Source = OpenedBagSource.Get();
	const int32 BagIndex = Source != nullptr ? Source->FindEntryIndexById(OpenedBagId) : INDEX_NONE;
	if (BagIndex == INDEX_NONE)
	{
		CloseOpenedBag();
		return;
	}

	if (OpenedBagInventory == nullptr)
	{
		return;
	}

	const TArray<FBagItemEntry>& SourceContents = Source->GetEntries()[BagIndex].Item.BagContents;
	const TArray<FInventoryEntry>& OpenedEntries = OpenedBagInventory->GetEntries();
	bool bSame = SourceContents.Num() == OpenedEntries.Num();
	for (int32 Index = 0; bSame && Index < SourceContents.Num(); ++Index)
	{
		const FBagItemEntry Current = InventoryEntryToBagItem(OpenedEntries[Index]);
		bSame = Current.InstanceId == SourceContents[Index].InstanceId
			&& Current.StackCount == SourceContents[Index].StackCount
			&& Current.Position == SourceContents[Index].Position
			&& Current.bRotated == SourceContents[Index].bRotated;
	}
	if (bSame)
	{
		return;
	}

	TArray<FInventoryEntry> Contents;
	Contents.Reserve(SourceContents.Num());
	for (const FBagItemEntry& BagEntry : SourceContents)
	{
		Contents.Add(BagItemToInventoryEntry(BagEntry));
	}

	bSyncingOpenedBag = true;
	OpenedBagInventory->SetEntries(Contents);
	bSyncingOpenedBag = false;
}

void UEquipmentComponent::Server_OpenBag_Implementation(UInventoryComponent* InSource, FGuid InInstanceId)
{
	OpenBagFrom(InSource, InInstanceId);
}

void UEquipmentComponent::Server_CloseOpenedBag_Implementation()
{
	CloseOpenedBag();
}

float UEquipmentComponent::SumArmorDefense() const
{
	// DT_Item 의 방어구는 이 세 슬롯이 전부다. HELM_* / CHEST_* / LEGS_* 가 여기 들어간다.
	// 피격 시 내구도를 깎는 목록과 같아야 한다 — 다르면 "안 닳는데 방어는 되는" 칸이 생긴다.
	static const EEquipSlotType ArmorSlots[] =
	{
		EEquipSlotType::Helmet,
		EEquipSlotType::Chest,
		EEquipSlotType::Legs
	};

	float Total = 0.f;

	for (const EEquipSlotType ArmorSlot : ArmorSlots)
	{
		// 부서진 방어구는 껴 있어도 없는 것과 같다.
		if (IsEquippedItemBroken(ArmorSlot))
		{
			continue;
		}

		FItemInstance ArmorItem;
		if (!GetItemInSlot(ArmorSlot, ArmorItem))
		{
			continue;
		}

		// 방어구는 무기(FWeaponData)·방패(FShieldData)와 달리 전용 행 구조체가 없다.
		// 일반 아이템 표에 BonusStatValue 로 들어 있다.
		const FItemData* ArmorRow = ArmorItem.ItemRow.GetRow<FItemData>(TEXT("SumArmorDefense"));
		if (!ArmorRow)
		{
			continue;
		}

		// BonusStatType 문자열은 보지 않는다.
		// 슬롯이 이미 종류를 말해주고, FString 비교는 "방어력"/"DefenseRate" 같은
		// 표기 차이를 경고 없이 삼켜서 방어력이 조용히 0 이 되는 사고를 낸다.
		Total += ArmorRow->BonusStatValue;
	}

	return Total;
}

const FWeaponData* UEquipmentComponent::FindUsableWeaponRow() const
{
	// 부서진 무기로는 공격 자체가 안 나가므로(각 공격 GA 가 막는다) 기초 스탯도 없는 것으로 둔다.
	if (IsEquippedItemBroken(EEquipSlotType::PrimaryWeapon))
	{
		return nullptr;
	}

	FItemInstance WeaponItem;
	if (!GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
	{
		return nullptr;
	}

	return WeaponItem.ItemRow.GetRow<FWeaponData>(TEXT("FindUsableWeaponRow"));
}

float UEquipmentComponent::SumWeaponAttackPower() const
{
	const FWeaponData* WeaponRow = FindUsableWeaponRow();
	if (!WeaponRow)
	{
		return 0.f;
	}
	return WeaponRow->AttackPower;
}

float UEquipmentComponent::GetWeaponAttackSpeedRate() const
{
	const FWeaponData* WeaponRow = FindUsableWeaponRow();
	if (!WeaponRow)
	{
		return 1.f;
	}
	return WeaponRow->AttackSpeedRate;
}

void UEquipmentComponent::RefreshEquipmentStats()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;   // 클라도 OnRep_Slots 로 여기 들어온다. 어트리뷰트 변경은 서버 권위
	}

	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		// BeginPlay 시점에는 아직 없을 수 있다. 그 경우 다음 OnEquipmentChanged 때 들어와 채워진다.
		// 장착을 해도 계속 이 줄만 뜨면 소유자에게 ASC 가 없는 것이다 — 조용히 넘기지 않는다.
		UE_LOG(LogTemp, Warning, TEXT("[Equip] ASC 없음 — 스탯 갱신 건너뜀 | Owner=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	// 붙이기 전에 먼저 센다. 값이 그대로면 GE 를 건드릴 이유가 없다.
	const FItemStatTotals Totals = CollectStatTotals();

	// 내구도가 깎였을 뿐이면 여기서 끝난다. 파손은 합계가 실제로 달라지므로
	// 이 비교가 알아서 통과시킨다 — "내구도인가" 를 따로 물을 필요가 없다.
	if (EquipmentStatsHandle.IsValid())
	{
		if (AreTotalsEqual(Totals, LastAppliedTotals))
		{
			return;
		}
	}

	// 묵은 것을 먼저 뗀다. 안 떼면 갈아입을 때마다 겹쳐 쌓인다.
	// 떼는 순간 최대 체력이 잠깐 내려갔다 올라오지만, 현재 체력은 비율로 따라가서
	// 원래 값으로 돌아온다 (HB_PlayerAttributeSet::CalculateCurrentOnMaxChanged).
	if (EquipmentStatsHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(EquipmentStatsHandle);
		EquipmentStatsHandle.Invalidate();
	}

	// 아래에서 실패하면 묵은 GE 는 이미 떨어졌다. 캐시도 비워서 다음에 무조건 다시 붙게 한다.
	LastAppliedTotals.Reset();

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		UGE_EquipmentStats::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}

	// GE 의 모디파이어 칸을 전부 채운다. 아이템 동작 효과의 스탯 GE 도 같은 함수를 쓴다.
	ItemStats::FillSpec(*Spec.Data.Get(), Totals);

	EquipmentStatsHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	LastAppliedTotals = Totals;

	// 0 이 아닌 스탯만 찍는다
	UE_LOG(LogTemp, Log, TEXT("[Equip] 스탯 갱신"));
	for (const FItemStatBinding& Binding : ItemStats::GetBindings())
	{
		const FItemStatTotal Total = ItemStats::GetTotal(Totals, Binding.Stat);

		if (FMath::IsNearlyZero(Total.Flat))
		{
			if (FMath::IsNearlyZero(Total.Percent))
			{
				continue;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[Equip]   %s  +%.2f  %+.1f%%"),
			*Binding.DisplayName, Total.Flat, Total.Percent * 100.f);
	}
}

FItemStatTotals UEquipmentComponent::CollectStatTotals() const
{
	FItemStatTotals Totals;

	// 아이템 자체의 성능. 옵션이 아니다.
	Totals.FindOrAdd(ItemTag::TAG_Stat_AttackPower).Flat += SumWeaponAttackPower();
	Totals.FindOrAdd(ItemTag::TAG_Stat_Defense).Flat += SumArmorDefense();

	// 무기 공격 속도. 배율을 % 로 바꿔 옵션 한 줄로 넣는다 (0.8 → -20).
	// 옵션으로 넣는 이유: "10 = 10%" 환산을 AddOptionToTotals 한 곳에 두려고.
	FItemOption WeaponSpeed;
	WeaponSpeed.Stat = ItemTag::TAG_Stat_AttackSpeed;
	WeaponSpeed.ValueType = EItemOptionValueType::Percent;
	WeaponSpeed.Value = (GetWeaponAttackSpeedRate() - 1.f) * 100.f;
	ItemStats::AddOptionToTotals(WeaponSpeed, Totals);

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const EEquipSlotType SlotType = static_cast<EEquipSlotType>(Index);

		// 부서진 장비는 옵션도 없다
		if (IsEquippedItemBroken(SlotType))
		{
			continue;
		}

		FItemInstance Item;
		if (!GetItemInSlot(SlotType, Item))
		{
			continue;
		}

		// 각인. 아래 DT 조회보다 위에 둔다 — DT 조회가 실패해도 각인 옵션은 살아야 한다.
		// 좋은 쪽 나쁜 쪽 구분 없이 적힌 부호 그대로 더한다. 나뉜 건 툴팁 색뿐이다.
		AddOptionsTo(Item.Engraving.Benefits, Item.ItemRow.RowName, Totals);
		AddOptionsTo(Item.Engraving.Risks, Item.ItemRow.RowName, Totals);

		// 행 종류를 가리지 않는다. 무기·방패 행은 옵션 배열이 비어서 온다.
		FItemViewData ViewData;
		if (!ResolveItemViewData(Item.ItemRow, ViewData))
		{
			continue;
		}

		// 좋은 쪽 나쁜 쪽 구분 없이 적힌 부호 그대로 더한다. 나뉜 건 툴팁 색뿐이다.
		AddOptionsTo(ViewData.BenefitOptions, Item.ItemRow.RowName, Totals);
		AddOptionsTo(ViewData.RiskOptions, Item.ItemRow.RowName, Totals);
	}

	return Totals;
}

void UEquipmentComponent::AddOptionsTo(const TArray<FItemOption>& Options, FName SourceRow, FItemStatTotals& Totals) const
{
	for (const FItemOption& Option : Options)
	{
		// 계산은 ItemStats::AddOptionToTotals 가 한다. 아이템 동작 효과와 같은 계산을 쓰기 위해서다.
		// 여기서는 실패 이유를 나눠 알린다. 조용히 0 이 되지 않게.
		if (!ItemStats::FindBinding(Option.Stat))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Equip] %s: 표에 없는 스탯 %s — 무시"),
				*SourceRow.ToString(), *Option.Stat.ToString());
			continue;
		}

		// "공격속도 +10" 처럼 받을 칸이 없는 조합
		if (!ItemStats::AddOptionToTotals(Option, Totals))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Equip] %s: %s 는 이 타입(+/%%) 옵션을 받지 않는다 — 무시"),
				*SourceRow.ToString(), *Option.Stat.ToString());
			continue;
		}
	}
}

bool UEquipmentComponent::AreTotalsEqual(const FItemStatTotals& A, const FItemStatTotals& B)
{
	// 맵에 키가 있고 없고는 상관없다. 값이 0 이면 같은 것이다. 그래서 표를 기준으로 본다.
	for (const FItemStatBinding& Binding : ItemStats::GetBindings())
	{
		const FItemStatTotal TA = ItemStats::GetTotal(A, Binding.Stat);
		const FItemStatTotal TB = ItemStats::GetTotal(B, Binding.Stat);

		if (!FMath::IsNearlyEqual(TA.Flat, TB.Flat))
		{
			return false;
		}
		if (!FMath::IsNearlyEqual(TA.Percent, TB.Percent))
		{
			return false;
		}
	}
	return true;
}

void UEquipmentComponent::CollectEquippedEffects(TArray<FResolvedItemEffect>& OutEffects) const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const EEquipSlotType SlotType = static_cast<EEquipSlotType>(Index);

		// 부서진 장비는 효과도 없다
		if (IsEquippedItemBroken(SlotType))
		{
			continue;
		}

		FItemInstance Item;
		if (!GetItemInSlot(SlotType, Item))
		{
			continue;
		}

		FItemViewData ViewData;
		if (!ResolveItemViewData(Item.ItemRow, ViewData))
		{
			continue;
		}

		// 좋은 쪽 나쁜 쪽 구분 없이 담는다. 나뉜 건 툴팁 색뿐이다.
		AddResolvedEffects(ViewData.BenefitEffects, Item.ItemRow.RowName, OutEffects);
		AddResolvedEffects(ViewData.RiskEffects, Item.ItemRow.RowName, OutEffects);
	}
}

void UEquipmentComponent::AddResolvedEffects(const TArray<FItemEffect>& Effects, FName SourceRow, TArray<FResolvedItemEffect>& OutEffects) const
{
	for (const FItemEffect& Effect : Effects)
	{
		// DT 에 줄만 추가하고 정의를 안 골랐거나, 다른 표를 고른 경우
		FResolvedItemEffect Resolved;
		if (!ItemEffectRules::Resolve(Effect, Resolved))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Equip] %s: 효과 정의를 못 찾음 (%s) — 무시"),
				*SourceRow.ToString(), *Effect.Definition.RowName.ToString());
			continue;
		}

		// 정의에 태그가 없으면 규칙 코드가 이 효과를 구분할 수 없다 (핸들 · 쌓인 시간의 키가 태그다)
		if (!Resolved.Def.Tag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Equip] %s: 효과 정의 %s 에 Tag 가 없음 — 무시"),
				*SourceRow.ToString(), *Effect.Definition.RowName.ToString());
			continue;
		}

		// 같은 효과를 두 개 꼈으면 먼저 찾은 것만. 겹치지 않는다 — 주기 효과가 두 배로 빨라지는 일도 막는다.
		if (HasResolvedEffect(OutEffects, Resolved.Def.Tag))
		{
			continue;
		}

		OutEffects.Add(Resolved);
	}
}

bool UEquipmentComponent::HasResolvedEffect(const TArray<FResolvedItemEffect>& Effects, const FGameplayTag& Tag)
{
	for (const FResolvedItemEffect& Effect : Effects)
	{
		if (Effect.Def.Tag == Tag)
		{
			return true;
		}
	}
	return false;
}

TArray<FGameplayTag> UEquipmentComponent::CollectItemEffectTags() const
{
	// "꼈다" 표시 태그. 같은 태그는 CollectEquippedEffects 가 이미 하나로 줄였다.
	TArray<FResolvedItemEffect> Effects;
	CollectEquippedEffects(Effects);

	TArray<FGameplayTag> Tags;
	for (const FResolvedItemEffect& Effect : Effects)
	{
		Tags.Add(Effect.Def.Tag);
	}
	return Tags;
}

void UEquipmentComponent::RefreshItemEffects()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (!Owner->HasAuthority())
	{
		return;   // 클라도 OnRep_Slots 로 여기 들어온다. GE 는 서버가 건다
	}

	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		return;   // RefreshEquipmentStats 가 이미 경고를 찍었다
	}

	const TArray<FGameplayTag> Wanted = CollectItemEffectTags();

	// 목록이 그대로면 건드리지 않는다. 내구도만 깎인 경우다.
	if (AreSameTags(Wanted, AppliedItemEffectTags))
	{
		return;
	}

	// 묵은 것을 뗀다. 태그만 거는 표시라 떼고 붙여도 잃는 게 없다.
	// 주의: 쿨타임·주기 피해처럼 시간이 흐르는 건 여기 두면 안 된다 — 교체할 때마다 초기화된다.
	//       그런 건 동작 코드 쪽에 둔다.
	if (ItemEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(ItemEffectHandle);
		ItemEffectHandle.Invalidate();
	}
	AppliedItemEffectTags.Reset();

	// 효과가 없으면 빈 GE 를 붙이지 않는다.
	if (Wanted.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Equip] 동작 효과 없음"));
		return;
	}

	// 모든 태그를 GE 하나에 담는다. HBEffects::ApplyTaggedDuration 과 같은 방식이다.
	FGameplayEffectSpec Spec(GetDefault<UGE_ItemEffectTag>(), ASC->MakeEffectContext(), 1.f);
	for (const FGameplayTag& Tag : Wanted)
	{
		Spec.DynamicGrantedTags.AddTag(Tag);
	}

	ItemEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(Spec);
	AppliedItemEffectTags = Wanted;

	UE_LOG(LogTemp, Log, TEXT("[Equip] 동작 효과 %d 개 적용"), Wanted.Num());
	for (const FGameplayTag& Tag : Wanted)
	{
		UE_LOG(LogTemp, Log, TEXT("[Equip]   %s"), *Tag.ToString());
	}
}

bool UEquipmentComponent::AreSameTags(const TArray<FGameplayTag>& A, const TArray<FGameplayTag>& B)
{
	// 순서까지 같아야 같다고 본다. 모으는 순서(슬롯 → Benefit → Risk)가 매번 같아서 이걸로 충분하다.
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		if (A[Index] != B[Index])
		{
			return false;
		}
	}
	return true;
}

bool UEquipmentComponent::IsSlotOccupied(EEquipSlotType InSlot) const
{
	if (InSlot == EEquipSlotType::None || InSlot == EEquipSlotType::MAX)
	{
		return false;
	}

	return Slots[SlotToIndex(InSlot)].IsValid();
}

bool UEquipmentComponent::GetItemInSlot(EEquipSlotType InSlot, FItemInstance& OutItem) const
{
	if (!IsSlotOccupied(InSlot))
	{
		return false;
	}

	OutItem = Slots[SlotToIndex(InSlot)];
	return true;
}

bool UEquipmentComponent::DamageEquippedItemDurability(EEquipSlotType InSlot, float InAmount, bool& bOutJustBroke)
{
	bOutJustBroke = false;

	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!IsSlotOccupied(InSlot))
	{
		return false;
	}

	const int32 SlotIndex = SlotToIndex(InSlot);
	FItemInstance& Item = Slots[SlotIndex];

	FItemViewData ViewData;
	if (!ResolveItemViewData(Item.ItemRow, ViewData))
	{
		return false;
	}

	if (Item.CurrentDurability < 0.f)
	{
		Item.CurrentDurability = GetMaxDurabilityForGrade(ViewData.Grade);
	}

	if (Item.CurrentDurability <= 0.f)
	{
		return false;
	}

	const float PreviousDurability = Item.CurrentDurability;
	Item.CurrentDurability = FMath::Max(0.f, Item.CurrentDurability - InAmount);

	UE_LOG(LogTemp, Warning, TEXT("[Durability] Slot=%d %.1f -> %.1f"), (int32)InSlot, PreviousDurability, Item.CurrentDurability);

	if (PreviousDurability > 0.f && Item.CurrentDurability <= 0.f)
	{
		bOutJustBroke = true;
		if (InSlot == EEquipSlotType::PrimaryWeapon)
			UHBUIAudioLibrary::PlayForOwningClient(this, EHBUIAudioEvent::WeaponBreak);
	}

	OnEquipmentChanged.Broadcast();

	return true;
}

bool UEquipmentComponent::IsEquippedItemBroken(EEquipSlotType InSlot) const
{
	FItemInstance Item;
	if (!GetItemInSlot(InSlot, Item))
	{
		return false;
	}

	return Item.IsBroken();
}

bool UEquipmentComponent::FindSlotByInstanceId(FGuid InInstanceId, EEquipSlotType& OutSlot) const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].IsValid() && Slots[Index].InstanceId == InInstanceId)
		{
			OutSlot = static_cast<EEquipSlotType>(Index);
			return true;
		}
	}

	return false;
}

bool UEquipmentComponent::CanEquip(const FItemInstance& InItem, EEquipSlotType InSlot) const
{
	if (InSlot == EEquipSlotType::None || InSlot == EEquipSlotType::MAX)
	{
		return false;
	}

	// 내구도가 0(파손)인 장비는 애초에 장착할 수 없다.
	if (InItem.IsBroken())
	{
		return false;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(InItem.ItemRow, ViewData))
	{
		return false;
	}

	if (ViewData.EquipSlotType != InSlot)
	{
		return false;
	}

	return true;
}

bool UEquipmentComponent::IsSecondarySlotLockedByWeapon() const
{
	FItemInstance CurrentWeapon;
	if (!GetItemInSlot(EEquipSlotType::PrimaryWeapon, CurrentWeapon))
	{
		return false;
	}

	const FWeaponData* CurrentWeaponData = CurrentWeapon.ItemRow.GetRow<FWeaponData>(TEXT("IsSecondarySlotLockedByWeapon"));
	return CurrentWeaponData && IsShieldIncompatibleWeaponType(CurrentWeaponData->WeaponType);
}

bool UEquipmentComponent::EquipFromInventory(UInventoryComponent* InSource, FGuid InInstanceId, EEquipSlotType InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InSource == nullptr)
	{
		return false;
	}

	const int32 SourceIndex = InSource->FindEntryIndexById(InInstanceId);
	if (SourceIndex == INDEX_NONE)
	{
		return false;
	}

	const FItemInstance SourceItem = InSource->GetEntries()[SourceIndex].Item;
	const FIntPoint SourcePosition = InSource->GetEntries()[SourceIndex].Position;

	if (!CanEquip(SourceItem, InSlot))
	{
		return false;
	}

	if (InSlot == EEquipSlotType::SecondaryEquip && IsSecondarySlotLockedByWeapon())
	{
		if (!UnequipToInventory(EEquipSlotType::PrimaryWeapon, InSource))
		{
			return false;
		}
	}

	if (InSlot == EEquipSlotType::PrimaryWeapon && IsSlotOccupied(EEquipSlotType::SecondaryEquip))
	{
		if (const FWeaponData* NewWeaponData = SourceItem.ItemRow.GetRow<FWeaponData>(TEXT("EquipFromInventory")))
		{
			if (IsShieldIncompatibleWeaponType(NewWeaponData->WeaponType))
			{
				if (!UnequipToInventory(EEquipSlotType::SecondaryEquip, InSource))
				{
					return false;
				}
			}
		}
	}

	const int32 SlotIndex = SlotToIndex(InSlot);
	const FItemInstance PreviousItem = Slots[SlotIndex];

	FItemInstance RemovedItem;
	if (!InSource->RemoveItem(InInstanceId, RemovedItem))
	{
		return false;
	}

	RemovedItem.bRotated = false;

	if (PreviousItem.IsValid())
	{
		if (!InSource->TryAddItemAt(PreviousItem, SourcePosition))
		{
			int32 RollbackRemainder = 0;
			InSource->TryAddItem(RemovedItem, RollbackRemainder);
			return false;
		}
	}

	Slots[SlotIndex] = RemovedItem;

	// 스탯과 동작 효과는 이 신호를 받은 HandleEquipmentChanged 가 맞춘다.
	OnEquipmentChanged.Broadcast();

	return true;
}

bool UEquipmentComponent::EquipDirect(const FItemInstance& InItem, EEquipSlotType InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!CanEquip(InItem, InSlot) || IsSlotOccupied(InSlot))
	{
		return false;
	}

	if (InSlot == EEquipSlotType::SecondaryEquip && IsSecondarySlotLockedByWeapon())
	{
		return false;
	}

	if (InSlot == EEquipSlotType::PrimaryWeapon && IsSlotOccupied(EEquipSlotType::SecondaryEquip))
	{
		if (const FWeaponData* NewWeaponData = InItem.ItemRow.GetRow<FWeaponData>(TEXT("EquipDirect")))
		{
			if (IsShieldIncompatibleWeaponType(NewWeaponData->WeaponType))
			{
				return false;
			}
		}
	}

	FItemInstance NewItem = InItem;
	NewItem.bRotated = false;
	Slots[SlotToIndex(InSlot)] = NewItem;

	OnEquipmentChanged.Broadcast();

	return true;
}

bool UEquipmentComponent::ToggleFavoriteInSlot(EEquipSlotType InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!IsSlotOccupied(InSlot))
	{
		return false;
	}

	FItemInstance& SlotItem = Slots[SlotToIndex(InSlot)];
	SlotItem.bIsFavorite = !SlotItem.bIsFavorite;

	OnEquipmentChanged.Broadcast();

	return true;
}

bool UEquipmentComponent::UnequipToInventory(EEquipSlotType InSlot, UInventoryComponent* InTarget)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InTarget == nullptr || !IsSlotOccupied(InSlot))
	{
		return false;
	}

	const int32 SlotIndex = SlotToIndex(InSlot);
	const FItemInstance EquippedItem = Slots[SlotIndex];

	int32 Remainder = 0;
	if (!InTarget->TryAddItem(EquippedItem, Remainder))
	{
		return false;
	}

	Slots[SlotIndex] = FItemInstance();

	OnEquipmentChanged.Broadcast();

	return true;
}

bool UEquipmentComponent::UnequipToInventoryAt(EEquipSlotType InSlot, UInventoryComponent* InTarget, FIntPoint InTargetPosition)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (InTarget == nullptr || !IsSlotOccupied(InSlot))
	{
		return false;
	}

	const int32 SlotIndex = SlotToIndex(InSlot);
	const FItemInstance EquippedItem = Slots[SlotIndex];

	if (!InTarget->TryAddItemAt(EquippedItem, InTargetPosition))
	{
		return false;
	}

	Slots[SlotIndex] = FItemInstance();

	OnEquipmentChanged.Broadcast();

	return true;
}

bool UEquipmentComponent::ClearSlot(EEquipSlotType InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!IsSlotOccupied(InSlot))
	{
		return false;
	}

	Slots[SlotToIndex(InSlot)] = FItemInstance();

	OnEquipmentChanged.Broadcast();

	return true;
}

void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEquipmentComponent, Slots);
	DOREPLIFETIME_CONDITION(UEquipmentComponent, OpenedBagId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UEquipmentComponent, OpenedBagRow, COND_OwnerOnly);
}

void UEquipmentComponent::DebugPrintSlots() const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Index == static_cast<int32>(EEquipSlotType::None))
		{
			continue;
		}

		const UEnum* SlotEnum = StaticEnum<EEquipSlotType>();
		const FString SlotName = SlotEnum ? SlotEnum->GetNameStringByValue(Index) : FString::FromInt(Index);

		if (!Slots[Index].IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("%s: empty"), *SlotName);
			continue;
		}

		FItemViewData ViewData;
		ResolveItemViewData(Slots[Index].ItemRow, ViewData);

		UE_LOG(LogTemp, Log, TEXT("%s: %s"), *SlotName, *ViewData.DisplayName);
	}
}

void UEquipmentComponent::ApplyLoadedSlots(const TArray<FItemInstance>& InSlots)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 Count = FMath::Min(Slots.Num(), InSlots.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Slots[Index] = InSlots[Index];
	}

	OnRep_Slots();
}
