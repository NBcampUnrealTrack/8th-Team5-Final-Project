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
};
