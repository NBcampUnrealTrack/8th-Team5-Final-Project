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

void UHB_SkillCardWidget::InitCard(const FSkillCardData& SkillData)
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
    // 이 카드는 이제 "장착 중" / "선택한 스킬" 정보 패널 전용이라 (그리드 목록 행이
    // 아님) 항상 클릭 가능하고 항상 완전 불투명하다. "이미 장착됨" 흐림 처리는
    // 그리드 쪽 아이콘 카드(HB_SkillIconCardWidget)로 옮겼다.
    if (CardButton)
    {
        CardButton->SetIsEnabled(true);
    }

    SetRenderOpacity(1.0f);

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

    // 값과 같이 있는 고정 글자("마나 소모", "쿨타임")도 같이 숨긴다.
    if (ManaCostRow)
    {
        ManaCostRow->SetVisibility(CostVisibility);
    }
    if (CooldownRow)
    {
        CooldownRow->SetVisibility(CostVisibility);
    }
}

void UHB_SkillCardWidget::SetHeaderText(const FText& InHeaderText)
{
    if (HeaderText)
    {
        HeaderText->SetText(InHeaderText);
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