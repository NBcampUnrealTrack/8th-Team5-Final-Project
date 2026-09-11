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
    if (!SkillIconImage)
    {
        return;
    }

    SkillIconImage->SetBrushFromTexture(NewIcon);

    // 널이 오면 빈 슬롯이라는 뜻이다.
    //
    // 왜 고쳤나: 예전에는 널일 때 아무것도 하지 않아 직전 아이콘이 그대로
    // 남았다. 슬롯을 비우면 비운 것처럼 보여야 한다.
    if (!NewIcon)
    {
        SkillIconImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    SkillIconImage->SetVisibility(ESlateVisibility::Visible);

    FLinearColor Color = SkillIconImage->GetColorAndOpacity();
    Color.A = 1.0f;
    SkillIconImage->SetColorAndOpacity(Color);
}