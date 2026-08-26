// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Lobby/HB_CharacterSelectionWidget.h"
#include "Components/Button.h"

void UHB_CharacterSelectionWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 델리게이트에 함수 바인딩
    if (WarriorSelectButton)
    {
        WarriorSelectButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnWarriorClicked);
    }

    if (MageSelectButton)
    {
        MageSelectButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnMageClicked);
    }

    if (ArcherSelectButton)
    {
        ArcherSelectButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnArcherClicked);
    }

    if (DecisionButton)
    {
        DecisionButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnDecisionClicked);
    }

    if (BackButton)
    {
        BackButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnBackClicked);
    }
}

void UHB_CharacterSelectionWidget::OnWarriorClicked()
{
}

void UHB_CharacterSelectionWidget::OnMageClicked()
{
}

void UHB_CharacterSelectionWidget::OnArcherClicked()
{
}

void UHB_CharacterSelectionWidget::OnDecisionClicked()
{
}

void UHB_CharacterSelectionWidget::OnBackClicked()
{
}
