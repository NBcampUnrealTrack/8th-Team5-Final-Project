// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HB_HUDWidget.generated.h"

/**
 * 
 */

class UImage;
class UMaterialInstanceDynamic;

UCLASS()
class HEAVENBREACH_API UHB_HUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// 매 프레임 보간 처리를 위해 NativeTick 오버라이드
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:

	UFUNCTION(BlueprintCallable, Category = "UI|GAS")
	void BindPlayerState();

protected:

	UFUNCTION()
	void UpdateHPBar(float CurrentHP, float MaxHP);

protected:

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_HP_Bar_Fill;

private:

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HPBarDynamicMaterial;

	float TargetPercent = 1.0f;

	float CurrentPercent = 1.0f;

	UPROPERTY(EditAnywhere, Category = "UI|Animation")
	float InterpSpeed = 5.0f;

	bool bIsInitialized = false;
	
};
