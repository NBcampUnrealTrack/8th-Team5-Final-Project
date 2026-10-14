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
class UTexture2D;
class UDragDropOperation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillIconClicked, FName, ClickedSkillId);

/** 이 카드를 끌기 시작했다. Operation 으로 끌기가 끝났는지(놓음/취소) 알 수 있다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillIconDragStarted, FName, DraggedSkillId, UDragDropOperation*, Operation);

/**
 * 스킬 선택 그리드의 한 칸. 아이콘만 보여준다 - 이름/설명/마나/쿨타임은
 * 마우스를 올렸을 때 뜨는 툴팁이 맡는다.
 *
 * 장착은 이 아이콘을 왼쪽 장착 슬롯으로 끌어다 놓아서 한다 (UHB_SkillMainWidget 이 받는다).
 * 클릭하면 "집은 상태"가 되어, 놓을 수 있는 장착 슬롯에 표시가 뜬다 (장착은 하지 않는다).
 */
UCLASS()
class HEAVENBREACH_API UHB_SkillIconCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitIconCard(const FSkillCardData& SkillData);

	/** 장착 중인 스킬인지 표시한다 (SelectedBorder 를 켜고 끈다). */
	void SetPreviewSelected(bool bSelected);

	/**
	 * 마우스를 올렸을 때 뜰 상세 툴팁(이름, 설명, 마나, 쿨타임)을 단다.
	 *
	 * 왜 따로 함수인가: InitIconCard 가 버튼에 이름만 있는 글자 툴팁을 달아 두는데, 슬레이트는
	 * 마우스 아래의 가장 안쪽 위젯의 툴팁을 쓴다. 이 카드 자체에 상세 툴팁을 달아도 버튼의
	 * 글자 툴팁이 이겨서 가려진다. 그래서 상세 툴팁을 달면서 버튼의 글자 툴팁을 걷어 낸다.
	 */
	void SetDetailTooltip(UWidget* InTooltip);

	/**
	 * 클릭으로 "집은" 카드인지 표시한다 (HeldBorder 를 켜고 끈다). WBP 그래프에서 OnHeldChanged 를 구현해
	 * 애니메이션을 넣을 수도 있다.
	 */
	void SetHeld(bool bHeld);

	FName GetSkillId() const { return CurrentSkillId; }
	ESkillType GetSkillType() const { return CurrentSkillType; }

	UPROPERTY(BlueprintAssignable, Category = "Skill Icon Card|Events")
	FOnSkillIconClicked OnIconClickedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Skill Icon Card|Events")
	FOnSkillIconDragStarted OnIconDragStartedDelegate;

protected:
	virtual void NativeConstruct() override;

	/**
	 * 버튼이 마우스 누름을 먼저 먹어서 일반 NativeOnMouseButtonDown 은 이 카드까지 안 온다.
	 * 자식보다 먼저 불리는 Preview 쪽에서 끌기를 감지해야 한다.
	 * 그 대신 이 카드의 버튼 클릭(OnClicked)은 더 이상 발생하지 않는다.
	 */
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	/** 끌지 않고 뗐으면 클릭이다. 버튼이 누름을 못 받으므로 이 카드가 클릭을 직접 판정한다. */
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** 집은 상태가 바뀔 때마다 불린다. HeldBorder 만으로 부족하면 WBP 그래프에서 구현한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Icon Card|Events")
	void OnHeldChanged(bool bHeld);

	/** 클릭으로 집었을 때 켜지는 테두리/글로우. 없어도 동작엔 문제없다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HeldBorder;

	/** 끌 때 커서에 붙는 아이콘의 한 변 크기. */
	UPROPERTY(EditAnywhere, Category = "Skill Icon Card|Drag", meta = (ClampMin = "16.0"))
	float DragVisualSize = 72.0f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	/** 미리보기로 선택됐을 때 켜지는 테두리/글로우. 없어도 동작엔 문제없다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedBorder;

private:
	UFUNCTION()
	void HandleCardClicked();

	/** 눌렀다 뗀 것이 클릭이면 한 번만 알린다. 버튼 쪽 클릭과 이 카드의 마우스 뗌이 겹쳐도 한 번만 나간다. */
	void BroadcastClickOnce();

	/** 마우스를 눌렀고 아직 끌기도 클릭도 되지 않은 상태. */
	bool bClickPending = false;

	FName CurrentSkillId;

	ESkillType CurrentSkillType = ESkillType::Active;

	/** 끌 때 보여 줄 아이콘. 목록이 이미 들고 있는 텍스처라 GC 걱정은 목록 쪽(IconCache)이 맡는다. */
	TWeakObjectPtr<UTexture2D> CurrentIcon;

	/** WBP 디자이너가 IconImage 에 세팅해 둔 기본(빈 슬롯) 브러시. NativeConstruct 에서 캐싱한다. */
	FSlateBrush DefaultIconBrush;
};
