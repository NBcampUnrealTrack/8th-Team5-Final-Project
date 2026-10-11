// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Skill/HB_SkillSlotWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "UI/Skill/HB_SkillIconWidget.h"
#include "Engine/Texture2D.h"


void UHB_SkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &UHB_SkillSlotWidget::HandleSlotButtonClicked);
    }

    // WBP 디자이너가 세팅해 둔 기본(빈 슬롯) 브러시를 여기서 한 번 캐싱해 둔다.
    // SetSkillIcon(nullptr) 때 이걸로 복원해야 "기본 이미지"가 계속 보인다.
    if (SkillIconImage)
    {
        DefaultIconBrush = SkillIconImage->GetBrush();
    }
}

void UHB_SkillSlotWidget::HandleSlotButtonClicked()
{
	OnSlotClicked.Broadcast(SlotType);
}

void UHB_SkillSlotWidget::SetSlotSelected(bool bSelected)
{
    if (bIsSlotSelected == bSelected)
    {
        return;
    }

    bIsSlotSelected = bSelected;
    OnSlotSelectedChanged(bIsSlotSelected);
}

void UHB_SkillSlotWidget::SetSkillIcon(UTexture2D* NewIcon)
{
    // 표시 전용 위젯이 있으면 그쪽이 그린다. 빈 슬롯 처리(기본 이미지 복원)도 그 위젯이 맡는다.
    if (SkillIconWidget)
    {
        SkillIconWidget->SetSkillIcon(NewIcon);
        return;
    }

    if (!SkillIconImage)
    {
        return;
    }

    // 널이 오면 빈 슬롯이라는 뜻이다. 예전 아이콘이 남는 것도, 디자이너가
    // 세팅해 둔 기본(빈 슬롯) 이미지가 사라지는 것도 둘 다 잘못이다.
    // 그래서 브러시를 지우는 대신 NativeConstruct 에서 캐싱해 둔 기본
    // 브러시로 되돌린다. 위젯 자체를 숨기지도 않는다 - 숨기면 기본 이미지도
    // 같이 숨는다.
    if (!NewIcon)
    {
        SkillIconImage->SetBrush(DefaultIconBrush);
        SkillIconImage->SetVisibility(ESlateVisibility::Visible);
        return;
    }

    SkillIconImage->SetBrushFromTexture(NewIcon);
    SkillIconImage->SetVisibility(ESlateVisibility::Visible);

    FLinearColor Color = SkillIconImage->GetColorAndOpacity();
    Color.A = 1.0f;
    SkillIconImage->SetColorAndOpacity(Color);
}