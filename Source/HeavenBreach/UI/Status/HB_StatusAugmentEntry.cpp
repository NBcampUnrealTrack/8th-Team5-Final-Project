// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Status/HB_StatusAugmentEntry.h"
#include "UI/Status/HB_StatusTooltip.h"
#include "Combat/Ability/HBSkillAugment.h"
#include "Player/Job/HBJobData.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

UHB_StatusAugmentEntry::UHB_StatusAugmentEntry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTitle = NSLOCTEXT("StatusWindow", "StartTitle", "시작");
	StartDescription = NSLOCTEXT("StatusWindow", "StartDescription", "여정의 시작입니다. 레벨이 오를 때마다 증강을 하나씩 고를 수 있습니다.");
	LockedTitle = NSLOCTEXT("StatusWindow", "LockedTitle", "잠김");
	LockedDescription = NSLOCTEXT("StatusWindow", "LockedDescription", "Lv {0} 에 도달하면 증강을 고를 수 있습니다.");
}

void UHB_StatusAugmentEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Init 은 AddChild 보다 먼저 불리므로 NativeConstruct 가 아니라 여기서 캐싱한다.
	if (IconImage)
	{
		DefaultIconBrush = IconImage->GetBrush();
	}
}

void UHB_StatusAugmentEntry::SetLevelText(int32 Level)
{
	if (LevelText)
	{
		LevelText->SetText(FText::Format(NSLOCTEXT("StatusWindow", "NodeLevel", "Lv {0}"), FText::AsNumber(Level)));
	}
}

void UHB_StatusAugmentEntry::SetCommon(int32 Level, UTexture2D* Icon)
{
	if (IconImage)
	{
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon);
		}
		else
		{
			IconImage->SetBrush(DefaultIconBrush);
		}
	}

	SetLevelText(Level);
	SetRenderOpacity(1.f);
	ShowStartVisual(false);
}

void UHB_StatusAugmentEntry::ShowStartVisual(bool bShowStart)
{
	// Collapsed 가 아니라 Hidden. 노드 줄 높이가 바뀌면 연결선 위치가 어긋난다.
	if (NodeFrame)
	{
		NodeFrame->SetVisibility(bShowStart ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
	}
	if (StartVisual)
	{
		StartVisual->SetVisibility(bShowStart ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

UWidget* UHB_StatusAugmentEntry::GetTooltipTarget() const
{
	// 보더나 배경 이미지가 아이콘 위에 그려져 있으면 마우스가 그 이미지에 걸려 아이콘의 툴팁이 안 뜬다.
	// 툴팁은 부모에 달면 자식 위에서도 뜨므로, 노드 모양 전체를 묶은 NodeFrame 에 단다.
	return NodeFrame ? static_cast<UWidget*>(NodeFrame) : static_cast<UWidget*>(IconImage);
}

void UHB_StatusAugmentEntry::SetLineEnds(bool bIsFirst, bool bIsLast)
{
	// Collapsed 가 아니라 Hidden. 자리를 유지해야 아이콘이 가운데에 그대로 있다.
	if (LineTop)
	{
		LineTop->SetVisibility(bIsFirst ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
	if (LineBottom)
	{
		LineBottom->SetVisibility(bIsLast ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UHB_StatusAugmentEntry::InitAugmentNode(int32 Level, const FHBSkillAugment& Augment, int32 Tier)
{
	SetCommon(Level, Augment.Icon.LoadSynchronous());

	if (UWidget* Target = GetTooltipTarget(); Target && TooltipClass)
	{
		if (UHB_StatusTooltip* Tip = CreateWidget<UHB_StatusTooltip>(this, TooltipClass))
		{
			Tip->SetAugment(Augment, Tier);
			Target->SetToolTip(Tip);
		}
	}
}

void UHB_StatusAugmentEntry::InitPassiveNode(int32 Level, const FHBJobSkillData& Passive)
{
	SetCommon(Level, Passive.Icon.LoadSynchronous());

	if (UWidget* Target = GetTooltipTarget(); Target && TooltipClass)
	{
		if (UHB_StatusTooltip* Tip = CreateWidget<UHB_StatusTooltip>(this, TooltipClass))
		{
			Tip->SetPassive(Passive);
			Target->SetToolTip(Tip);
		}
	}
}

void UHB_StatusAugmentEntry::InitStartNode(int32 Level)
{
	SetLevelText(Level);
	SetRenderOpacity(1.f);

	// 보더/배경이 없는 시작 전용 모양으로 바꾼다. 일반 노드 모양은 숨긴다.
	ShowStartVisual(true);

	if (StartVisual && TooltipClass)
	{
		if (UHB_StatusTooltip* Tip = CreateWidget<UHB_StatusTooltip>(this, TooltipClass))
		{
			// 툴팁 아이콘은 WBP_StatusTooltip 에 넣어 둔 기본 이미지를 그대로 쓴다.
			Tip->SetPlain(nullptr, StartTitle, StartDescription);
			StartVisual->SetToolTip(Tip);
		}
	}
}

void UHB_StatusAugmentEntry::InitLockedNode(int32 Level)
{
	SetLevelText(Level);
	ShowStartVisual(false);

	// 줄 전체를 흐리게 한다. 아이콘, 레벨 글자, 연결선이 같이 어두워져 아직 안 열린 구간으로 보인다.
	SetRenderOpacity(LockedOpacity);

	if (IconImage)
	{
		IconImage->SetBrush(LockedBrush);
	}

	if (UWidget* Target = GetTooltipTarget(); Target && TooltipClass)
	{
		if (UHB_StatusTooltip* Tip = CreateWidget<UHB_StatusTooltip>(this, TooltipClass))
		{
			Tip->SetPlain(&LockedBrush, LockedTitle, FText::Format(LockedDescription, FText::AsNumber(Level)));
			Target->SetToolTip(Tip);
		}
	}
}
