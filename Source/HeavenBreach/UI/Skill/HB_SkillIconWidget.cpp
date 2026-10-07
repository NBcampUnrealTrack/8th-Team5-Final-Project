// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillIconWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UHB_SkillIconWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 만든 직후 SetSkillIcon 이 불려도 기본 브러시가 캐싱돼 있도록 NativeConstruct 가 아니라 여기서 한다.
	if (SkillIconImage)
	{
		DefaultIconBrush = SkillIconImage->GetBrush();
	}
}

void UHB_SkillIconWidget::SetSkillIcon(UTexture2D* NewIcon)
{
	if (!SkillIconImage)
	{
		return;
	}

	if (NewIcon)
	{
		SkillIconImage->SetBrushFromTexture(NewIcon);
	}
	else
	{
		SkillIconImage->SetBrush(DefaultIconBrush);
	}
}
