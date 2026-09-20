// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillCardWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"

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
    ApplyDescription(SkillData);

    // "없음" 카드(SkillId 없음)는 0/0 이 아니라 "값 자체가 없다"는 뜻이라 "–" 로 보여준다.
    // 0 은 "마나를 0 쓰는 스킬"로 오해될 수 있다. 단위(MP/초)도 여기서 붙인다.
    if (ManaCostText)
    {
        ManaCostText->SetText(SkillData.SkillId.IsNone()
            ? FText::FromString(TEXT("–"))
            : FText::FromString(FString::Printf(TEXT("%d MP"), SkillData.ManaCost)));
    }
    if (CooldownText)
    {
        CooldownText->SetText(SkillData.SkillId.IsNone()
            ? FText::FromString(TEXT("–"))
            : FText::Format(NSLOCTEXT("SkillCard", "CooldownSeconds", "{0}초"), FText::AsNumber(SkillData.DisplayCooldown)));
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

void UHB_SkillCardWidget::ApplyDescription(const FSkillCardData& SkillData)
{
    // 리치 텍스트 칸이 WBP 에 있으면 색이 들어간 쪽을 쓴다.
    // 없으면 평문으로 떨어진다. WBP 를 안 바꿔도 아무것도 깨지지 않는다.
    if (DescriptionRichText)
    {
        DescriptionRichText->SetText(SkillData.DescriptionRich);
        DescriptionRichText->SetVisibility(ESlateVisibility::Visible);

        // 둘이 같이 보이면 설명이 두 번 나온다.
        if (DescriptionText)
        {
            DescriptionText->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    if (DescriptionText)
    {
        DescriptionText->SetText(SkillData.Description);
    }
}

void UHB_SkillCardWidget::OnButtonClicked()
{
    if (OnCardClickedDelegate.IsBound())
    {
        OnCardClickedDelegate.Broadcast(CurrentSkillId);
    }
}