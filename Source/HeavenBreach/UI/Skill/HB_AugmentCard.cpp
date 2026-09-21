// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Skill/HB_AugmentCard.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UHB_AugmentCard::NativeOnClicked()
{
	Super::NativeOnClicked();

	OnAugmentCardClickedDelegate.Broadcast(CachedCandidate.CandidateId);
}

void UHB_AugmentCard::InitCard(const FHBUpgradeCandidate& InCandidate)
{
	CachedCandidate = InCandidate;

	if (TitleText)
	{
		TitleText->SetText(InCandidate.DisplayName);
	}

	if (DescriptionText)
	{
		DescriptionText->SetText(InCandidate.Description);
	}

	if (IconImage)
	{
		// 아이콘은 소프트 참조라 여기서 꺼낸다. 화면이 열릴 때 최대 3장이라
		// 동기 로드로 충분하다. 카드 수가 늘면 비동기로 바꿀 것.
		if (UTexture2D* Icon = InCandidate.Icon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(Icon);
		}
	}
}
