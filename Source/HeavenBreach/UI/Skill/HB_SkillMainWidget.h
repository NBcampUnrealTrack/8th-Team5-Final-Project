// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "UI/Skill/HB_SkillData.h"
#include "UI/Skill/HB_SkillSlotWidget.h"
#include "UI/Skill/HB_SkillCardWidget.h"
#include "UI/Skill/HB_SkillIconCardWidget.h"
#include "Player/Job/HBStartingLoadout.h"
#include "HB_SkillMainWidget.generated.h"

class AHB_PlayerState;
class UHBJobData;
class UUniformGridPanel;
class UTextBlock;
class UTexture2D;

struct FHBJobSkillData;

/**
 * 로비의 스킬 선택 화면.
 *
 * 이 위젯은 "확정된" 선택 상태를 들고 있지 않는다. 진실은 PlayerState 의
 * StartingLoadout 하나뿐이고, 여기서는 그것을 비추고 변경 요청만 보낸다.
 *
 *   화면 오른쪽에 액티브 목록과 패시브 목록이 처음부터 같이 보인다.
 *   1) 목록의 스킬 아이콘을 마우스로 끌어 왼쪽 장착 슬롯(Q, E, 패시브)에 놓으면 그 슬롯에 바로 장착 요청 (장착 버튼이 없다)
 *      → 서버 확정 → 복제 → 화면 갱신
 *   2) Q/E 칸은 액티브만, 패시브 칸은 패시브만 받는다. 종류가 안 맞으면 놓아도 아무 일이 없다.
 *   3) 이미 다른 칸에 있는 스킬을 Q/E 칸에 놓으면 둘을 맞바꾼다 (Q == E 가 되지 않게).
 *   4) 슬롯에 든 스킬을 다른 슬롯에 끌어다 놓으면 옮기거나 맞바꾸고, 슬롯 밖(이 화면의 다른 곳)에 놓으면 장착이 풀린다.
 *   5) 목록의 스킬을 클릭하면 "집은" 상태가 되어, 그 스킬이 들어갈 수 있는 슬롯에 안내 표시가 뜬다. 다시 클릭하면 푼다.
 *      끌고 있는 동안에도 같은 안내가 뜬다. 클릭만으로는 장착되지 않는다.
 *   이름, 설명, 마나, 쿨타임은 아이콘에 마우스를 올렸을 때 툴팁으로 보인다.
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
    /**
     * [더 이상 불리지 않는다] "변경사항이 저장되었습니다" 문구를 없애기로 해서 호출하지 않는다.
     * 장착이 서버에서 확정되면 슬롯과 목록 표시가 바로 갱신되는 것으로 충분하다.
     *
     * 왜 선언을 남기나: WBP 그래프에 이 이벤트 노드가 아직 있으면 선언을 지우는 순간 그 WBP 가
     * 컴파일 에러가 난다. WBP 에서 노드를 지운 뒤에 이 선언도 지워도 된다.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Skills", meta = (DeprecatedFunction, DeprecationMessage = "더 이상 호출되지 않는다."))
    void BP_OnLoadoutSaved();

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeOnActivated() override;

    /** 게임패드·키보드 조작 시 첫 포커스를 Q 슬롯에 준다. */
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    /**
     * 장착 슬롯에서 끌어낸 스킬을 슬롯이 아닌 곳에 놓으면 여기까지 올라온다. 그 슬롯의 장착을 푼다.
     * 화면 배경이 마우스를 받지 못하는 상태(Visibility)면 이 함수까지 오지 않는다.
     */
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    // ---- UMG 바인딩. WBP 가 이 이름으로 물려 있으므로 바꾸지 말 것 ----

    /** 액티브 대상 칸. Slot Type 을 Active Q 로 지정할 것. */
    UPROPERTY(meta = (BindWidget))
    UHB_SkillSlotWidget* Slot_Q;

    /** 액티브 대상 칸. Slot Type 을 Active E 로 지정할 것. */
    UPROPERTY(meta = (BindWidget))
    UHB_SkillSlotWidget* Slot_E;

    /** 패시브 칸. Slot Type 을 Passive 로 지정할 것. */
    UPROPERTY(meta = (BindWidget))
    UHB_SkillSlotWidget* Slot_Passive;

    /** 오른쪽의 액티브 스킬 목록. N열(GridColumnCount)로 자동 배치한다. */
    UPROPERTY(meta = (BindWidget))
    UUniformGridPanel* ActiveSkillGrid;

    /** 오른쪽의 패시브 스킬 목록. */
    UPROPERTY(meta = (BindWidget))
    UUniformGridPanel* PassiveSkillGrid;

    // ---- 직업 기본 능력치 (선택) ----
    // 로비에는 캐릭터가 없어서 "현재 체력" 같은 것은 없다. 선택한 직업의 기본 능력치(장비, 증강 제외)를 보여 준다.
    // WBP 에 이 이름의 TextBlock 이 없으면 그 줄은 건너뛴다.

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_HP;

    /** 이름은 스태미나지만 최대 마나를 보여 준다. 스킬 비용이 마나라서다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Stamina;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_AttackPower;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Defense;

    // ---- 에디터 설정 ----

    /** 목록의 아이콘 한 칸. WBP_SkillIconCard 를 지정한다. */
    UPROPERTY(EditAnywhere, Category = "UI Setup")
    TSubclassOf<UHB_SkillIconCardWidget> SkillIconCardClass;

    /**
     * 아이콘에 마우스를 올렸을 때 뜨는 설명 창. 기존 WBP_SkillCard 를 그대로 쓸 수 있다.
     * 비우면 툴팁이 뜨지 않는다.
     */
    UPROPERTY(EditAnywhere, Category = "UI Setup")
    TSubclassOf<UHB_SkillCardWidget> SkillTooltipCardClass;

    UPROPERTY(EditAnywhere, Category = "UI Setup", meta = (ClampMin = "1"))
    int32 GridColumnCount = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI | Skill Catalog")
    FMargin CardPadding = FMargin(6.0f);

