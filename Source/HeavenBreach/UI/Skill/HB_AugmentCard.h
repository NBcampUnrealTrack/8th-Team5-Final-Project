// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "HB_AugmentData.h"
#include "HB_AugmentCard.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAugmentCardClicked, FName, ClickedAugmentId);

UCLASS()
class HEAVENBREACH_API UHB_AugmentCard : public UCommonButtonBase
{
	GENERATED_BODY()
	
public:
    void InitCard(const FAugmentCardData& InData);

    UPROPERTY(BlueprintAssignable, Category = "Augment Card|Events")
    FOnAugmentCardClicked OnAugmentCardClickedDelegate;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnClicked() override;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* TitleText;

    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* DescriptionText;

    UPROPERTY(meta = (BindWidgetOptional))
    class UImage* IconImage;

    FAugmentCardData CachedAugmentData;
};
