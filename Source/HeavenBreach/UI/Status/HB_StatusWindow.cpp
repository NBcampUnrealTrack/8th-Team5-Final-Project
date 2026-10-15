// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Status/HB_StatusWindow.h"
#include "UI/Status/HB_StatusAugmentEntry.h"
#include "Player/HBPlayerController.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/Job/HBJobData.h"
#include "Combat/Ability/HBSkillAugment.h"
#include "UI/HB_UILayout.h"
#include "Core/Tag/UI/HB_UITag.h"
#include "UI/Skill/HB_SkillIconWidget.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace
{
	// 소수점 한 자리까지만. 100.0 이 아니라 100 으로, 12.5 는 12.5 로 보인다.
	FText FormatStat(float Value)
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 1;
		return FText::AsNumber(Value, &Options);
	}
}

UHB_StatusWindow::UHB_StatusWindow()
{
	InputMode = EHBWidgetInputMode::Menu;

	bIsBackHandler = true;
}

void UHB_StatusWindow::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);
}

void UHB_StatusWindow::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 스택이 위젯을 풀에서 재사용하므로 열릴 때마다 처음부터 다시 그린다.
	LastAugmentSignature = -1;
	RefreshElapsed = 0.f;
	RefreshAll();

	// Menu 입력 모드는 포커스가 있어야 K 를 받는다. 포커스 대상 칸이 따로 없으니 창 자신이 가진다.
	SetUserFocus(GetOwningPlayer());
}

void UHB_StatusWindow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshElapsed += InDeltaTime;
	if (RefreshElapsed < RefreshInterval)
	{
		return;
	}
	RefreshElapsed = 0.f;

	RefreshStats();

	// 증강/패시브는 값이 바뀌었을 때만 다시 그린다 (RefreshSkills/Augments 안에서 판정).
	RefreshSkills();
	RefreshAugments();
}

