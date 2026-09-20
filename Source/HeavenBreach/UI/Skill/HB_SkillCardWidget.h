// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Skill/HB_SkillData.h"
#include "HB_SkillCardWidget.generated.h"

class UButton;
class UTextBlock;
class URichTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillCardClicked, FName, ClickedSkillId);

UCLASS()
class HEAVENBREACH_API UHB_SkillCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Skill Card")
    void InitCard(const FSkillCardData& SkillData);

    /**
     * 같은 WBP_SkillCard 를 "장착 중인 스킬"/"선택한 스킬" 두 군데에 재사용하면서,
     * 어느 쪽인지 구분해 줄 라벨을 바깥(HB_SkillMainWidget)에서 지정할 수 있게 한다.
     * InitCard 와 달리 SkillData 에 속한 값이 아니라 "이 카드가 무슨 역할인지"를
     * 나타내는 고정 문구라 별도 함수로 뺐다.
     */
    UFUNCTION(BlueprintCallable, Category = "Skill Card")
    void SetHeaderText(const FText& InHeaderText);

    UPROPERTY(BlueprintAssignable, Category = "Skill Card|Events")
    FOnSkillCardClicked OnCardClickedDelegate;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> CardButton;

    /** "장착 중인 스킬" / "선택한 스킬" 같은 구분 라벨. 없어도 동작엔 문제없다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> HeaderText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DisplayNameText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> DescriptionText;

    /**
     * 값에 색을 넣을 수 있는 설명 칸.
     *
     * 왜 Optional 인가: 이게 필수면 WBP 에 Rich Text Block 을 넣기 전까지
     * 위젯 컴파일이 깨진다. 있으면 색이 들어가고, 없으면 위의 평문으로 떨어진다.
     *
     * 쓰려면 WBP 에 Rich Text Block 을 DescriptionRichText 라는 이름으로 놓고,
     * Text Style Set 에 Default 행과 파란 Value 행을 가진 DT 를 지정한다.
     * 기존 DescriptionText 는 지우지 말 것. BindWidget 이 깨진다.
     */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<URichTextBlock> DescriptionRichText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> ManaCostText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CooldownText;

private:
    // InitCard 에서 분리했다. 여기서 return 하면 뒤의 마나, 쿨, 아이콘 설정이
    // 통째로 생략되므로 설명 처리는 별도 함수여야 한다.
    void ApplyDescription(const FSkillCardData& SkillData);

    UFUNCTION()
    void OnButtonClicked();

    FName CurrentSkillId;
};