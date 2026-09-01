// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Skill/HB_SkillSlotWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"


void UHB_SkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &UHB_SkillSlotWidget::HandleSlotButtonClicked);
    }
}

void UHB_SkillSlotWidget::HandleSlotButtonClicked()
{
	OnSlotClicked.Broadcast(SlotType);
}

void UHB_SkillSlotWidget::SetSkillIcon(UTexture2D* NewIcon)
{
    if (SkillIconImage && NewIcon)
    {
        SkillIconImage->SetBrushFromTexture(NewIcon);

        SkillIconImage->SetVisibility(ESlateVisibility::Visible);

        FLinearColor Color = SkillIconImage->GetColorAndOpacity();
        Color.A = 1.0f;
        SkillIconImage->SetColorAndOpacity(Color);
    }
}