// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillMainWidget.h"
#include "UI/Skill/HB_SkillSlotWidget.h"
#include "UI/Skill/HB_SkillCardWidget.h"
#include "UI/Skill/HB_SkillData.h"
#include "Components/ScrollBox.h"

UHB_SkillMainWidget::UHB_SkillMainWidget()
{
    // 1. CommonUI 입력 모드 선언 (메뉴 모드: 게임 입력 차단 + 커서 활성화)
    InputMode = EHBWidgetInputMode::Menu;

    // 2. ESC 키 또는 패드 B 버튼 입력 시 자기를 자동으로 Deactivate 처리
    bIsBackHandler = true;
}

void UHB_SkillMainWidget::NativeOnActivated()
{
    Super::NativeOnActivated();

    // 스택에 푸시되어 UI가 실제 활성화되는 시점에 바인딩 수행
    if (Slot_Q) Slot_Q->OnSlotClicked.AddDynamic(this, &UHB_SkillMainWidget::OnSkillSlotClicked);
    if (Slot_E) Slot_E->OnSlotClicked.AddDynamic(this, &UHB_SkillMainWidget::OnSkillSlotClicked);
    if (Slot_Passive) Slot_Passive->OnSlotClicked.AddDynamic(this, &UHB_SkillMainWidget::OnSkillSlotClicked);
}

UWidget* UHB_SkillMainWidget::NativeGetDesiredFocusTarget() const
{
    // UI가 열렸을 때 첫 번째로 포커스를 받을 슬롯 지정 (패드/키보드 조작 대응)
    if (Slot_Q)
    {
        return Slot_Q;
    }
    return Super::NativeGetDesiredFocusTarget();
}

void UHB_SkillMainWidget::OnSkillSlotClicked(ESkillSlotType ClickedSlotType)
{
    CurrentEditingSlotType = ClickedSlotType;

    bool bIsPassive = (ClickedSlotType == ESkillSlotType::Passive);
    PopulateSkillList(bIsPassive);
}

void UHB_SkillMainWidget::PopulateSkillList(bool bIsPassive)
{
    if (!SkillListScrollBox || !SkillCardClass || !SkillCatalog) return;

    // 1. 기존 목록 비우기
    SkillListScrollBox->ClearChildren();

    // 2. 통합 데이터 테이블의 모든 행 가져오기
    TArray<FSkillCardData*> AllSkills;
    SkillCatalog->GetAllRows<FSkillCardData>(TEXT("SkillCatalogContext"), AllSkills);

    // 3. 현재 클릭한 슬롯에 맞는 스킬 타입 결정
    ESkillType TargetType = bIsPassive ? ESkillType::Passive : ESkillType::Active;

    // 4. 필터링 후 UI 카드 동적 생성
    for (FSkillCardData* SkillData : AllSkills)
    {
        if (SkillData && SkillData->SkillType == TargetType)
        {
            UHB_SkillCardWidget* NewCard = CreateWidget<UHB_SkillCardWidget>(GetWorld(), SkillCardClass);
            if (NewCard)
            {
                bool bIsAlreadySelected = IsSkillAlreadySelected(SkillData->SkillId);

                NewCard->InitCard(*SkillData, bIsAlreadySelected);
                NewCard->OnCardClickedDelegate.AddDynamic(this, &UHB_SkillMainWidget::OnSkillCardClicked);

                SkillListScrollBox->AddChild(NewCard);
            }
        }
    }
}

bool UHB_SkillMainWidget::IsSkillAlreadySelected(FName SkillId)
{
    return (SkillId == SelectedSkillQId || SkillId == SelectedSkillEId || SkillId == SelectedPassiveSkillId);
}

void UHB_SkillMainWidget::OnSkillCardClicked(FName ClickedSkillId)
{
    if (!SkillCatalog) return;

    FSkillCardData* SkillData = SkillCatalog->FindRow<FSkillCardData>(ClickedSkillId, TEXT(""));
    if (!SkillData)
    {
        return;
    }


    switch (CurrentEditingSlotType)
    {
    case ESkillSlotType::Active_Q:
        SelectedSkillQId = ClickedSkillId;
        if (Slot_Q) Slot_Q->SetSkillIcon(SkillData->Icon);
        break;
    case ESkillSlotType::Active_E:
        SelectedSkillEId = ClickedSkillId;
        if (Slot_E) Slot_E->SetSkillIcon(SkillData->Icon);
        break;
    case ESkillSlotType::Passive:
        SelectedPassiveSkillId = ClickedSkillId;
        if (Slot_Passive) Slot_Passive->SetSkillIcon(SkillData->Icon);
        break;
    default:
        break;
    }

    if (SkillListScrollBox)
    {
        SkillListScrollBox->ClearChildren();
    }
}

void UHB_SkillMainWidget::RequestApplyLoadout()
{
    TArray<FName> PassiveSkillIds;
    if (!SelectedPassiveSkillId.IsNone())
    {
        PassiveSkillIds.Add(SelectedPassiveSkillId);
    }

    // TODO: 생성된 캐릭터 또는 플레이어 스테이트/컨트롤러로 데이터 전달
}