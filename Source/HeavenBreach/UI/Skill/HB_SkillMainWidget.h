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
 *   1) 왼쪽에서 바꿀 슬롯(Q, E, 패시브)을 먼저 클릭한다. 같은 슬롯을 다시 누르면 선택이 풀린다.
 *   2) 오른쪽 목록에서 스킬 아이콘을 클릭하면 그 슬롯에 바로 장착 요청 (장착 버튼이 없다)
 *      → 서버 확정 → 복제 → 화면 갱신
 *   슬롯을 고르지 않고 누른 스킬이나 슬롯 종류와 맞지 않는 스킬은 무시한다.
 *   슬롯에 이미 들어 있는 스킬을 그 슬롯이 선택된 채로 다시 클릭하면 장착이 풀린다.
 *   Q 가 선택됐는데 E 에 있는 스킬을 고르면 둘을 맞바꾼다 (Q == E 가 되지 않게).
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

    /** 장착하고 나면 슬롯 선택을 푼다. 끄면 같은 슬롯에 여러 스킬을 연달아 눌러 볼 수 있다. */
    UPROPERTY(EditAnywhere, Category = "UI Setup")
    bool bDeselectSlotAfterEquip = true;

    /** 슬롯을 골랐을 때 그 슬롯과 종류가 맞지 않는 쪽 목록의 불투명도. 어느 목록을 눌러야 하는지 알려 준다. */
    UPROPERTY(EditAnywhere, Category = "UI Setup", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InactiveListOpacity = 0.4f;

private:
    // ---- 이벤트 핸들러 ----

    UFUNCTION()
    void OnSkillSlotClicked(ESkillSlotType ClickedSlotType);

    UFUNCTION()
    void OnSkillIconClicked(FName ClickedSkillId);

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

    /**
     * 바꿀 슬롯을 고른다. None 이면 선택을 푼다.
     * 슬롯의 선택 표시와, 종류가 맞지 않는 목록의 흐림을 같이 맞춘다.
     */
    void SelectSlot(ESkillSlotType NewSelection);

    void UnbindFromPlayerState();

    /** 지금 선택된 슬롯. 스킬 아이콘을 누르면 이 슬롯에 들어간다. None 이면 아이콘을 눌러도 아무 일도 없다. */
    ESkillSlotType SelectedSlot = ESkillSlotType::None;

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
