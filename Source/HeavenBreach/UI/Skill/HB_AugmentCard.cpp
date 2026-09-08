// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Skill/HB_AugmentCard.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UHB_AugmentCard::NativeOnInitialized()
{
    Super::NativeOnInitialized();
}

void UHB_AugmentCard::NativeOnClicked()
{
    Super::NativeOnClicked();

    if (OnAugmentCardClickedDelegate.IsBound())
    {
        OnAugmentCardClickedDelegate.Broadcast(CachedAugmentData.AugmentId);
    }
}

void UHB_AugmentCard::InitCard(const FAugmentCardData& InData)
{
    CachedAugmentData = InData;

    if (TitleText)
    {
        TitleText->SetText(InData.DisplayName);
    }

    if (DescriptionText)
    {
        DescriptionText->SetText(InData.Description);
    }

    if (IconImage && InData.Icon)
    {
        IconImage->SetBrushFromTexture(InData.Icon);
    }
}