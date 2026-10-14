// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Skill/HB_AugmentCard.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UHB_AugmentCard::NativeConstruct()
{
	Super::NativeConstruct();

	// 가운데를 기준으로 커져야 한다. 기본 기준점(왼쪽 위)이면 한쪽으로 쏠려 커진다.
	if (UWidget* Visual = GetRootWidget())
	{
		Visual->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
}

void UHB_AugmentCard::NativeOnHovered()
{
	Super::NativeOnHovered();

	bHovered = true;

	if (!HoverSound.IsNull())
	{
		if (USoundBase* Sound = HoverSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, Sound);
		}
	}
}

void UHB_AugmentCard::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	bHovered = false;
}

void UHB_AugmentCard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAnimateHover)
	{
		return;
	}

	const float Target = bHovered ? 1.0f : 0.0f;

	// 이미 목표에 닿았으면 매 프레임 값을 다시 쓰지 않는다.
	if (HoverAlpha == Target)
	{
		return;
	}

	// FInterpTo 는 목표에 충분히 가까워지면 목표값을 그대로 돌려준다. 그래서 끝에서 멈춘다.
	HoverAlpha = FMath::FInterpTo(HoverAlpha, Target, InDeltaTime, HoverAnimSpeed);

	// 카드 자체(this)가 아니라 안쪽 루트 위젯을 움직인다.
	//
	// 왜: 슬레이트는 렌더 변환까지 반영해서 마우스가 어디 위에 있는지 판정한다. 카드를 직접 위로 띄우면
	// 카드 아래쪽 가장자리에 있던 마우스가 카드 밖이 되어 Unhover → 카드가 내려오면 다시 Hover → ...
	// 하며 마우스를 멈춰도 계속 떨린다. 안쪽만 움직이면 카드의 판정 영역은 그대로라서 떨리지 않는다.
	UWidget* Visual = GetRootWidget();
	if (!Visual)
	{
		return;
	}

	const float Scale = FMath::Lerp(1.0f, HoverScale, HoverAlpha);
	Visual->SetRenderScale(FVector2D(Scale, Scale));
	Visual->SetRenderTranslation(FVector2D(0.0f, -HoverLiftPixels * HoverAlpha));
}

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
