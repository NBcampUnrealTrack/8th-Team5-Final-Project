// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Lobby/HB_CharacterSelectionWidget.h"
#include "Components/Button.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"

namespace HBCharacterSelectionJob
{
	static const FName Warrior(TEXT("Warrior"));
	static const FName Mage(TEXT("Mage"));
	static const FName Archer(TEXT("Archer"));
}

void UHB_CharacterSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 버튼-함수 바인딩
	if (WarriorSelectButton)
	{
		WarriorButtonStyle = WarriorSelectButton->GetStyle();
		WarriorSelectButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnWarriorClicked);
	}

	if (MageSelectButton)
	{
		MageButtonStyle = MageSelectButton->GetStyle();
		MageSelectButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnMageClicked);
	}

	if (ArcherSelectButton)
	{
		ArcherButtonStyle = ArcherSelectButton->GetStyle();
		ArcherSelectButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnArcherClicked);
	}

	if (DecisionButton)
	{
		DecisionButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnDecisionClicked);
		// 아직 아무것도 선택하지 않았으므로 확정 버튼은 비활성화
		DecisionButton->SetIsEnabled(false);
	}

	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnBackClicked);
	}

	LobbyPlayerState = GetOwningPlayerState<AHBLobbyPlayerState>();
	if (IsValid(LobbyPlayerState))
	{
		LobbyPlayerState->OnSelectedJobChanged.AddUniqueDynamic(
			this, &UHB_CharacterSelectionWidget::HandleSelectedJobChanged);

		// 이전에 이미 선택/확정한 직업이 있다면 화면을 그 상태로 복원
		if (LobbyPlayerState->HasSelectedJob())
		{
			HandleSelectedJobChanged(LobbyPlayerState->GetSelectedJobId());
		}
	}
}

void UHB_CharacterSelectionWidget::NativeDestruct()
{
	if (IsValid(LobbyPlayerState))
	{
		LobbyPlayerState->OnSelectedJobChanged.RemoveDynamic(
			this, &UHB_CharacterSelectionWidget::HandleSelectedJobChanged);
	}

	Super::NativeDestruct();
}

void UHB_CharacterSelectionWidget::OnWarriorClicked()
{
	SelectPendingJob(HBCharacterSelectionJob::Warrior);
}

void UHB_CharacterSelectionWidget::OnMageClicked()
{
	SelectPendingJob(HBCharacterSelectionJob::Mage);
}

void UHB_CharacterSelectionWidget::OnArcherClicked()
{
	SelectPendingJob(HBCharacterSelectionJob::Archer);
}

void UHB_CharacterSelectionWidget::OnDecisionClicked()
{
	if (PendingJobId.IsNone())
	{
		return;
	}

	AHBLobbyPlayerController* LobbyPC = GetOwningPlayer<AHBLobbyPlayerController>();
	if (!IsValid(LobbyPC))
	{
		return;
	}

	// 서버가 확정하면 HandleSelectedJobChanged를 통해 결과가 다시 반영된다.
	LobbyPC->SelectJob(PendingJobId);
}

void UHB_CharacterSelectionWidget::OnBackClicked()
{
	OnBackRequested();
}

void UHB_CharacterSelectionWidget::SelectPendingJob(FName JobId)
{
	if (PendingJobId == JobId)
	{
		return;
	}

	PendingJobId = JobId;
	UpdateButtonStyles();

	if (DecisionButton)
	{
		DecisionButton->SetIsEnabled(!PendingJobId.IsNone());
	}
}

void UHB_CharacterSelectionWidget::UpdateButtonStyles()
{
	if (WarriorSelectButton)
	{
		WarriorSelectButton->SetStyle(WarriorButtonStyle);
	}

	if (MageSelectButton)
	{
		MageSelectButton->SetStyle(MageButtonStyle);
	}

	if (ArcherSelectButton)
	{
		ArcherSelectButton->SetStyle(ArcherButtonStyle);
	}

	if (UButton* SelectedButton = GetButtonForJob(PendingJobId))
	{
		const FButtonStyle* DefaultStyle =
			SelectedButton == WarriorSelectButton ? &WarriorButtonStyle :
			SelectedButton == MageSelectButton ? &MageButtonStyle :
			SelectedButton == ArcherSelectButton ? &ArcherButtonStyle : nullptr;

		if (DefaultStyle)
		{
			ApplySelectedStyle(SelectedButton, *DefaultStyle);
		}
	}
}

void UHB_CharacterSelectionWidget::ApplySelectedStyle(UButton* Button, const FButtonStyle& DefaultStyle) const
{
	if (!Button)
	{
		return;
	}

	FButtonStyle SelectedStyle = DefaultStyle;
	SelectedStyle.SetNormal(DefaultStyle.Hovered);
	Button->SetStyle(SelectedStyle);
}

UButton* UHB_CharacterSelectionWidget::GetButtonForJob(FName JobId) const
{
	if (JobId == HBCharacterSelectionJob::Warrior)
	{
		return WarriorSelectButton;
	}

	if (JobId == HBCharacterSelectionJob::Mage)
	{
		return MageSelectButton;
	}

	if (JobId == HBCharacterSelectionJob::Archer)
	{
		return ArcherSelectButton;
	}

	return nullptr;
}

void UHB_CharacterSelectionWidget::HandleSelectedJobChanged(FName NewSelectedJobId)
{
	PendingJobId = NewSelectedJobId;
	UpdateButtonStyles();

	if (DecisionButton)
	{
		DecisionButton->SetIsEnabled(!PendingJobId.IsNone());
	}

	OnSelectionConfirmed(NewSelectedJobId);
}
