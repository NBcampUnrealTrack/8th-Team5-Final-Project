// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillDragDropOperation.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

UHB_SkillDragDropOperation* UHB_SkillDragDropOperation::Create(
	UObject* Outer,
	FName InSkillId,
	ESkillType InSkillType,
	ESkillSlotType InSourceSlot,
	UTexture2D* InIcon,
	float InVisualSize)
{
	UHB_SkillDragDropOperation* Operation = NewObject<UHB_SkillDragDropOperation>(Outer);
	Operation->SkillId = InSkillId;
	Operation->SkillType = InSkillType;
	Operation->SourceSlot = InSourceSlot;
	Operation->Pivot = EDragPivot::CenterCenter;

	if (InIcon)
	{
		// 끄는 동안 커서에 붙어 다닐 아이콘. 별도 WBP 없이 이미지 하나로 충분하다.
		UImage* Visual = NewObject<UImage>(Operation);
		Visual->SetBrushFromTexture(InIcon);
		Visual->SetDesiredSizeOverride(FVector2D(InVisualSize, InVisualSize));
		Visual->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.85f));
		Operation->DefaultDragVisual = Visual;
	}

	return Operation;
}

bool UHB_SkillDragDropOperation::CanSlotAccept(ESkillSlotType InSlotType, ESkillType InSkillType)
{
	switch (InSlotType)
	{
	case ESkillSlotType::Active_Q:
	case ESkillSlotType::Active_E:
		return InSkillType == ESkillType::Active;
	case ESkillSlotType::Passive:
		return InSkillType == ESkillType::Passive;
	default:
		return false;
	}
}
