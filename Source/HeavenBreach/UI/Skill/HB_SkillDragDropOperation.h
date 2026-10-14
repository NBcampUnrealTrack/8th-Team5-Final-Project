// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "UI/Skill/HB_SkillData.h"
#include "UI/Skill/HB_SkillSlotWidget.h"
#include "HB_SkillDragDropOperation.generated.h"

class UTexture2D;

/**
 * 로비 스킬 선택 화면에서 스킬 아이콘을 끌고 있는 동안 들고 다니는 정보.
 *
 * 끌기 시작점은 둘이다.
 *   - 오른쪽 스킬 목록의 아이콘 (SourceSlot == None)
 *   - 왼쪽 장착 슬롯 (SourceSlot == 그 슬롯. 다른 슬롯으로 옮기거나 밖으로 끌어내 해제할 때)
 *
 * 놓을 곳은 장착 슬롯이다. 슬롯 종류와 스킬 종류가 맞아야 받는다.
 */
UCLASS()
class HEAVENBREACH_API UHB_SkillDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	/**
	 * 끌기 정보를 만들고 끄는 동안 커서를 따라다닐 아이콘을 붙인다.
	 * 아이콘이 없으면 시각 표시 없이 정보만 든다.
	 */
	static UHB_SkillDragDropOperation* Create(
		UObject* Outer,
		FName InSkillId,
		ESkillType InSkillType,
		ESkillSlotType InSourceSlot,
		UTexture2D* InIcon,
		float InVisualSize);

	/** 이 슬롯 종류가 이 스킬을 받을 수 있는가. Q/E 는 액티브, 패시브 칸은 패시브만 받는다. */
	static bool CanSlotAccept(ESkillSlotType InSlotType, ESkillType InSkillType);

	UPROPERTY(BlueprintReadOnly, Category = "Skill Drag")
	FName SkillId;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Drag")
	ESkillType SkillType = ESkillType::Active;

	/** 장착 슬롯에서 끌기 시작했으면 그 슬롯. 목록에서 끌었으면 None. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Drag")
	ESkillSlotType SourceSlot = ESkillSlotType::None;
};
