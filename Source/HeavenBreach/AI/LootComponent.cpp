// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LootComponent.h"

#include "Engine/DataTable.h"
#include "Interaction/InteractionComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Item/Item.h"
#include "AI/DataRow/EnemyLootTableRow.h"
#include "AI/Enum/EAlertState.h"

namespace
{
	struct FLootCandidate
	{
		UDataTable* Table = nullptr;
		FName RowName = NAME_None;
	};

	// DT_Item 은 재료 · 골드 · 잡템 · 방어구가 섞여 있어서 ItemType 으로 걸러야 한다.
	// 안 거르면 장비를 뽑으려는데 잡템이 나온다.
	void CollectItemsByGrade(UDataTable* Table, const TArray<FString>& Types, TMap<EItemGrade, TArray<FLootCandidate>>& Out)
	{
		if (!Table)
		{
			return;
		}
		if (Types.Num() == 0)
		{
			return;
		}

		TArray<FItemData*> Rows;
		Table->GetAllRows<FItemData>(TEXT("ULootComponent::CollectItemsByGrade"), Rows);
		const TArray<FName> Names = Table->GetRowNames();

		for (int32 i = 0; i < Rows.Num() && i < Names.Num(); ++i)
		{
			if (!Rows[i])
			{
				continue;
			}
			if (!Types.Contains(Rows[i]->ItemType))
			{
				continue;
			}

			FLootCandidate Candidate;
			Candidate.Table = Table;
			Candidate.RowName = Names[i];
			Out.FindOrAdd(Rows[i]->Grade).Add(Candidate);
		}
	}

	// DT_Weapon / DT_Shield 는 전부 장비라 거를 게 없다.
	template<typename TRow>
	void CollectAllByGrade(UDataTable* Table, TMap<EItemGrade, TArray<FLootCandidate>>& Out)
	{
		if (!Table)
		{
			return;
		}

		TArray<TRow*> Rows;
		Table->GetAllRows<TRow>(TEXT("ULootComponent::CollectAllByGrade"), Rows);
		const TArray<FName> Names = Table->GetRowNames();

		for (int32 i = 0; i < Rows.Num() && i < Names.Num(); ++i)
		{
			if (!Rows[i])
			{
				continue;
			}

			FLootCandidate Candidate;
			Candidate.Table = Table;
			Candidate.RowName = Names[i];
			Out.FindOrAdd(Rows[i]->Grade).Add(Candidate);
		}
	}
}

void ULootComponent::GenerateLoot()
{
	// 서버에서만 동작해야함
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 스탯 DT 에서 주입받지 못한 경우. 그 행의 LootRow 가 비어 있다
	if (LootRow.IsNull())
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Loot] LootRow 미설정: %s"), *GetNameSafe(GetOwner()));
		return;
	}

	const FEnemyLootTableRow* Source = LootRow.GetRow<FEnemyLootTableRow>(TEXT("ULootComponent::GenerateLoot"));
	if (!Source)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Loot] 행 없음 '%s': %s"), *LootRow.RowName.ToString(), *GetNameSafe(GetOwner()));
		return;
	}

	// 이번에 몇 자리를 채울지 먼저 정한다. 보장분이 그 자리를 먼저 가져간다.
	const int32 MinTotal = FMath::Max(0, Source->MinTotalDraws);
	const int32 MaxTotal = FMath::Max(MinTotal, Source->MaxTotalDraws);
	const int32 TotalDraws = FMath::RandRange(MinTotal, MaxTotal);

	int32 AddedCount = 0;

	// 보장분. 총량을 넘겨도 전부 나온다. "반드시" 가 총량보다 우선이다.
	for (const FEnemyLootRule& Rule : Source->GuaranteedRules)
	{
		AddedCount += RunLootRule(Rule, *Source);
	}

	// 남은 자리를 가중 추첨으로 채운다
	const int32 RandomDraws = FMath::Max(0, TotalDraws - Source->GuaranteedRules.Num());
	for (int32 d = 0; d < RandomDraws; ++d)
	{
		const FEnemyLootRule* Picked = PickRandomRule(Source->RandomRules);
		if (!Picked)
		{
			break;
		}

		AddedCount += RunLootRule(*Picked, *Source);
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Loot] %s (%s) 총 %d자리 → %d개"),
		*GetNameSafe(GetOwner()), *LootRow.RowName.ToString(), TotalDraws, AddedCount);
}

const FEnemyLootRule* ULootComponent::PickRandomRule(const TArray<FEnemyLootRule>& Rules) const
{
	float TotalWeight = 0.f;
	for (const FEnemyLootRule& Rule : Rules)
	{
		TotalWeight += FMath::Max(0.f, Rule.SelectionWeight);
	}

	if (TotalWeight <= 0.f)
	{
		return nullptr;
	}

	// 누적이 굴린 값을 처음 넘는 구간이 당첨
	float Roll = FMath::FRand() * TotalWeight;
	for (const FEnemyLootRule& Rule : Rules)
	{
		Roll -= FMath::Max(0.f, Rule.SelectionWeight);
		if (Roll <= 0.f)
		{
			return &Rule;
		}
	}

	return &Rules.Last();
}

