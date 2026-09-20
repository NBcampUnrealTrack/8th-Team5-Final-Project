// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/Skill/HB_SkillData.h"
#include "HB_SkillIconCardWidget.generated.h"

class UButton;
class UImage;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillIconClicked, FName, ClickedSkillId);

/**
 * 스킬 선택 그리드의 한 칸. 아이콘만 보여준다 - 이름/설명/마나/쿨타임은
 * 오른쪽 상세정보 패널(WBP_SkillCard 재사용)이 맡는다.
 *
 * 클릭해도 바로 장착되지 않는다. "미리보기 선택"만 하고, 실제 장착은
 * UHB_SkillMainWidget 의 EquipButton 을 눌러야 확정된다.
 */
UCLASS()
class HEAVENBREACH_API UHB_SkillIconCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitIconCard(const FSkillCardData& SkillData, bool bIsEquipped);

	/** 지금 오른쪽 상세정보 패널에 미리보기로 떠 있는 카드인지 표시한다. */
	void SetPreviewSelected(bool bSelected);

	FName GetSkillId() const { return CurrentSkillId; }

	UPROPERTY(BlueprintAssignable, Category = "Skill Icon Card|Events")
	FOnSkillIconClicked OnIconClickedDelegate;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	/** 이 스킬이 (다른 슬롯 포함) 이미 장착 중일 때만 보여줄 뱃지. 없어도 동작엔 문제없다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EquippedBadge;

	/** 미리보기로 선택됐을 때 켜지는 테두리/글로우. 없어도 동작엔 문제없다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedBorder;

private:
	UFUNCTION()
	void HandleCardClicked();

	FName CurrentSkillId;

	/** WBP 디자이너가 IconImage 에 세팅해 둔 기본(빈 슬롯) 브러시. NativeConstruct 에서 캐싱한다. */
	FSlateBrush DefaultIconBrush;
};
