// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Skill/HB_SkillSlotWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "UI/Skill/HB_SkillIconWidget.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/Skill/HB_SkillDragDropOperation.h"


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

void UHB_SkillSlotWidget::SetDropHintActive(bool bActive)
{
    if (bIsDropHintActive == bActive)
    {
        return;
    }

    bIsDropHintActive = bActive;
    OnDropHintChanged(bIsDropHintActive);
}

FReply UHB_SkillSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 빈 슬롯은 끌 것이 없다. 아무것도 가로채지 않는다.
    if (EquippedSkillId.IsNone() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
    }

    return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
}

void UHB_SkillSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    const ESkillType Type = (SlotType == ESkillSlotType::Passive) ? ESkillType::Passive : ESkillType::Active;

    OutOperation = UHB_SkillDragDropOperation::Create(
        GetOwningPlayer(),
        EquippedSkillId,
        Type,
        SlotType,
        CurrentIcon.Get(),
        DragVisualSize);
}

void UHB_SkillSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

    const UHB_SkillDragDropOperation* SkillOp = Cast<UHB_SkillDragDropOperation>(InOperation);

    // 자기 자리로 되돌려 놓는 것은 신호를 주지 않는다. 받을 수 있는 스킬일 때만 켠다.
    if (SkillOp && SkillOp->SourceSlot != SlotType
        && UHB_SkillDragDropOperation::CanSlotAccept(SlotType, SkillOp->SkillType))
    {
        SetSlotSelected(true);
    }
}

void UHB_SkillSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);

    SetSlotSelected(false);
}

bool UHB_SkillSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    // 스킬 끌기가 아니면 이 슬롯이 받을 게 없다.
    return Cast<UHB_SkillDragDropOperation>(InOperation) != nullptr;
}

bool UHB_SkillSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    SetSlotSelected(false);

    const UHB_SkillDragDropOperation* SkillOp = Cast<UHB_SkillDragDropOperation>(InOperation);

    if (!SkillOp)
    {
        return false;
    }

    // 종류가 안 맞거나 제자리로 놓은 경우에도 "처리했다"고 답한다.
    // 안 그러면 위로 전달돼 슬롯 밖에 놓은 것으로 취급되어 장착이 풀린다.
    if (SkillOp->SourceSlot != SlotType
        && UHB_SkillDragDropOperation::CanSlotAccept(SlotType, SkillOp->SkillType))
    {
        OnSkillDropped.Broadcast(SlotType, SkillOp->SkillId, SkillOp->SourceSlot);
    }

    return true;
}

void UHB_SkillSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

    SetSlotSelected(false);
}

void UHB_SkillSlotWidget::SetSkillIcon(UTexture2D* NewIcon)
{
    CurrentIcon = NewIcon;

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