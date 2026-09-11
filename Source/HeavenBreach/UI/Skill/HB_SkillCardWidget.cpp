// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillCardWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UHB_SkillCardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (CardButton)
    {
        // UButton 전용 다이나믹 바인딩 사용
        CardButton->OnClicked.AddDynamic(this, &UHB_SkillCardWidget::OnButtonClicked);
    }
}

void UHB_SkillCardWidget::InitCard(const FSkillCardData& SkillData, bool bIsAlreadySelected)
{
    // 1. 데이터 캐싱 및 UI 텍스트/이미지 적용
    CurrentSkillId = SkillData.SkillId;

    if (DisplayNameText) DisplayNameText->SetText(SkillData.DisplayName);
    if (DescriptionText) DescriptionText->SetText(SkillData.Description);
    if (ManaCostText) ManaCostText->SetText(FText::AsNumber(SkillData.ManaCost));
    if (CooldownText) CooldownText->SetText(FText::AsNumber(SkillData.DisplayCooldown));
    if (IconImage && SkillData.Icon) IconImage->SetBrushFromTexture(SkillData.Icon);

    // 2. 이미 선택된 스킬 예외 처리 (클릭 방지 및 반투명 처리)
    if (CardButton)
    {
        if (bIsAlreadySelected)
        {
            CardButton->SetIsEnabled(false);
            SetRenderOpacity(0.5f);
        }
        else
        {
            CardButton->SetIsEnabled(true);
            SetRenderOpacity(1.0f);
        }
    }
}

void UHB_SkillCardWidget::OnButtonClicked()
{
    if (OnCardClickedDelegate.IsBound())
    {
        OnCardClickedDelegate.Broadcast(CurrentSkillId);
    }
}