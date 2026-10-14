// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Skill/HB_SkillAugment.h"
#include "Player/HBPlayerController.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "UI/HB_UILayout.h"
#include "UI/Skill/HB_AugmentCard.h"
#include "Core/Tag/UI/HB_UITag.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UHB_SkillAugment::UHB_SkillAugment()
{
	InputMode = EHBWidgetInputMode::Menu;

	bIsBackHandler = true;

	// 기본 효과음. 에셋이 이미 프로젝트에 있다 (Sound/UI/Skill). WBP 에서 다른 소리로 바꿀 수 있다.
	OpenSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sound/UI/Skill/SFX_Augment_Open.SFX_Augment_Open")));
	SelectSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sound/UI/Skill/SFX_Augment_Confirm.SFX_Augment_Confirm")));
}

void UHB_SkillAugment::PlayAugmentSound(const TSoftObjectPtr<USoundBase>& Sound) const
{
	if (Sound.IsNull())
	{
		return;
	}

	// 소리 몇 개뿐이라 동기 로드로 충분하다. 첫 재생에만 로드하고 이후에는 캐시된다.
	if (USoundBase* Loaded = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Loaded);
	}
}

void UHB_SkillAugment::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (AHB_PlayerState* HBPS = GetHBPlayerState())
	{
		HBPS->OnUpgradeCandidatesChanged.AddDynamic(this, &UHB_SkillAugment::HandleUpgradeCandidatesChanged);
	}

	RebuildCards();
}

void UHB_SkillAugment::NativeDestruct()
{
	// PlayerState 는 이 위젯보다 오래 산다. 풀어주지 않으면 죽은 위젯으로 방송된다.
	if (AHB_PlayerState* HBPS = GetHBPlayerState())
	{
		HBPS->OnUpgradeCandidatesChanged.RemoveDynamic(this, &UHB_SkillAugment::HandleUpgradeCandidatesChanged);
	}

	Super::NativeDestruct();
}

void UHB_SkillAugment::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 스택이 위젯 인스턴스를 풀에서 재사용하므로 열릴 때마다 잠금을 푼다.
	// 재사용 시 NativeConstruct 가 다시 불린다는 보장이 없어서 카드도 여기서 다시 그린다.
	bSelectionSubmitted = false;
	bAwaitingNextCandidates = false;
	RebuildCards();

	// K 키로 열 때마다 난다. 고른 뒤 다음 증강으로 이어질 때는 HandleUpgradeCandidatesChanged 가 따로 낸다.
	PlayAugmentSound(OpenSound);
}

UWidget* UHB_SkillAugment::NativeGetDesiredFocusTarget() const
{
	// Menu 입력 모드라 포커스 대상이 있어야 키보드/패드로 고를 수 있다.
	if (CardContainer && CardContainer->GetChildrenCount() > 0)
	{
		return CardContainer->GetChildAt(0);
	}

	return Super::NativeGetDesiredFocusTarget();
}