private:
    // ---- 이벤트 핸들러 ----

    /** 스킬을 슬롯에 끌어다 놓았다. SourceSlot 이 None 이면 목록에서 온 것이다. */
    UFUNCTION()
    void OnSkillDroppedOnSlot(ESkillSlotType TargetSlot, FName SkillId, ESkillSlotType SourceSlot);

    /** 목록의 스킬을 클릭했다. 집고 있지 않으면 집고, 이미 집은 스킬이면 놓는다. */
    UFUNCTION()
    void OnSkillIconClicked(FName ClickedSkillId);

    /** 목록의 스킬을 끌기 시작했다. 끌고 있는 동안에도 집은 것으로 보고 슬롯에 안내를 띄운다. */
    UFUNCTION()
    void OnSkillIconDragStarted(FName DraggedSkillId, UDragDropOperation* Operation);

    /** 끌기가 끝났다(슬롯에 놓았거나 취소했다). 집은 상태를 푼다. */
    UFUNCTION()
    void OnSkillDragFinished(UDragDropOperation* Operation);

    UFUNCTION()
    void HandleStartingLoadoutChanged();

    // ---- 조회 ----

    AHB_PlayerState*   GetOwningHBPlayerState() const;
    UHBJobData*        GetEditingJob() const;
    FHBStartingLoadout GetCurrentLoadout() const;

    // ---- 목록 ----

    /** 직업이 바뀌었을 때만 두 목록을 다시 만든다. 그 외에는 장착 표시만 갱신한다. */
    void EnsureSkillLists();

    void AddSkillIconCard(UUniformGridPanel* Grid, int32 Index, const FSkillCardData& InCardData);

    /** 못 쓸 스킬이면 false. 목록에서 빼야 한다. */
    bool MakeCardData(const FHBJobSkillData& InSkill, ESkillType InType, FSkillCardData& OutCard);

    UTexture2D* ResolveIcon(const FHBJobSkillData& InSkill);

    /** 지금 장착된 스킬들의 아이콘에 "장착됨" 표시(SelectedBorder)를 켠다. */
    void RefreshEquippedHighlights();

    // ---- 슬롯 ----

    void RefreshSlotIcons();
    void RefreshSlotIcon(UHB_SkillSlotWidget* InSlot, FName InSkillId);

    /** 선택한 직업의 기본 능력치를 글자 칸에 채운다. */
    void RefreshJobStats();

    /** 로드아웃을 서버에 보낸다. 이 요청이 곧 저장이다. */
    void SubmitLoadout(const FHBStartingLoadout& NewLoadout);

    // ---- 집은 스킬 ----
    // 목록의 스킬을 클릭하면 "집은" 상태가 된다. 집은 동안 그 스킬이 들어갈 수 있는 장착 슬롯에 안내(SetDropHintActive)가 뜬다.
    // 장착은 하지 않는다. 장착은 끌어다 놓아서만 한다.

    /** None 이면 집은 스킬이 없는 것이다. 카드와 슬롯의 표시를 같이 맞춘다. */
    void SetHeldSkill(FName InSkillId);

    /** 집은 스킬이 들어갈 수 있는 슬롯에만 안내를 켠다. 이미 그 슬롯에 들어 있으면 켜지 않는다. */
    void RefreshDropHints();

    FName HeldSkillId;

    void UnbindFromPlayerState();

    /** 지금 그려진 목록의 카드들. SkillId 로 찾아서 장착 표시를 갱신한다. */
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UHB_SkillIconCardWidget>> IconCardsBySkillId;

    /** 목록을 만들 때 기준이 된 직업. 바뀌면 목록을 다시 만든다. */
    TWeakObjectPtr<UHBJobData> ListedJob;

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
