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
class UButton;
class UTexture2D;

struct FHBJobSkillData;

/**
 * 로비의 스킬 선택 화면.
 *
 * 이 위젯은 "확정된" 선택 상태를 들고 있지 않는다. 진실은 PlayerState 의
 * StartingLoadout 하나뿐이고, 여기서는 그것을 비추고 변경 요청만 보낸다.
 * 다만 아직 서버에 보내지 않은 "미리보기 선택"(CurrentPreviewSkillId)만은
 * 이 위젯이 직접 들고 있다 - 그건 로컬 UI 상태일 뿐 게임 상태가 아니기 때문이다.
 *
 *   슬롯 클릭 → 그리드에 후보 스킬 아이콘 나열 (액티브/패시브 중 하나)
 *   아이콘 클릭 → "미리보기"만 함: 오른쪽 상세정보 패널 갱신 + 장착 버튼 활성화
 *   장착 버튼 클릭 → 현재 구성 복사 → 해당 칸 교체 → 컨트롤러로 요청
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
    /**
     * 서버가 시작 구성 변경을 실제로 확정했을 때(=OnStartingLoadoutChanged 발생) 호출된다.
     * "변경사항이 저장되었습니다" 같은 토스트를 여기서 띄우면 된다.
     *
     * 즉시 반영이 아니라 서버 왕복 후에만 오는 신호라, 여기서 토스트를 띄우는 게
     * "저장 확정"의 정확한 시점이다. 로비 최초 접속 시 기본 구성이 정해질 때도
     * 한 번 호출될 수 있음에 유의.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Skills")
    void BP_OnLoadoutSaved();

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

    /** 가운데 스킬 아이콘 그리드. N열(GridColumnCount)로 자동 배치한다. */
    UPROPERTY(meta = (BindWidget))
    UUniformGridPanel* SkillGridPanel;

    /** 위쪽: 지금 이 슬롯에 실제로 장착되어 있는 스킬 정보. WBP_SkillCard 인스턴스를 재사용한다. */
    UPROPERTY(meta = (BindWidget))
    UHB_SkillCardWidget* EquippedSkillCard;

    /** 아래쪽: 그리드에서 미리보기로 선택 중인 스킬 정보. 마찬가지로 WBP_SkillCard 인스턴스. */
    UPROPERTY(meta = (BindWidget))
    UHB_SkillCardWidget* PreviewSkillCard;

    /** 미리보기 중인 스킬을 실제로 장착 확정하는 버튼. */
    UPROPERTY(meta = (BindWidget))
    UButton* EquipButton;

    // ---- 에디터 설정 ----

    UPROPERTY(EditAnywhere, Category = "UI Setup")
    TSubclassOf<UHB_SkillIconCardWidget> SkillIconCardClass;

    UPROPERTY(EditAnywhere, Category = "UI Setup", meta = (ClampMin = "1"))
    int32 GridColumnCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI | Skill Catalog")
    FMargin CardPadding = FMargin(6.0f);

private:
    // ---- 이벤트 핸들러 ----

    UFUNCTION()
    void OnSkillSlotClicked(ESkillSlotType ClickedSlotType);

    UFUNCTION()
    void OnSkillIconClicked(FName ClickedSkillId);

    UFUNCTION()
    void OnEquipButtonClicked();

    UFUNCTION()
    void HandleStartingLoadoutChanged();

    // ---- 조회 ----

    AHB_PlayerState*   GetOwningHBPlayerState() const;
    UHBJobData*        GetEditingJob() const;
    FHBStartingLoadout GetCurrentLoadout() const;

    /** 지정한 슬롯(Q/E/Passive)에 지금 장착된 스킬 ID. 빈 슬롯이면 NAME_None. */
    FName GetEquippedSkillIdForSlot(ESkillSlotType SlotType) const;

    // ---- 목록 ----

    void PopulateSkillList(bool bIsPassive);
    void AddSkillIconCard(const FSkillCardData& InCardData);

    /** 못 쓸 스킬이면 false. 목록에서 빼야 한다. */
    bool MakeCardData(const FHBJobSkillData& InSkill, ESkillType InType, FSkillCardData& OutCard);

    /** "없음"(슬롯 비우기) 카드 데이터를 만든다. PopulateSkillList/미리보기 양쪽에서 쓴다. */
    FSkillCardData MakeEmptyCardData(ESkillType InType) const;

    UTexture2D* ResolveIcon(const FHBJobSkillData& InSkill);

    // ---- 슬롯 ----

    void RefreshSlotIcons();
    void RefreshSlotIcon(UHB_SkillSlotWidget* InSlot, FName InSkillId);

    /** EquippedSkillCard 를 CurrentEditingSlotType 기준 현재 로드아웃으로 갱신한다. */
    void RefreshEquippedCard();

    // ---- 미리보기 (아직 서버에 안 보낸 로컬 선택) ----

    /** 미리보기를 완전히 접는다: 상세정보 패널 숨김, 장착 버튼 비활성화, 그리드 하이라이트 해제. */
    void ClearPreview();

    /** CurrentPreviewSkillId 기준으로 상세정보 패널/장착 버튼/그리드 하이라이트를 다시 그린다. */
    void RefreshPreviewDisplay();

    void UpdatePreviewSelectionHighlight();
    void RefreshEquipButtonState();

    void UnbindFromPlayerState();

    /** 지금 어느 슬롯을 편집 중인가. 아이콘 클릭/장착 버튼이 이걸 보고 칸을 고른다. */
    ESkillSlotType CurrentEditingSlotType = ESkillSlotType::None;

    /** 그리드에서 미리보기로 선택한 스킬. NAME_None 이면 "없음" 카드를 미리보기 중이라는 뜻일 수도 있어서, 실제로 뭔가 선택됐는지는 bHasPreviewSelection 으로 따로 구분한다. */
    FName CurrentPreviewSkillId = NAME_None;

    /** 그리드에서 뭔가(빈 슬롯 카드 포함) 클릭해서 미리보기가 잡혀 있는 상태인가. */
    bool bHasPreviewSelection = false;

    /** 지금 그려진 그리드 카드들. SkillId 로 찾아서 하이라이트를 갱신한다. */
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UHB_SkillIconCardWidget>> IconCardsBySkillId;

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
