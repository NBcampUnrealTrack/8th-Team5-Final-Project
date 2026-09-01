// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "Engine/DataTable.h"
#include "UI/Skill/HB_SkillData.h"
#include "UI/Skill/HB_SkillSlotWidget.h"
#include "UI/Skill/HB_SkillCardWidget.h"
#include "HB_SkillMainWidget.generated.h"

/**
 *
 */
UCLASS()
class HEAVENBREACH_API UHB_SkillMainWidget : public UHB_ActivatableWidget
{
    GENERATED_BODY()

public:
    UHB_SkillMainWidget();

protected:
    virtual void NativeOnActivated() override;

    // 게임패드/키보드 진입 시 자동 포커스 대상 지정
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    // --- UMG 바인딩 ---
    UPROPERTY(meta = (BindWidget))
    class UHB_SkillSlotWidget* Slot_Q;

    UPROPERTY(meta = (BindWidget))
    class UHB_SkillSlotWidget* Slot_E;

    UPROPERTY(meta = (BindWidget))
    class UHB_SkillSlotWidget* Slot_Passive;

    UPROPERTY(meta = (BindWidget))
    class UScrollBox* SkillListScrollBox;

    // --- 데이터 및 클래스 설정 ---
    UPROPERTY(EditAnywhere, Category = "UI Setup")
    TSubclassOf<class UHB_SkillCardWidget> SkillCardClass;

    UPROPERTY(EditAnywhere, Category = "UI Setup")
    UDataTable* SkillCatalog;

public:
    UFUNCTION(BlueprintCallable, Category = "Lobby")
    void RequestApplyLoadout();

private:
    // --- 저장용 변수 ---
    FName SelectedSkillQId;
    FName SelectedSkillEId;
    FName SelectedPassiveSkillId;
    ESkillSlotType CurrentEditingSlotType;

    // --- 이벤트 핸들러 ---
    UFUNCTION()
    void OnSkillSlotClicked(ESkillSlotType ClickedSlotType);

    UFUNCTION()
    void OnSkillCardClicked(FName ClickedSkillId);

    // --- 헬퍼 함수 ---
    void PopulateSkillList(bool bIsPassive);
    bool IsSkillAlreadySelected(FName SkillId);
};