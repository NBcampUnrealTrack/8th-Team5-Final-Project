// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Skill/HB_SkillAugment.h"
#include "Player/HBPlayerController.h"
#include "UI/HB_UILayout.h"
#include "Core/Tag/UI/HB_UITag.h"
#include "Math/UnrealMathUtility.h"
#include "Components/HorizontalBox.h" 
#include "HB_AugmentData.h"
#include "UI/Skill/HB_AugmentCard.h"


UHB_SkillAugment::UHB_SkillAugment()
{
    InputMode = EHBWidgetInputMode::Menu;

    bIsBackHandler = true;
}

void UHB_SkillAugment::NativeOnActivated()
{
}

UWidget* UHB_SkillAugment::NativeGetDesiredFocusTarget() const
{
    return nullptr;
}

void UHB_SkillAugment::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
}

FReply UHB_SkillAugment::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::K)
    {
        if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
        {
            PC->ToggleSkillAugment(); // 이미 열려있는 상태이므로 ToggleSkillAugment의 "닫기" 분기가 실행됨
        }
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHB_SkillAugment::GenerateRandomAugments(FName EquippedSlotQ, FName EquippedSlotE, TArray<FName> EquippedPassives)
{
    if (!AugmentDataTable || !CardContainer || !AugmentCardClass) return;

    CardContainer->ClearChildren();

    TArray<FAugmentCardData*> AllAugments;
    AugmentDataTable->GetAllRows<FAugmentCardData>(TEXT("AugmentContext"), AllAugments);

    TArray<FAugmentCardData*> ValidAugmentPool;

    // 1. 플레이어 상태에 맞는 유효한 증강만 풀(Pool)에 추가
    for (FAugmentCardData* Augment : AllAugments)
    {
        if (!Augment) continue;

        if (Augment->AugmentType == EAugmentType::ActiveUpgrade)
        {
            // 타겟 스킬이 현재 장착한 Q 또는 E 스킬과 일치할 때만 등장
            if (Augment->TargetSkillId == EquippedSlotQ || Augment->TargetSkillId == EquippedSlotE)
            {
                ValidAugmentPool.Add(Augment);
            }
        }
        else if (Augment->AugmentType == EAugmentType::NewPassive)
        {
            // 이미 보유한 패시브가 아닐 때만 등장
            if (!EquippedPassives.Contains(Augment->TargetSkillId))
            {
                ValidAugmentPool.Add(Augment);
            }
        }
    }

    // 2. 유효한 증강 풀이 3개 미만일 경우의 예외 처리
    int32 MaxCards = FMath::Min(3, ValidAugmentPool.Num());
    if (MaxCards == 0) return;

    // 3. 피셔-예이츠 셔플(Fisher-Yates Shuffle)로 배열 무작위 섞기
    for (int32 i = ValidAugmentPool.Num() - 1; i > 0; i--)
    {
        int32 j = FMath::RandRange(0, i);
        ValidAugmentPool.Swap(i, j);
    }

    // 4. 섞인 풀에서 앞에서부터 3개(MaxCards)를 뽑아 UI 생성
    for (int32 i = 0; i < MaxCards; i++)
    {
        FAugmentCardData* SelectedData = ValidAugmentPool[i];

        UHB_AugmentCard* NewCard = CreateWidget<UHB_AugmentCard>(GetWorld(), AugmentCardClass);
        if (NewCard)
        {
            // 카드 초기화 (CommonUI 버튼 클릭 이벤트 바인딩 포함)
            NewCard->InitCard(*SelectedData);
            CardContainer->AddChild(NewCard);
        }
    }
}

UHB_SkillAugment* UHB_SkillAugment::OpenOn(
    AHBPlayerController* OwningPC,
    TSubclassOf<UHB_SkillAugment> ScreenClass)
{
    UE_LOG(LogTemp, Warning, TEXT("OwningPC=%s ScreenClass=%s UILayout=%s"),
        OwningPC ? TEXT("valid") : TEXT("null"),
        *GetNameSafe(ScreenClass),
        (OwningPC && OwningPC->GetUILayout()) ? TEXT("valid") : TEXT("null"));
    if (OwningPC == nullptr || ScreenClass == nullptr || OwningPC->GetUILayout() == nullptr)
    {
        return nullptr;
    }

    return OwningPC->GetUILayout()->PushWidgetToLayer<UHB_SkillAugment>(
        UITag::TAG_UI_Layer_Menu, ScreenClass,
        [](UHB_SkillAugment& Screen)
        {
            // TODO: 실제 장착된 Q/E 스킬, 패시브 목록으로 교체
            Screen.GenerateRandomAugments(TEXT("SkillQ"), TEXT("SkillE"), TArray<FName>{});
        });
}
