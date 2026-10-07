// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "HB_StatusAugmentEntry.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;
class UHB_StatusTooltip;
struct FHBSkillAugment;
struct FHBJobSkillData;

/**
 * 스탯창 증강 타임라인의 노드 하나. 선택 순서(Lv N)와 아이콘을 보여주고,
 * 아이콘에 마우스를 올리면 설명 툴팁이 뜬다.
 *
 * 어떤 선택을 몇 번째에 보여줄지는 UHB_StatusWindow 가 정한다.
 */
UCLASS()
class HEAVENBREACH_API UHB_StatusAugmentEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	UHB_StatusAugmentEntry(const FObjectInitializer& ObjectInitializer);

	/** Level 은 이 카드를 고르게 한 캐릭터 레벨이다. 화면에 "Lv N" 으로 그대로 찍힌다. */
	void InitAugmentNode(int32 Level, const FHBSkillAugment& Augment, int32 Tier);

	/** 레벨업으로 얻은 패시브. 단계 표시는 없다. */
	void InitPassiveNode(int32 Level, const FHBJobSkillData& Passive);

	/** Lv 1. 선택이 없는 시작 노드. StartBrush 와 시작 문구를 쓴다. */
	void InitStartNode(int32 Level);

	/** 아직 도달하지 못한 레벨. LockedBrush 와 잠김 문구를 쓰고 줄 전체를 흐리게 한다. */
	void InitLockedNode(int32 Level);

	/**
	 * 타임라인의 맨 위/맨 아래 노드에서 선이 바깥으로 튀어나오지 않게 한다.
	 * 첫 노드는 위쪽 절반, 마지막 노드는 아래쪽 절반을 숨긴다. 노드가 하나뿐이면 둘 다 숨는다.
	 */
	void SetLineEnds(bool bIsFirst, bool bIsLast);

protected:
	virtual void NativeOnInitialized() override;

	/** 마우스를 올릴 수 있어야 툴팁이 뜬다. 디자이너에서 Visibility 를 Visible 로 둘 것. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** "Lv N" */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	/** 아이콘 중심에서 위로 뻗는 선 절반. Image 든 SizeBox 든 상관없다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LineTop;

	/** 아이콘 중심에서 아래로 뻗는 선 절반. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LineBottom;

	/** 호버 시 뜰 설명 창. WBP_StatusTooltip 을 지정한다. 비우면 툴팁이 뜨지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	TSubclassOf<UHB_StatusTooltip> TooltipClass;

	/**
	 * 보더, 배경, 아이콘을 묶은 일반 노드 모양 전체. 시작 노드일 때 통째로 숨긴다.
	 * IconImage 가 이 안에 있어야 한다. 비워 두면 시작 노드에서도 일반 모양이 그대로 보인다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> NodeFrame;

	/**
	 * Lv 1 시작 노드 전용 모양. 보더와 배경 없이 네모난 모양만 놓는다.
	 * 평소에는 숨어 있고 시작 노드에서만 보인다. 호버할 수 있게 Visible 로 둘 것.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StartVisual;

	UPROPERTY(EditDefaultsOnly, Category = "UI Setup|Start")
	FText StartTitle;

	UPROPERTY(EditDefaultsOnly, Category = "UI Setup|Start", meta = (MultiLine = "true"))
	FText StartDescription;

	/** 아직 도달하지 못한 레벨의 아이콘 (자물쇠 등). */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup|Locked")
	FSlateBrush LockedBrush;

	UPROPERTY(EditDefaultsOnly, Category = "UI Setup|Locked")
	FText LockedTitle;

	/** {0} 에 도달해야 하는 레벨이 들어간다. */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup|Locked", meta = (MultiLine = "true"))
	FText LockedDescription;

	/** 잠긴 노드 줄 전체의 불투명도. */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup|Locked", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockedOpacity = 0.4f;

private:
	void SetCommon(int32 Level, UTexture2D* Icon);

	/** 일반 노드의 툴팁을 달 위젯. NodeFrame 이 있으면 그것, 없으면 IconImage. */
	UWidget* GetTooltipTarget() const;

	/** 일반 노드 모양과 시작 노드 모양 중 어느 쪽을 보일지 정한다. */
	void ShowStartVisual(bool bShowStart);
	void SetLevelText(int32 Level);

	/** 디자이너가 IconImage 에 세팅해 둔 기본 브러시. 아이콘이 없을 때 이걸로 되돌린다. */
	FSlateBrush DefaultIconBrush;
};
