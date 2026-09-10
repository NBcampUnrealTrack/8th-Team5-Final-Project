// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Lobby/HB_CharacterSelectionWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"
#include "UI/Lobby/HBLobbyHUDWidget.h"

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
		// 마법사는 아직 플레이 가능한 상태가 아니라 선택 비활성화
		MageSelectButton->SetIsEnabled(false);
	}

	if (ArcherSelectButton)
	{
		ArcherButtonStyle = ArcherSelectButton->GetStyle();
		ArcherSelectButton->OnClicked.AddDynamic(this, &UHB_CharacterSelectionWidget::OnArcherClicked);
		// 궁수는 아직 플레이 가능한 상태가 아니라 선택 비활성화
		ArcherSelectButton->SetIsEnabled(false);
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

		// 현재(이전에 확정된) 직업으로 화면을 초기화 - 기본값은 전사
		HandleSelectedJobChanged(LobbyPlayerState->GetSelectedJobId());
	}
	else if (CurrentJobText)
	{
		CurrentJobText->SetText(GetJobDisplayName(NAME_None));
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
	// This widget is placed inside the lobby HUD's widget switcher, so its outer
	// chain leads back to the owning UHBLobbyHUDWidget. Switch menus directly in
	// C++ so Back works with no Blueprint wiring required.
	if (UHBLobbyHUDWidget* LobbyHUD = GetTypedOuter<UHBLobbyHUDWidget>())
	{
		LobbyHUD->SwitchToMenu(EHBLobbyHUDMenu::Lobby);
	}

	// Still fired for optional Blueprint-side polish (e.g. a transition animation).
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

	if (CurrentJobText)
	{
		// 서버가 실제로 확정한 직업만 반영한다 (버튼 클릭만으로는 안 바뀜, Decision을 눌러야 여기로 옴).
		CurrentJobText->SetText(GetJobDisplayName(NewSelectedJobId));
	}

	OnSelectionConfirmed(NewSelectedJobId);
}

FText UHB_CharacterSelectionWidget::GetJobDisplayName(FName JobId) const
{
	if (JobId == HBCharacterSelectionJob::Warrior)
	{
		return NSLOCTEXT("HBCharacterSelection", "Job_Warrior", "현재 직업: 전사");
	}

	if (JobId == HBCharacterSelectionJob::Mage)
	{
		return NSLOCTEXT("HBCharacterSelection", "Job_Mage", "현재 직업: 마법사");
	}

	if (JobId == HBCharacterSelectionJob::Archer)
	{
		return NSLOCTEXT("HBCharacterSelection", "Job_Archer", "현재 직업: 궁수");
	}

	return NSLOCTEXT("HBCharacterSelection", "Job_None", "직업을 선택하세요");
}
