// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "HB_StatusTooltip.generated.h"

class UImage;
class UTextBlock;
struct FHBSkillAugment;
struct FHBJobSkillData;

/**
 * 스탯창 증강 노드 위에 마우스를 올렸을 때 뜨는 설명 창.
 * 값을 채우기만 한다. 띄우고 치우는 것은 UMG 의 툴팁 기능이 한다.
 */
UCLASS()
class HEAVENBREACH_API UHB_StatusTooltip : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetAugment(const FHBSkillAugment& Augment, int32 Tier);
	void SetPassive(const FHBJobSkillData& Passive);

	/**
	 * 증강이 아닌 노드(시작, 잠김)용. 브러시와 문구를 그대로 쓴다. 단계는 보이지 않는다.
	 * IconBrush 가 null 이면 툴팁 WBP 에 넣어 둔 기본 아이콘을 그대로 둔다.
	 */
	void SetPlain(const FSlateBrush* IconBrush, const FText& Title, const FText& Description);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** 2단계 이상일 때만 "Lv.N" 으로 보인다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TierText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

private:
	void Fill(UTexture2D* Icon, const FText& Name, const FText& Description, int32 Tier);
};
