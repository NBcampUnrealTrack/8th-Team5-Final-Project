// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillIconCardWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Texture2D.h"
#include "UI/Skill/HB_SkillDragDropOperation.h"

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

	// 처음에는 아무것도 집지 않은 상태다.
	if (HeldBorder)
	{
		HeldBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UHB_SkillIconCardWidget::InitIconCard(const FSkillCardData& SkillData)
{
	CurrentSkillId = SkillData.SkillId;
	CurrentSkillType = SkillData.SkillType;
	CurrentIcon = SkillData.Icon;

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

	if (CardButton)
	{
		// 아이콘만 있는 카드라 이름은 툴팁으로라도 노출한다.
		CardButton->SetToolTipText(SkillData.DisplayName);
	}
}

void UHB_SkillIconCardWidget::SetDetailTooltip(UWidget* InTooltip)
{
	// 아이콘만 있는 카드의 임시 이름 툴팁(InitIconCard)을 걷어 낸다. 그대로 두면 가장 안쪽인 버튼의
	// 툴팁이 이겨서 상세 툴팁이 뜨지 않는다.
	if (CardButton)
	{
		CardButton->SetToolTip(nullptr);
		CardButton->SetToolTipText(FText::GetEmpty());
	}

	SetToolTip(InTooltip);
}

void UHB_SkillIconCardWidget::SetPreviewSelected(bool bSelected)
{
	if (SelectedBorder)
	{
		SelectedBorder->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

FReply UHB_SkillIconCardWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 스킬 정보가 없는 카드는 끌 것이 없다. 아무것도 가로채지 않는다.
	if (CurrentSkillId.IsNone() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}

	// 끌지 않고 떼면 클릭이다. 끌기가 시작되면 NativeOnDragDetected 가 이 표시를 지운다.
	bClickPending = true;

	return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
}

void UHB_SkillIconCardWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bClickPending = false;

	OutOperation = UHB_SkillDragDropOperation::Create(
		GetOwningPlayer(),
		CurrentSkillId,
		CurrentSkillType,
		ESkillSlotType::None,
		CurrentIcon.Get(),
		DragVisualSize);

	OnIconDragStartedDelegate.Broadcast(CurrentSkillId, OutOperation);
}

FReply UHB_SkillIconCardWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bClickPending)
	{
		BroadcastClickOnce();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UHB_SkillIconCardWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	// 누른 채로 카드 밖으로 나갔다가 뗀 것은 클릭이 아니다.
	bClickPending = false;

	Super::NativeOnMouseLeave(InMouseEvent);
}

void UHB_SkillIconCardWidget::SetHeld(bool bHeld)
{
	if (HeldBorder)
	{
		HeldBorder->SetVisibility(bHeld ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	OnHeldChanged(bHeld);
}

void UHB_SkillIconCardWidget::HandleCardClicked()
{
	BroadcastClickOnce();
}

void UHB_SkillIconCardWidget::BroadcastClickOnce()
{
	if (!bClickPending)
	{
		return;
	}

	bClickPending = false;
	OnIconClickedDelegate.Broadcast(CurrentSkillId);
}
