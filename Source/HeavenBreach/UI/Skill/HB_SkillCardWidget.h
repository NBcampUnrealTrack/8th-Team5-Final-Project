// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Skill/HB_SkillData.h"
#include "HB_SkillCardWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillCardClicked, FName, ClickedSkillId);

UCLASS()
class HEAVENBREACH_API UHB_SkillCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Skill Card")
    void InitCard(const FSkillCardData& SkillData, bool bIsAlreadySelected);

    UPROPERTY(BlueprintAssignable, Category = "Skill Card|Events")
    FOnSkillCardClicked OnCardClickedDelegate;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> CardButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> IconImage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DisplayNameText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DescriptionText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> ManaCostText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CooldownText;

private:
    UFUNCTION()
    void OnButtonClicked();

    FName CurrentSkillId;
};