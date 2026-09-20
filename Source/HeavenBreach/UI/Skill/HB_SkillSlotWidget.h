// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "HB_SkillSlotWidget.generated.h"

/**
 * 
 */

class UButton;
class UImage;
class UTexture2D;


UENUM(BlueprintType)
enum class ESkillSlotType : uint8
{
	None UMETA(DisplayName = "None"),
	Active_Q UMETA(DisplayName = "Active Q"),
	Active_E UMETA(DisplayName = "Active E"),
	Passive UMETA(DisplayName = "Passive")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillSlotClicked, ESkillSlotType, SlotType);

UCLASS()
class HEAVENBREACH_API UHB_SkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 에디터에서 해당 슬롯이 Q, E, Passive 중 무엇인지 설정할 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
	ESkillSlotType SlotType;

	// 외부(WBP_LobbyUI 등)에서 이벤트를 바인딩할 수 있도록 노출
	UPROPERTY(BlueprintAssignable, Category = "Skill Slot|Events")
	FOnSkillSlotClicked OnSlotClicked;

	// 나중에 스킬이 선택되었을 때 아이콘 이미지를 교체하기 위한 함수
	UFUNCTION(BlueprintCallable, Category = "Skill Slot")
	void SetSkillIcon(UTexture2D* NewIcon);

	/**
	 * 이 슬롯이 지금 "편집 중"(클릭해서 고정된) 상태인지 설정한다.
	 * 호버와는 별개의 상태다 - 호버는 마우스를 떼면 풀리지만, 이건 다른 슬롯을
	 * 클릭하기 전까지 계속 켜져 있어야 한다. Q/E/Passive 중 셋 중 하나만 켜지는
	 * 배타적 처리는 UHB_SkillMainWidget 이 세 슬롯에 각각 이 함수를 불러서 맡는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Slot")
	void SetSlotSelected(bool bSelected);

	UFUNCTION(BlueprintPure, Category = "Skill Slot")
	bool IsSlotSelected() const { return bIsSlotSelected; }

	/**
	 * bIsSlotSelected 가 바뀔 때마다 불린다. WBP 그래프에서 이걸 구현해서
	 * 글로우 애니메이션을 켜고 끈다 (true 면 Play Animation, false 면 Play
	 * Animation Reverse). OnUnhovered 쪽에서는 IsSlotSelected() 가 true 인
	 * 동안은 글로우를 끄지 않도록 분기해야 호버가 끝나도 선택 글로우가 유지된다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Slot|Events")
	void OnSlotSelectedChanged(bool bSelected);

protected:
	virtual void NativeConstruct() override;

	// 블루프린트 위젯의 컴포넌트와 동일한 이름으로 생성해야 바인딩 됨
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SkillIconImage;

private:
	// 버튼이 클릭되었을 때 실행될 내부 함수
	UFUNCTION()
	void HandleSlotButtonClicked();

	/** WBP 디자이너가 SkillIconImage 에 세팅해 둔 기본(빈 슬롯) 브러시. NativeConstruct 에서 캐싱한다. */
	FSlateBrush DefaultIconBrush;

	/** 지금 이 슬롯이 클릭으로 고정 선택된 상태인가. */
	bool bIsSlotSelected = false;
};
