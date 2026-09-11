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
    // 아이콘이 없으면 ("없음" 카드, 또는 아이콘 미설정) 빈 브러시가 흰 네모로
    // 보인다. 이미지를 숨겨서 자리만 비운다.
    if (IconImage)
    {
        IconImage->SetBrushFromTexture(SkillData.Icon);

        ESlateVisibility IconVisibility = ESlateVisibility::Visible;

        if (!SkillData.Icon)
        {
            IconVisibility = ESlateVisibility::Hidden;
        }

        IconImage->SetVisibility(IconVisibility);
    }

    // 2. 이미 선택된 스킬은 흐리게만 표시하고 클릭은 막지 않는다.
    //
    // 왜 안 막나: Q 에 있는 스킬을 E 에서 눌러 서로 맞바꾸는 조작이 있다.
    // 막으면 두 액티브를 교환할 방법이 사라진다.
    if (CardButton)
    {
        CardButton->SetIsEnabled(true);
    }

    SetRenderOpacity(1.0f);

    if (bIsAlreadySelected)
    {
        SetRenderOpacity(0.5f);
    }

    // 패시브에는 마나·쿨 개념이 없다. 0 을 보여주면 잘못된 정보다.
    ESlateVisibility CostVisibility = ESlateVisibility::Visible;

    if (SkillData.SkillType == ESkillType::Passive)
    {
        CostVisibility = ESlateVisibility::Collapsed;
    }

    if (ManaCostText)
    {
        ManaCostText->SetVisibility(CostVisibility);
    }
    if (CooldownText)
    {
        CooldownText->SetVisibility(CostVisibility);
    }
}

void UHB_SkillCardWidget::OnButtonClicked()
{
    if (OnCardClickedDelegate.IsBound())
    {
        OnCardClickedDelegate.Broadcast(CurrentSkillId);
    }
}