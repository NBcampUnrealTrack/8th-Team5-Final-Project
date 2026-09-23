#include "EquipmentComponent.h"

#include "Net/UnrealNetwork.h"
#include "InventoryComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Player/HBPlayerCharacter.h"
#include "Item/Item.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Item/HB_ItemTag.h"

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

void UEquipmentComponent::ApplyRelicEffects(const FItemInstance& InItem)
{
	UE_LOG(LogTemp, Warning, TEXT("[Relic] ApplyRelicEffects CALLED"));

	FItemViewData ViewData;
	if (!ResolveItemViewData(InItem.ItemRow, ViewData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] ResolveItemViewData FAILED"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Relic] bIsRelic = %s"), ViewData.bIsRelic ? TEXT("true") : TEXT("false"));

	if (!ViewData.bIsRelic)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] ASC is NULL"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Relic] BenefitEffect valid: %s, RiskEffect valid: %s"),
		ViewData.BenefitEffect ? TEXT("true") : TEXT("false"),
		ViewData.RiskEffect ? TEXT("true") : TEXT("false"));

	if (ViewData.BenefitEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		EquippedRelicBenefitHandle = ASC->ApplyGameplayEffectToSelf(
			ViewData.BenefitEffect.GetDefaultObject(), 1.f, Context);
	}

	if (ViewData.RiskEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		EquippedRelicRiskHandle = ASC->ApplyGameplayEffectToSelf(
			ViewData.RiskEffect.GetDefaultObject(), 1.f, Context);
	}
}

void UEquipmentComponent::RemoveRelicEffects()
{
	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC == nullptr)
	{
		return;
	}

	if (EquippedRelicBenefitHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(EquippedRelicBenefitHandle);
		EquippedRelicBenefitHandle.Invalidate();
	}

	if (EquippedRelicRiskHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(EquippedRelicRiskHandle);
		EquippedRelicRiskHandle.Invalidate();
	}
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
	OnEquipmentChanged.AddUniqueDynamic(this, &UEquipmentComponent::RefreshEquipmentStats);

	// 이미 채워진 채로 시작하는 경우(세이브 복원이 BeginPlay 보다 빨랐을 때)를 위해 한 번 돈다.
	RefreshEquipmentStats();
}

void UEquipmentComponent::OnRep_Slots()
{
	OnEquipmentChanged.Broadcast();
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

float UEquipmentComponent::SumWeaponAttackPower() const
{
	// 부서진 무기로는 공격 자체가 안 나가므로(각 공격 GA 가 막는다) 공격력도 0 으로 둔다.
	if (IsEquippedItemBroken(EEquipSlotType::PrimaryWeapon))
	{
		return 0.f;
	}

	FItemInstance WeaponItem;
	if (!GetItemInSlot(EEquipSlotType::PrimaryWeapon, WeaponItem))
	{
		return 0.f;
	}

	const FWeaponData* WeaponRow = WeaponItem.ItemRow.GetRow<FWeaponData>(TEXT("SumWeaponAttackPower"));
	if (!WeaponRow)
	{
		return 0.f;
	}

	return WeaponRow->AttackPower;
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
	const float TotalDefense = SumArmorDefense();
	const float TotalAttackPower = SumWeaponAttackPower();

	// 내구도가 깎였을 뿐이면 여기서 끝난다. 파손은 합계가 실제로 달라지므로
	// 이 비교가 알아서 통과시킨다 — "내구도인가" 를 따로 물을 필요가 없다.
	if (EquipmentStatsHandle.IsValid())
	{
		const bool bUnchanged =
			FMath::IsNearlyEqual(TotalDefense, LastAppliedDefense)
			&& FMath::IsNearlyEqual(TotalAttackPower, LastAppliedAttackPower);

		if (bUnchanged)
		{
			return;
		}
	}

	// 묵은 것을 먼저 뗀다. 안 떼면 갈아입을 때마다 겹쳐 쌓인다.
	if (EquipmentStatsHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(EquipmentStatsHandle);
		EquipmentStatsHandle.Invalidate();
	}

	// 여기서 실패하면 묵은 GE 는 이미 떨어진 상태다. 캐시도 같이 버려서
	// 다음 호출이 무조건 다시 시도하게 둔다.
	LastAppliedDefense = -1.f;
	LastAppliedAttackPower = -1.f;

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		UGE_EquipmentStats::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(ItemTag::TAG_Data_Equipment_Defense, TotalDefense);
	Spec.Data->SetSetByCallerMagnitude(ItemTag::TAG_Data_Equipment_AttackPower, TotalAttackPower);

	EquipmentStatsHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	LastAppliedDefense = TotalDefense;
	LastAppliedAttackPower = TotalAttackPower;

	UE_LOG(LogTemp, Log, TEXT("[Equip] 스탯 갱신 | Defense=%.2f AttackPower=%.2f"),
		TotalDefense, TotalAttackPower);
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

	FItemViewData ViewData;
	if (!ResolveItemViewData(InItem.ItemRow, ViewData))
	{
		return false;
	}

	const UEnum* SlotEnum = StaticEnum<EEquipSlotType>();

	return ViewData.EquipSlotType == InSlot;
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

	UE_LOG(LogTemp, Warning, TEXT("[Relic] EquipFromInventory: InSlot = %d (Accessory = %d)"),
		(int32)InSlot, (int32)EEquipSlotType::Accessory);

	if (InSlot == EEquipSlotType::Accessory)
	{
		ApplyRelicEffects(RemovedItem);
	}

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

	if (InSlot == EEquipSlotType::Accessory)
	{
		RemoveRelicEffects();
	}

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

	if (InSlot == EEquipSlotType::Accessory)
	{
		RemoveRelicEffects();
	}

	OnEquipmentChanged.Broadcast();

	return true;
}

void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEquipmentComponent, Slots);
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
