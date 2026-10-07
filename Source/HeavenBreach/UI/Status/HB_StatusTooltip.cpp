// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Status/HB_StatusTooltip.h"
#include "Combat/Ability/HBSkillAugment.h"
#include "Player/Job/HBJobData.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UHB_StatusTooltip::SetAugment(const FHBSkillAugment& Augment, int32 Tier)
{
	Fill(Augment.Icon.LoadSynchronous(), Augment.DisplayName, Augment.GetFormattedDescription(), Tier);
}

void UHB_StatusTooltip::SetPassive(const FHBJobSkillData& Passive)
{
	Fill(Passive.Icon.LoadSynchronous(), Passive.DisplayName, Passive.GetFormattedDescription(), 1);
}

void UHB_StatusTooltip::SetPlain(const FSlateBrush* IconBrush, const FText& Title, const FText& Description)
{
	if (IconImage && IconBrush)
	{
		IconImage->SetBrush(*IconBrush);
	}

	Fill(nullptr, Title, Description, 1);
}

void UHB_StatusTooltip::Fill(UTexture2D* Icon, const FText& Name, const FText& Description, int32 Tier)
{
	// 아이콘이 없으면 디자이너가 넣어 둔 기본 이미지를 그대로 둔다.
	if (IconImage && Icon)
	{
		IconImage->SetBrushFromTexture(Icon);
	}

	if (NameText)
	{
		NameText->SetText(Name);
	}

	if (TierText)
	{
		const bool bShowTier = Tier > 1;
		TierText->SetVisibility(bShowTier ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowTier)
		{
			TierText->SetText(FText::Format(NSLOCTEXT("StatusWindow", "AugmentTier", "Lv.{0}"), FText::AsNumber(Tier)));
		}
	}

	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
	}
}