int32 ULootComponent::RunLootRule(const FEnemyLootRule& Rule, const FEnemyLootTableRow& Source)
{
	const int32 MinCount = FMath::Max(1, Rule.MinCount);
	const int32 MaxCount = FMath::Max(MinCount, Rule.MaxCount);
	const int32 Count = FMath::RandRange(MinCount, MaxCount);

	// 행을 직접 지정했으면 추첨하지 않는다
	if (!Rule.FixedItem.IsNull())
	{
		return AddLootItem(Rule.FixedItem, Count);
	}

	// 이 규칙이 뽑을 수 있는 후보를 등급별로 모은다
	TMap<EItemGrade, TArray<FLootCandidate>> ByGrade;
	CollectItemsByGrade(Source.ItemDataTable, Rule.ItemTypes, ByGrade);

	if (Rule.bIncludeWeapons)
	{
		CollectAllByGrade<FWeaponData>(Source.WeaponDataTable, ByGrade);
	}
	if (Rule.bIncludeShields)
	{
		CollectAllByGrade<FShieldData>(Source.ShieldDataTable, ByGrade);
	}

	if (ByGrade.Num() == 0)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Loot] 규칙 '%s' 후보 없음: %s"), *Rule.RuleName.ToString(), *GetNameSafe(GetOwner()));
		return 0;
	}

	// 가중치가 0 이거나 그 등급에 아이템이 없으면 후보에서 뺀다
	TArray<TPair<EItemGrade, float>> Weights;
	float TotalWeight = 0.f;
	for (const TPair<EItemGrade, float>& W : Rule.ItemGradeWeights)
	{
		if (W.Value <= 0.f)
		{
			continue;
		}
		if (!ByGrade.Contains(W.Key))
		{
			continue;
		}

		Weights.Add(W);
		TotalWeight += W.Value;
	}

	if (Weights.Num() == 0)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Loot] 규칙 '%s' 뽑을 등급 없음: %s"), *Rule.RuleName.ToString(), *GetNameSafe(GetOwner()));
		return 0;
	}

	// 등급 가중 랜덤
	float Roll = FMath::FRand() * TotalWeight;
	EItemGrade PickedGrade = Weights.Last().Key;
	for (const TPair<EItemGrade, float>& W : Weights)
	{
		Roll -= W.Value;
		if (Roll <= 0.f)
		{
			PickedGrade = W.Key;
			break;
		}
	}

	// 그 등급 풀에서 하나를 균등 랜덤
	const TArray<FLootCandidate>* Pool = ByGrade.Find(PickedGrade);
	if (!Pool || Pool->Num() == 0)
	{
		return 0;
	}

	const FLootCandidate& Candidate = (*Pool)[FMath::RandRange(0, Pool->Num() - 1)];

	FDataTableRowHandle RowHandle;
	RowHandle.DataTable = Candidate.Table;
	RowHandle.RowName = Candidate.RowName;

	return AddLootItem(RowHandle, Count);
}

int32 ULootComponent::AddLootItem(const FDataTableRowHandle& Row, int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}

	FItemViewData ViewData;
	if (!ResolveItemViewData(Row, ViewData))
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Loot] 아이템 행 해석 실패: %s"), *Row.RowName.ToString());
		return 0;
	}

	FItemInstance Item;
	Item.ItemRow = Row;
	Item.InstanceId = FGuid::NewGuid();
	Item.StackCount = 1;
	// 시체에 갓 생성된 아이템은 아직 서칭 전 - 화면이 열린 뒤 BeginItemRevealSequence()가
	// 순서대로 공개해줄 때까지 "?"로 보인다.
	Item.bRevealed = false;

	if (ViewData.IsEquippable())
	{
		Item.CurrentDurability = GetMaxDurabilityForGrade(ViewData.Grade);
	}

	// 쌓이는 건 한 번에 넣는다. 골드 500개를 TryAddItem 500번 부르지 않기 위해서.
	// TryAddItem 은 안 쌓이는 아이템에 StackCount > 1 을 주면 거부한다.
	if (ViewData.IsStackable())
	{
		Item.StackCount = Count;

		int32 Remainder = 0;
		TryAddItem(Item, Remainder);
		return Count - Remainder;
	}

	// 안 쌓이는 건 하나씩. InstanceId 를 매번 새로 만들어야 한다.
	int32 Added = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		Item.InstanceId = FGuid::NewGuid();

		int32 Remainder = 0;
		TryAddItem(Item, Remainder);
		if (Remainder == 0)
		{
			++Added;
		}
	}

	return Added;
}

void ULootComponent::HandleInteract(AActor* Interactor)
{
	// 서버에서만 동작해야함
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Interactor)
	{
		return;
	}

	// 서칭 안 된(bRevealed=false) 아이템이 있으면 하나씩 공개하는 타이머를 시작한다.
	// 이미 돌고 있거나 전부 공개된 상태면 아무것도 안 하므로 매번 호출해도 안전 -
	// 화면을 닫았다가 다시 열어도 처음부터 다시 돌지 않고 이어서 진행된다.
	BeginItemRevealSequence();

	// 플레이어의 인벤토리 UI열기위해 상호작용 컴포넌트에 보낸다.
	if (UInteractionComponent* InteractionComp = Interactor->FindComponentByClass<UInteractionComponent>())
	{
		InteractionComp->Client_OpenContainer(this);
	}
}
