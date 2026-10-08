// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "HB_SkillIconWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * 스킬 아이콘 한 칸을 보여주기만 하는 위젯. 스탯창, HUD 등 여러 곳에서 쓴다.
 *
 * 클릭, 호버, 선택 상태가 없다. 로비에서 스킬을 고르는 슬롯은 UHB_SkillSlotWidget 이고,
 * 이 위젯은 그와 별개다. 로비 쪽 동작이 바뀌어도 여기에는 영향이 없다.
 *
 * 액티브/패시브 디자인은 이 클래스를 부모로 한 WBP 를 각각 만들어 구분한다
 * (WBP_SkillIcon_Active, WBP_SkillIcon_Passive). 코드는 디자인을 모른다.
 */
UCLASS()
class HEAVENBREACH_API UHB_SkillIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 아이콘을 넣는다. nullptr 이면 디자이너가 세팅해 둔 기본(빈 칸) 이미지로 되돌린다. */
	UFUNCTION(BlueprintCallable, Category = "Skill Icon")
	void SetSkillIcon(UTexture2D* NewIcon);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SkillIconImage;

private:
	/** 디자이너가 SkillIconImage 에 세팅해 둔 기본 브러시. */
	FSlateBrush DefaultIconBrush;
};