FReply UHB_SkillAugment::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::K)
	{
		if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
		{
			PC->ToggleSkillAugment(); // 이미 열려있는 상태이므로 ToggleSkillAugment 는 "닫기" 분기로 간다
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

AHB_PlayerState* UHB_SkillAugment::GetHBPlayerState() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPlayerState<AHB_PlayerState>() : nullptr;
}

void UHB_SkillAugment::HandleUpgradeCandidatesChanged()
{
	if (bSelectionSubmitted)
	{
		// 다음 증강을 고를 차례라면 잠금을 풀고 새 후보를 그린다.
		if (bAwaitingNextCandidates)
		{
			bAwaitingNextCandidates = false;
			bSelectionSubmitted = false;
			RebuildCards();

			// 뽑을 후보가 없어 서버가 남은 선택권을 비웠으면 더 고를 게 없다. 스탯창으로 넘어간다.
			const AHB_PlayerState* HBPS = GetHBPlayerState();
			if (HBPS && HBPS->GetPendingUpgradeChoices() <= 0)
			{
				if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
				{
					PC->FinishAugmentSelection();
				}
			}
			else
			{
				// 화면이 닫히지 않고 다음 증강 카드로 바뀌는 경우다. 새로 열린 것처럼 열기 소리를 낸다.
				PlayAugmentSound(OpenSound);
			}
		}

		// 그 외에는 닫히는 중이다. 여기서 다시 그리면 다음 후보가 보인다.
		return;
	}

	RebuildCards();
}

void UHB_SkillAugment::RebuildCards()
{
	if (!CardContainer || !AugmentCardClass)
	{
		return;
	}

	// 디자인 타임에 배치된 카드도 여기서 같이 걷힌다.
	CardContainer->ClearChildren();

	AHB_PlayerState* HBPS = GetHBPlayerState();
	if (!HBPS)
	{
		return;
	}

	for (const FHBUpgradeCandidate& Candidate : HBPS->GetCurrentUpgradeCandidates())
	{
		UHB_AugmentCard* NewCard = CreateWidget<UHB_AugmentCard>(this, AugmentCardClass);
		if (!NewCard)
		{
			continue;
		}

		NewCard->InitCard(Candidate);
		NewCard->OnAugmentCardClickedDelegate.AddDynamic(this, &UHB_SkillAugment::OnAugmentCardClicked);

		// 슬롯을 균등 분할로 못 박는다.
		//
		// 왜 필요한가: 기본값인 Auto 는 카드가 원하는 만큼 가져간다. 카드 배경이
		// 화면만 해서 첫 장이 가로를 다 먹고 나머지 두 장은 오른쪽으로 밀려 잘렸다.
		// 카드가 몇 장이든 폭을 1/N 로 나눠 갖게 한다.
		if (UHorizontalBoxSlot* CardSlot = Cast<UHorizontalBoxSlot>(CardContainer->AddChild(NewCard)))
		{
			CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CardSlot->SetHorizontalAlignment(HAlign_Fill);
			CardSlot->SetVerticalAlignment(VAlign_Fill);
			CardSlot->SetPadding(FMargin(CardSpacing * 0.5f, 0.f));
		}
	}
}

void UHB_SkillAugment::OnAugmentCardClicked(FName ClickedCandidateId)
{
	// 페이드아웃 중에 들어온 두 번째 클릭은 버린다.
	if (bSelectionSubmitted)
	{
		return;
	}
	bSelectionSubmitted = true;

	// 위의 중복 클릭 막기를 지난 뒤에 낸다. 같은 선택이 두 번 울리지 않는다.
	PlayAugmentSound(SelectSound);

	AHB_PlayerState* HBPS = GetHBPlayerState();

	// 클라이언트는 요청이 서버를 다녀와야 남은 개수가 줄어든다. 보내기 전에 센다.
	const bool bHasMoreChoices = HBPS && HBPS->GetPendingUpgradeChoices() > 1;

	// 호스트는 요청 안에서 다음 후보 방송까지 끝나므로 플래그를 먼저 세운다.
	bAwaitingNextCandidates = bHasMoreChoices;

	// 적용은 서버가 한다. 클라는 고른 후보의 이름만 보낸다.
	if (HBPS)
	{
		HBPS->RequestSelectUpgrade(ClickedCandidateId);
	}

	if (bHasMoreChoices)
	{
		// 방금 고른 카드는 지운다. 다음 후보가 올 때까지 빈 화면이 잠깐 보인다.
		// (호스트는 위에서 이미 다음 후보로 다시 그려졌다.)
		if (bSelectionSubmitted && CardContainer)
		{
			CardContainer->ClearChildren();
		}
		return;
	}

	if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
	{
		PC->FinishAugmentSelection(); // 다 골랐다 -> 증강 화면을 닫고 스탯창을 연다
	}
}

UHB_SkillAugment* UHB_SkillAugment::OpenOn(
	AHBPlayerController* OwningPC,
	TSubclassOf<UHB_SkillAugment> ScreenClass)
{
	if (OwningPC == nullptr || ScreenClass == nullptr || OwningPC->GetUILayout() == nullptr)
	{
		return nullptr;
	}

	// 카드 구성은 NativeConstruct 가 한다. 위젯 트리가 만들어진 뒤라야
	// CardContainer 가 유효하다.
	return OwningPC->GetUILayout()->PushWidgetToLayer<UHB_SkillAugment>(
		UITag::TAG_UI_Layer_Menu, ScreenClass);
}
