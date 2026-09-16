// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Inventory/InventoryTypes.h"
#include "EnemyLootTableRow.generated.h"

/**
 * 드랍 규칙 하나. 뽑히면 아이템 한 종류를 준다.
 *
 * 규칙 하나가 드랍 한 자리를 차지한다. 그래야 "잡몹은 1~3개" 가 그대로 표현된다.
 * 골드처럼 양이 필요한 것은 자리를 하나만 쓰고 MinCount/MaxCount 로 스택을 늘린다.
 */
USTRUCT(BlueprintType)
struct FEnemyLootRule
{
	GENERATED_BODY()

	// 로그와 에디터 표시용. 동작에는 영향이 없다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName RuleName = TEXT("Rule");

	// RandomRules 에서 이 규칙이 뽑힐 가중치. GuaranteedRules 에서는 쓰지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.f;

	// 채우면 추첨을 건너뛰고 이 행을 그대로 준다.
	// 보스의 성역의 핵처럼 반드시 그것이어야 할 때 쓴다.
	// 테이블까지 고를 수 있어서 무기 · 방패도 지정할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FDataTableRowHandle FixedItem;

	// ----- 아래는 FixedItem 이 비었을 때만 쓴다 -----

	// ItemDataTable 에서 걸러낼 ItemType. 비우면 ItemDataTable 을 아예 안 본다.
	// Material / Currency / key / Accessory / Helmet / Chest / Legs / Junk / Consumable
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FString> ItemTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bIncludeWeapons = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bIncludeShields = false;

	// 어느 등급이 나오나. 가중치가 0 이거나 그 등급에 아이템이 없으면 후보에서 빠진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TMap<EItemGrade, float> ItemGradeWeights;

	// ----- 개수 -----

	// 한 자리를 차지했을 때 몇 개를 주나. 드랍 개수는 1 로 세고 스택만 늘어난다.
	// 안 쌓이는 아이템이면 그 개수만큼 따로 들어간다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};

/**
 * 드랍 테이블의 행 하나. "이런 놈은 이렇게 떨군다" 한 세트다.
 *
 * 행 이름은 등급으로 묶어도 되고(Normal / Strong / Elite / Boss)
 * 특정 적 전용으로 만들어도 된다(Warden). 섞여도 상관없다.
 * 어느 행을 쓸지는 FEnemyStatRow::LootRow 가 정한다.
 */
USTRUCT(BlueprintType)
struct FEnemyLootTableRow : public FTableRowBase
{
	GENERATED_BODY()

	// ----- 소스 -----

	// 뽑을 아이템의 출처.
	//
	// 왜 행이 갖나: 컴포넌트에 두면 적 BP 마다 세 개를 다시 연결해야 하고
	// 하나 빠뜨리면 조용히 안 나온다. 행은 몇 개 안 되니 여기 적는 게 싸다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TObjectPtr<UDataTable> WeaponDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source")
	TObjectPtr<UDataTable> ShieldDataTable;

	// ----- 총량 -----

	// 총 드랍 개수. 아래 보장분을 포함한다.
	// 보장분이 총량보다 많으면 보장분이 전부 나오고 랜덤은 0회가 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Draws", meta = (ClampMin = "0"))
	int32 MinTotalDraws = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Draws", meta = (ClampMin = "0"))
	int32 MaxTotalDraws = 3;

	// ----- 규칙 -----

	// 반드시 나온다. 각각 한 자리씩 차지한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (TitleProperty = "RuleName"))
	TArray<FEnemyLootRule> GuaranteedRules;

	// 남은 자리를 채운다. 매번 SelectionWeight 로 규칙을 고른다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (TitleProperty = "RuleName"))
	TArray<FEnemyLootRule> RandomRules;
};
