// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Inventory/InventoryComponent.h"
#include "LootComponent.generated.h"

struct FEnemyLootRule;
struct FEnemyLootTableRow;

/**
 * 적이 죽을 때 드랍을 만들어 자기 안에 담는 컴포넌트.
 * UInventoryComponent 를 상속하므로 담긴 것이 곧 시체를 열었을 때 보이는 목록이다.
 *
 * 값은 하나도 갖지 않는다. 어느 테이블의 어느 행을 볼지만 알고 나머지는 전부 그 행에서 읽는다.
 */
UCLASS()
class HEAVENBREACH_API ULootComponent : public UInventoryComponent
{
	GENERATED_BODY()

public:
	// 루팅 가능한 아이템을 채워넣기. 사망 시 서버에서만 불린다
	void GenerateLoot();

	// 클라에게 UI열라고 명령
	void HandleInteract(AActor* Interactor);

	FText GetPrompt() const { return InteractPrompt; }

	// 스탯 DT 가 주입한다. BP 에서 건드리지 않는다
	void SetLootRow(const FDataTableRowHandle& InRow) { LootRow = InRow; }

protected:
	// 상호작용시 띄울 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FText InteractPrompt = FText::FromString(TEXT("루팅 [F]"));

	// 볼 FEnemyLootTableRow. 스탯 DT 의 LootRow 가 주입한다.
	//
	// 왜 BP 에서 못 고치나: 여기서 고칠 수 있으면 스탯 DT 와 두 개의 진실이 된다.
	// 적 BP 에서는 드랍에 대해 아무것도 설정하지 않는 게 맞다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	FDataTableRowHandle LootRow;

private:
	// 가중치로 규칙 하나를 고른다. 후보가 없으면 nullptr
	const FEnemyLootRule* PickRandomRule(const TArray<FEnemyLootRule>& Rules) const;

	// 규칙 하나를 실행해 아이템을 넣는다. 실제로 넣은 개수를 돌려준다.
	// 소스 테이블이 행에 있어서 행을 같이 넘긴다
	int32 RunLootRule(const FEnemyLootRule& Rule, const FEnemyLootTableRow& Source);

	// 아이템 하나를 Count 개 넣는다. 쌓이면 한 번에, 아니면 하나씩
	int32 AddLootItem(const FDataTableRowHandle& Row, int32 Count);
};