FReply UHB_StatusWindow::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::K)
	{
		if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
		{
			PC->ToggleSkillAugment(); // 이미 열려 있으므로 "닫기" 분기로 간다
		}
		return FReply::Handled();
	}

	// ESC 는 CommonUI 의 뒤로 가기가 먼저 처리하지만, 포커스 상태에 따라 놓칠 수 있어서 여기서도 닫는다.
	// 이 창이 열려 있을 때 ToggleSkillAugment 는 "닫기" 분기로 간다 (K 와 같다).
	// 뒤로 가기가 이미 닫았다면 이 함수가 불리지 않는다. 불렸다면 아직 열려 있는 것이다.
	if (InKeyEvent.GetKey() == EKeys::Escape && IsActivated())
	{
		if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
		{
			PC->ToggleSkillAugment();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

AHBPlayerCharacter* UHB_StatusWindow::GetHBCharacter() const
{
	return Cast<AHBPlayerCharacter>(GetOwningPlayerPawn());
}

void UHB_StatusWindow::RefreshAll()
{
	RefreshStats();
	RefreshSkills();
	RefreshAugments();
}

void UHB_StatusWindow::RefreshStats()
{
	const APlayerController* PC = GetOwningPlayer();
	const AHB_PlayerState* PS = PC ? PC->GetPlayerState<AHB_PlayerState>() : nullptr;
	const UHB_PlayerAttributeSet* Attr = PS ? PS->GetPlayerAttributeSet() : nullptr;
	if (!Attr)
	{
		return;
	}

	if (Text_HP)
	{
		Text_HP->SetText(FText::Format(
			NSLOCTEXT("StatusWindow", "Gauge", "{0} / {1}"),
			FormatStat(Attr->GetPlayer_HP()), FormatStat(Attr->GetPlayer_MaxHP())));
	}

	if (Text_Stamina)
	{
		Text_Stamina->SetText(FText::Format(
			NSLOCTEXT("StatusWindow", "Gauge", "{0} / {1}"),
			FormatStat(Attr->GetPlayer_Mana()), FormatStat(Attr->GetPlayer_MaxMana())));
	}

	if (Text_AttackPower)
	{
		Text_AttackPower->SetText(FormatStat(Attr->GetPlayer_AttackPower()));
	}

	if (Text_Defense)
	{
		Text_Defense->SetText(FormatStat(Attr->GetPlayer_Defense()));
	}
}

void UHB_StatusWindow::RefreshSkills()
{
	const AHBPlayerCharacter* Character = GetHBCharacter();
	const UHBJobData* Job = Character ? Character->GetJobData() : nullptr;
	if (!Character || !Job)
	{
		return;
	}

	// Q/E 는 한 번 정해지면 바뀌지 않지만 값이 싸서 매번 같이 읽는다.
	// 액티브인지 패시브인지는 몰라도 된다. DA 가 양쪽을 다 뒤져 준다.
	if (SkillIcon_Q)
	{
		const FHBJobSkillData* Skill = Job->FindSkillAnywhere(Character->GetSelectedSkillQId());
		SkillIcon_Q->SetSkillIcon(Skill ? Skill->Icon.LoadSynchronous() : nullptr);
	}
	if (SkillIcon_E)
	{
		const FHBJobSkillData* Skill = Job->FindSkillAnywhere(Character->GetSelectedSkillEId());
		SkillIcon_E->SetSkillIcon(Skill ? Skill->Icon.LoadSynchronous() : nullptr);
	}

	// 패시브 칸은 로비에서 고른 시작 패시브 하나만 보여준다.
	// 레벨업으로 얻은 패시브는 여기가 아니라 증강 목록(RefreshAugments)에 들어간다.
	if (SkillIcon_Passive)
	{
		const AHB_PlayerState* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AHB_PlayerState>() : nullptr;
		const FName StartPassiveId = PS ? PS->GetStartingLoadout().PassiveSkillId : NAME_None;
		const FHBJobSkillData* Skill = Job->FindSkillAnywhere(StartPassiveId);
		SkillIcon_Passive->SetSkillIcon(Skill ? Skill->Icon.LoadSynchronous() : nullptr);
	}
}

void UHB_StatusWindow::RefreshAugments()
{
	const AHBPlayerCharacter* Character = GetHBCharacter();
	if (!Character || !AugmentContainer || !AugmentEntryClass)
	{
		return;
	}

	const AHB_PlayerState* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AHB_PlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}

	// 노드는 1 레벨부터 최대 레벨까지 항상 깔아 둔다.
	//   Lv 1            선택이 없는 시작 노드
	//   Lv 2 ~          N 번째 선택이 N + 1 레벨이다. 한 번에 여러 레벨이 올라도 선택은 하나씩 소모된다.
	//   기록이 없는 레벨  아직 도달하지 못한 잠긴 노드
	// 선택하지 않은 채로는 이 창이 열리지 않으므로 "레벨은 올랐는데 못 고른" 상태는 없다.
	// 기록은 늘기만 하고 매치가 끝나야 비워지므로 개수가 같으면 같은 목록이다.
	const TArray<FHBUpgradePick>& History = PS->GetUpgradeHistory();

	if (History.Num() == LastAugmentSignature)
	{
		return;
	}
	LastAugmentSignature = History.Num();

	AugmentContainer->ClearChildren();

	const UHBJobData* Job = Character->GetJobData();
	const int32 MaxLevel = FMath::Max(PS->GetMaxPlayerLevel(), 1);

	for (int32 Level = 1; Level <= MaxLevel; ++Level)
	{
		UHB_StatusAugmentEntry* Entry = CreateWidget<UHB_StatusAugmentEntry>(this, AugmentEntryClass);
		if (!Entry)
		{
			continue;
		}

		if (Level == 1)
		{
			Entry->InitStartNode(Level);
		}
		else if (History.IsValidIndex(Level - 2))
		{
			// 문구와 아이콘은 기록에 없다. 태그와 스킬 ID 로 데이터를 다시 찾는다.
			// 데이터를 못 찾아도 노드는 남긴다. 빼면 Lv 번호가 건너뛰어 보인다.
			const FHBUpgradePick& Pick = History[Level - 2];
			if (Pick.Type == EHBUpgradeCandidateType::Augment)
			{
				FHBSkillAugment Augment;
				if (Character->FindAugmentInfo(Pick.AugmentTag, Augment))
				{
					Entry->InitAugmentNode(Level, Augment, Pick.Tier);
				}
			}
			else if (Job)
			{
				if (const FHBJobSkillData* Passive = Job->FindSkillAnywhere(Pick.TargetSkillId))
				{
					Entry->InitPassiveNode(Level, *Passive);
				}
			}
		}
		else
		{
			Entry->InitLockedNode(Level);
		}

		Entry->SetLineEnds(Level == 1, Level == MaxLevel);

		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(AugmentContainer->AddChild(Entry)))
		{
			VSlot->SetPadding(FMargin(0.f, AugmentEntrySpacing * 0.5f));
		}
	}
}

UHB_StatusWindow* UHB_StatusWindow::OpenOn(
	AHBPlayerController* OwningPC,
	TSubclassOf<UHB_StatusWindow> ScreenClass)
{
	if (OwningPC == nullptr || ScreenClass == nullptr || OwningPC->GetUILayout() == nullptr)
	{
		return nullptr;
	}

	return OwningPC->GetUILayout()->PushWidgetToLayer<UHB_StatusWindow>(
		UITag::TAG_UI_Layer_Menu, ScreenClass);
}
