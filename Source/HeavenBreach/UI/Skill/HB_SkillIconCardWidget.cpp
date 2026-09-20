// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillIconCardWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"

void UHB_SkillIconCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CardButton)
	{
		CardButton->OnClicked.AddDynamic(this, &UHB_SkillIconCardWidget::HandleCardClicked);

		// WBP 디자이너에서 실수로 Collapsed 로 남겨두면 이 카드 자체가 렌더링/공간할당이
		// 전혀 안 돼서 (자식인 IconImage 를 Visible 로 만들어도 소용없다) 그리드가
		// 텅 비어 보인다. 카드의 핵심 위젯이니 여기서 강제로 보이게 한다.
		CardButton->SetVisibility(ESlateVisibility::Visible);
	}

	// WBP 디자이너가 IconImage 에 세팅해 둔 기본(빈 슬롯) 브러시를 캐싱해 둔다.
	if (IconImage)
	{
		DefaultIconBrush = IconImage->GetBrush();
	}

	SetPreviewSelected(false);
}

void UHB_SkillIconCardWidget::InitIconCard(const FSkillCardData& SkillData, bool bIsEquipped)
{
	CurrentSkillId = SkillData.SkillId;

	if (IconImage)
	{
		if (SkillData.Icon)
		{
			IconImage->SetBrushFromTexture(SkillData.Icon);
		}
		else
		{
			// "없음" 카드나 아이콘 미설정 스킬. 브러시를 지우지 않고 기본값으로 되돌린다.
			IconImage->SetBrush(DefaultIconBrush);
		}

		IconImage->SetVisibility(ESlateVisibility::Visible);
	}

	if (EquippedBadge)
	{
		EquippedBadge->SetVisibility(bIsEquipped ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CardButton)
	{
		// 아이콘만 있는 카드라 이름은 툴팁으로라도 노출한다.
		CardButton->SetToolTipText(SkillData.DisplayName);
	}
}

void UHB_SkillIconCardWidget::SetPreviewSelected(bool bSelected)
{
	if (SelectedBorder)
	{
		SelectedBorder->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UHB_SkillIconCardWidget::HandleCardClicked()
{
	OnIconClickedDelegate.Broadcast(CurrentSkillId);
}
