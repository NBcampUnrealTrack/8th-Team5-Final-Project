// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "UI/Skill/HB_SkillData.h"
#include "UI/Skill/HB_SkillSlotWidget.h"
#include "UI/Skill/HB_SkillCardWidget.h"
#include "Player/Job/HBStartingLoadout.h"
#include "HB_SkillMainWidget.generated.h"

class AHB_PlayerState;
class UHBJobData;
class UScrollBox;
class UTexture2D;

struct FHBJobSkillData;

/**
 * 로비의 스킬 선택 화면.
 *
 * 이 위젯은 선택 상태를 들고 있지 않는다. 진실은 PlayerState 의 StartingLoadout
 * 하나뿐이고, 여기서는 그것을 비추고 변경 요청만 보낸다.
 *
 *   슬롯 클릭 → 목록 표시
 *   카드 클릭 → 현재 구성 복사 → 해당 칸 교체 → 컨트롤러로 요청
 *   서버 확정 → 복제 → OnStartingLoadoutChanged → RefreshFromPlayerState
 *
 * 쳐낼 조건: 서버 왕복 지연이 눈에 거슬리면 로컬 낙관적 표시를 넣게 된다.
 * 그때는 서버가 거절했을 때 되돌리는 경로가 반드시 같이 있어야 한다.
 */
UCLASS()
class HEAVENBREACH_API UHB_SkillMainWidget : public UHB_ActivatableWidget
{
    GENERATED_BODY()

public:
    UHB_SkillMainWidget();

    /**
     * 화면을 보일 때마다 호출한다. (UI 담당자용 진입점)
     *
     * 왜 밖에서 불러야 하나: 이 위젯이 활성화 스택에 푸시될지 로비 HUD 의
     * WidgetSwitcher 에 놓일지 여기서는 알 수 없다. 스위처면 NativeOnActivated
     * 가 불리지 않으므로 갱신 시점을 밖에서 알려 줘야 한다.
     */
    UFUNCTION(BlueprintCallable, Category = "Lobby|Skills")
    void RefreshFromPlayerState();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeOnActivated() override;

    /** 게임패드·키보드 조작 시 첫 포커스를 Q 슬롯에 준다. */
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    // ---- UMG 바인딩. WBP 가 이 이름으로 물려 있으므로 바꾸지 말 것 ----

    UPROPERTY(meta = (BindWidget))
    UHB_SkillSlotWidget* Slot_Q;

    UPROPERTY(meta = (BindWidget))
    UHB_SkillSlotWidget* Slot_E;

    UPROPERTY(meta = (BindWidget))
    UHB_SkillSlotWidget* Slot_Passive;

    UPROPERTY(meta = (BindWidget))
    UScrollBox* SkillListScrollBox;

    // ---- 에디터 설정 ----

    UPROPERTY(EditAnywhere, Category = "UI Setup")
    TSubclassOf<UHB_SkillCardWidget> SkillCardClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI | Skill Catalog")
    FMargin CardPadding = FMargin(0.0f, 0.0f, 0.0f, 12.0f);

private:
    // ---- 이벤트 핸들러 ----

    UFUNCTION()
    void OnSkillSlotClicked(ESkillSlotType ClickedSlotType);

    UFUNCTION()
    void OnSkillCardClicked(FName ClickedSkillId);

    UFUNCTION()
    void HandleStartingLoadoutChanged();

    // ---- 조회 ----

    AHB_PlayerState*   GetOwningHBPlayerState() const;
    UHBJobData*        GetEditingJob() const;
    FHBStartingLoadout GetCurrentLoadout() const;
    bool               IsSkillAlreadySelected(FName SkillId) const;

    // ---- 목록 ----

    void PopulateSkillList(bool bIsPassive);
    void AddSkillCard(const FSkillCardData& InCardData);

    /** 못 쓸 스킬이면 false. 목록에서 빼야 한다. */
    bool MakeCardData(const FHBJobSkillData& InSkill, ESkillType InType, FSkillCardData& OutCard);

    UTexture2D* ResolveIcon(const FHBJobSkillData& InSkill);

    // ---- 슬롯 ----

    void RefreshSlotIcons();
    void RefreshSlotIcon(UHB_SkillSlotWidget* InSlot, FName InSkillId);

    void UnbindFromPlayerState();

    /** 지금 어느 슬롯을 편집 중인가. 카드 클릭이 이걸 보고 칸을 고른다. */
    ESkillSlotType CurrentEditingSlotType = ESkillSlotType::None;

    /**
     * 아이콘 캐시.
     *
     * 왜 필요한가: FHBJobSkillData::Icon 은 TSoftObjectPtr 이라 목록을 열 때마다
     * LoadSynchronous 가 항목 수만큼 몰린다.
     *
     * 왜 UPROPERTY 인가: 안 그러면 GC 가 걷어가 다음에 빈 아이콘이 나온다.
     *
     * 쳐낼 조건: 직업이 여러 개가 되면 직업이 바뀔 때 비워야 한다. 지금은
     * 직업이 하나라 쌓일 일이 없다.
     */
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UTexture2D>> IconCache;

    /** 구독 중인 PlayerState. 바뀌면 끊고 다시 건다. */
    TWeakObjectPtr<AHB_PlayerState> BoundPlayerState;
};
