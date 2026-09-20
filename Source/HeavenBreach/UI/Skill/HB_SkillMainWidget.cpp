// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillMainWidget.h"
#include "UI/Skill/HB_SkillSlotWidget.h"
#include "UI/Skill/HB_SkillCardWidget.h"
#include "UI/Skill/HB_SkillIconCardWidget.h"
#include "UI/Skill/HB_SkillData.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/Job/HBJobData.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "Components/Button.h"
#include "Components/UniformGridPanel.h"
#include "Engine/Texture2D.h"

UHB_SkillMainWidget::UHB_SkillMainWidget()
{
    // 1. CommonUI 입력 모드 선언 (메뉴 모드: 게임 입력 차단 + 커서 활성화)
    InputMode = EHBWidgetInputMode::Menu;

    // 2. ESC 키 또는 패드 B 버튼 입력 시 자기를 자동으로 Deactivate 처리
    bIsBackHandler = true;

    // 위 둘은 활성화 스택에 푸시될 때만 의미가 있다. WidgetSwitcher 에 놓이면
    // 무시되지만 해로울 것은 없어 그대로 둔다.
}

void UHB_SkillMainWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 슬롯 바인딩을 NativeOnActivated 가 아니라 여기에 둔다.
    //
    // 왜: NativeOnActivated 는 활성화 스택에 푸시될 때만 불린다. 이 위젯이
    // 로비 HUD 의 WidgetSwitcher 에 놓이면 영영 안 불려 슬롯 클릭이 죽는다.
    // NativeConstruct 는 어느 쪽이든 불린다.
    if (Slot_Q)
    {
        Slot_Q->OnSlotClicked.AddUniqueDynamic(this, &UHB_SkillMainWidget::OnSkillSlotClicked);
    }
    if (Slot_E)
    {
        Slot_E->OnSlotClicked.AddUniqueDynamic(this, &UHB_SkillMainWidget::OnSkillSlotClicked);
    }
    if (Slot_Passive)
    {
        Slot_Passive->OnSlotClicked.AddUniqueDynamic(this, &UHB_SkillMainWidget::OnSkillSlotClicked);
    }
    if (EquipButton)
    {
        EquipButton->OnClicked.AddUniqueDynamic(this, &UHB_SkillMainWidget::OnEquipButtonClicked);
    }

    // UUniformGridSlot 은 칸별 Padding 을 지원하지 않는다 (Row/Column/정렬만 있음).
    // 대신 패널 전체에 적용되는 SlotPadding 을 한 번만 설정한다 - 모든 칸이 균일한
    // 간격을 가지므로 어차피 칸마다 다르게 줄 이유도 없다.
    if (SkillGridPanel)
    {
        SkillGridPanel->SetSlotPadding(CardPadding);
    }

    // 처음엔 아무것도 미리보기 중이 아니다: 상세정보 패널 숨김, 장착 버튼 비활성화.
    ClearPreview();

    RefreshFromPlayerState();
}

void UHB_SkillMainWidget::NativeDestruct()
{
    UnbindFromPlayerState();

    Super::NativeDestruct();
}

void UHB_SkillMainWidget::NativeOnActivated()
{
    Super::NativeOnActivated();

    // 스택에 푸시되는 경우의 보조 갱신이다. 여기에만 의존하지 말 것.
    RefreshFromPlayerState();
}

UWidget* UHB_SkillMainWidget::NativeGetDesiredFocusTarget() const
{
    // UI가 열렸을 때 첫 번째로 포커스를 받을 위젯 지정 (패드/키보드 조작 대응)
    if (Slot_Q)
    {
        return Slot_Q;
    }
    return Super::NativeGetDesiredFocusTarget();
}

void UHB_SkillMainWidget::RefreshFromPlayerState()
{
    // PlayerState 는 위젯보다 늦게 도착할 수 있다. 생성 시 한 번 잡고 마는 대신
    // 갱신할 때마다 확인해서 붙인다.
    AHB_PlayerState* State = GetOwningHBPlayerState();

    if (State && BoundPlayerState.Get() != State)
    {
        UnbindFromPlayerState();
        State->OnStartingLoadoutChanged.AddUniqueDynamic(
            this, &UHB_SkillMainWidget::HandleStartingLoadoutChanged);
        BoundPlayerState = State;
    }

    RefreshSlotIcons();
    RefreshEquippedCard();

    // 그리드가 열려 있으면(=슬롯을 하나라도 고른 적 있으면) 목록/미리보기도 같이 갱신한다.
    if (!SkillGridPanel)
    {
        return;
    }
    if (SkillGridPanel->GetChildrenCount() == 0)
    {
        return;
    }

    PopulateSkillList(CurrentEditingSlotType == ESkillSlotType::Passive);
}

void UHB_SkillMainWidget::HandleStartingLoadoutChanged()
{
    RefreshFromPlayerState();

    // 서버가 확정한 뒤에만 오는 신호이므로, 여기가 "저장 완료"를 알릴 정확한 시점이다.
    BP_OnLoadoutSaved();
}

void UHB_SkillMainWidget::UnbindFromPlayerState()
{
    AHB_PlayerState* Bound = BoundPlayerState.Get();

    if (!Bound)
    {
        BoundPlayerState.Reset();
        return;
    }

    Bound->OnStartingLoadoutChanged.RemoveDynamic(
        this, &UHB_SkillMainWidget::HandleStartingLoadoutChanged);
    BoundPlayerState.Reset();
}

AHB_PlayerState* UHB_SkillMainWidget::GetOwningHBPlayerState() const
{
    const APlayerController* Owner = GetOwningPlayer();

    if (!Owner)
    {
        return nullptr;
    }
    return Owner->GetPlayerState<AHB_PlayerState>();
}

UHBJobData* UHB_SkillMainWidget::GetEditingJob() const
{
    const AHB_PlayerState* State = GetOwningHBPlayerState();

    if (!State)
    {
        return nullptr;
    }
    return State->GetSelectedJob();
}

FHBStartingLoadout UHB_SkillMainWidget::GetCurrentLoadout() const
{
    const AHB_PlayerState* State = GetOwningHBPlayerState();

    if (!State)
    {
        return FHBStartingLoadout();
    }
    return State->GetStartingLoadout();
}

FName UHB_SkillMainWidget::GetEquippedSkillIdForSlot(ESkillSlotType SlotType) const
{
    const FHBStartingLoadout Loadout = GetCurrentLoadout();

    switch (SlotType)
    {
    case ESkillSlotType::Active_Q:
        return Loadout.SkillQId;
    case ESkillSlotType::Active_E:
        return Loadout.SkillEId;
    case ESkillSlotType::Passive:
        return Loadout.PassiveSkillId;
    default:
        return NAME_None;
    }
}

void UHB_SkillMainWidget::OnSkillSlotClicked(ESkillSlotType ClickedSlotType)
{
    // ClickedSlotType 이 항상 None 으로 찍히면, WBP_SkillMain 에 배치한
    // Slot_Q/Slot_E/Slot_Passive 인스턴스의 "Slot Type" 값을 디자이너에서
    // 안 정해준 것이다 (전부 기본값 None 인 상태).
    UE_LOG(LogTemp, Log, TEXT("SkillMain | 슬롯 클릭됨: %d"), static_cast<int32>(ClickedSlotType));

    CurrentEditingSlotType = ClickedSlotType;

    // 클릭한 슬롯만 선택 글로우를 켜고 나머지는 끈다. 셋 중 하나만 켜지는
    // 배타적 처리를 여기서 한 번에 맡는다 - 그리드 아이콘 쪽 SetPreviewSelected 와
    // 같은 패턴이다.
    if (Slot_Q)
    {
        Slot_Q->SetSlotSelected(ClickedSlotType == ESkillSlotType::Active_Q);
    }
    if (Slot_E)
    {
        Slot_E->SetSlotSelected(ClickedSlotType == ESkillSlotType::Active_E);
    }
    if (Slot_Passive)
    {
        Slot_Passive->SetSlotSelected(ClickedSlotType == ESkillSlotType::Passive);
    }

    // 위쪽 "장착됨" 카드는 슬롯이 바뀌는 즉시 그 슬롯 기준으로 다시 그린다.
    RefreshEquippedCard();

    // 슬롯이 바뀌면 이전 슬롯에서 미리보던 스킬은 더 이상 의미가 없다.
    ClearPreview();

    PopulateSkillList(ClickedSlotType == ESkillSlotType::Passive);
}

void UHB_SkillMainWidget::PopulateSkillList(bool bIsPassive)
{
    if (!SkillGridPanel)
    {
        // WBP_SkillMain 에 SkillGridPanel 이라는 이름의 UniformGridPanel 이 없거나
        // 이름이 다르면 BindWidget 이 실패해서 여기서 조용히 막힌다.
        UE_LOG(LogTemp, Error, TEXT("SkillMain | PopulateSkillList 중단: SkillGridPanel 바인딩 실패"));
        return;
    }

    SkillGridPanel->ClearChildren();
    IconCardsBySkillId.Empty();

    // 목록의 출처는 직업 DA 다. 예전에는 SkillCatalog(DataTable)를 봤는데,
    // 그러면 직업과 무관한 스킬까지 섞여 나오고 값의 주인이 둘이 된다.
    UHBJobData* Job = GetEditingJob();

    if (!Job)
    {
        // PlayerState.StartingLoadout.Job 이 비어있다는 뜻이다. 보통
        // BP_HBLobbyGameMode 의 SelectableJobs 가 비어 있을 때 이렇게 된다
        // (AHBLobbyGameMode::InitializeLobbyLoadout 참고 - 그쪽에도 별도 로그가 남는다).
        UE_LOG(LogTemp, Error, TEXT("SkillMain | PopulateSkillList 중단: GetEditingJob() == nullptr (직업 미확정)"));
        return;
    }
    if (!SkillIconCardClass)
    {
        // WBP_SkillMain 블루프린트의 클래스 디폴트(Details 패널)에서
        // "Skill Icon Card Class" 를 WBP_SkillIconCard 로 지정했는지 확인할 것.
        UE_LOG(LogTemp, Error, TEXT("SkillMain | PopulateSkillList 중단: SkillIconCardClass 가 클래스 디폴트에 설정되지 않음"));
        return;
    }

    ESkillType Type = ESkillType::Active;
    TArray<FHBJobSkillData> Skills = Job->GetActiveSkillCatalog();

    if (bIsPassive)
    {
        Type = ESkillType::Passive;
        Skills = Job->GetPassiveSkillCatalog();
    }

    // 지금 이 슬롯에 이미 들어있는 것(스킬이면 그 스킬, 빈 슬롯이면 "없음")은
    // 다시 골라도 아무 의미가 없으니 목록에서 아예 뺀다.
    const FName EquippedInThisSlot = GetEquippedSkillIdForSlot(CurrentEditingSlotType);

    // "없음" 카드는 슬롯을 비우는 유일한 방법이다. 슬롯 위젯에는 버튼이 하나뿐이라
    // 우클릭 같은 취소 입구가 없고, WBP 는 건드리지 않기로 했다. 단, 슬롯이 이미
    // 비어 있으면(=EquippedInThisSlot 이 None) 이 카드 자체가 "지금 상태"이므로 뺀다.
    if (!EquippedInThisSlot.IsNone())
    {
        AddSkillIconCard(MakeEmptyCardData(Type));
    }

    for (const FHBJobSkillData& Skill : Skills)
    {
        if (Skill.SkillId == EquippedInThisSlot)
        {
            continue;
        }

        FSkillCardData CardData;

        if (!MakeCardData(Skill, Type, CardData))
        {
            continue;
        }

        AddSkillIconCard(CardData);
    }

    // 미리보기 중이던 스킬이 있으면(예: 장착 직후 서버 확정으로 다시 그려진 경우)
    // 상세정보 패널/장착 버튼/하이라이트를 최신 상태로 다시 맞춘다.
    if (bHasPreviewSelection)
    {
        RefreshPreviewDisplay();
    }
}

void UHB_SkillMainWidget::AddSkillIconCard(const FSkillCardData& InCardData)
{
    if (!SkillGridPanel || !SkillIconCardClass)
    {
        return;
    }

    UHB_SkillIconCardWidget* NewCard = CreateWidget<UHB_SkillIconCardWidget>(GetOwningPlayer(), SkillIconCardClass);

    if (!NewCard)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillMain | AddSkillIconCard 실패: CreateWidget(SkillIconCardClass) == nullptr"));
        return;
    }

    NewCard->InitIconCard(InCardData);
    NewCard->OnIconClickedDelegate.AddUniqueDynamic(this, &UHB_SkillMainWidget::OnSkillIconClicked);

    const int32 Columns = FMath::Max(1, GridColumnCount);
    const int32 Index = IconCardsBySkillId.Num();
    const int32 Row = Index / Columns;
    const int32 Column = Index % Columns;

    SkillGridPanel->AddChildToUniformGrid(NewCard, Row, Column);

    IconCardsBySkillId.Add(InCardData.SkillId, NewCard);
}

bool UHB_SkillMainWidget::MakeCardData(const FHBJobSkillData& InSkill, ESkillType InType, FSkillCardData& OutCard)
{
    // AbilityClass 가 없는 스킬은 서버 검증이 거절한다. 고를 수 없는 것을
    // 목록에 보여 주면 눌러도 아무 일이 안 일어나 원인을 못 찾는다.
    if (!InSkill.AbilityClass)
    {
        return false;
    }

    OutCard.SkillId = InSkill.SkillId;
    OutCard.SkillType = InType;
    OutCard.DisplayName = InSkill.DisplayName;
    // 설명의 {Attack%} 같은 치환자를 애셋의 실제 수치로 바꾼다.
    // 평문과 리치를 둘 다 채운다. 어느 쪽을 쓸지는 카드 위젯이 정한다.
    const FHBFormattedText Formatted = InSkill.FormatDescription();
    OutCard.Description = Formatted.Plain;
    OutCard.DescriptionRich = Formatted.Rich;
    OutCard.Icon = ResolveIcon(InSkill);
    OutCard.ManaCost = 0;
    OutCard.DisplayCooldown = 0.f;

    // 패시브에는 마나·쿨 개념이 없다. 카드에서도 숨긴다.
    if (InType != ESkillType::Active)
    {
        return true;
    }

    // 수치의 주인은 GA 애셋이다. DA 에는 쿨·마나 칸이 아예 없다.
    const UHBPlayerActiveGABase* AbilityCDO =
        Cast<UHBPlayerActiveGABase>(InSkill.AbilityClass->GetDefaultObject());

    if (!AbilityCDO)
    {
        return true;
    }

    // 증강이 반영되지 않은 애셋 기본값이다. 로비에는 증강이 없으므로 이게 맞다.
    OutCard.ManaCost = FMath::RoundToInt(AbilityCDO->GetBaseManaCost());
    OutCard.DisplayCooldown = AbilityCDO->GetBaseCooldown();
    return true;
}

FSkillCardData UHB_SkillMainWidget::MakeEmptyCardData(ESkillType InType) const
{
    FSkillCardData EmptyCard;
    EmptyCard.SkillId = NAME_None;
    EmptyCard.SkillType = InType;
    EmptyCard.DisplayName = FText::FromString(TEXT("없음"));
    EmptyCard.Description = FText::FromString(TEXT("이 슬롯을 비웁니다."));
    // 치환자도 특수문자도 없는 문장이라 리치도 같은 것을 쓴다.
    EmptyCard.DescriptionRich = EmptyCard.Description;
    EmptyCard.Icon = nullptr;
    EmptyCard.ManaCost = 0;
    EmptyCard.DisplayCooldown = 0.f;
    return EmptyCard;
}

UTexture2D* UHB_SkillMainWidget::ResolveIcon(const FHBJobSkillData& InSkill)
{
    if (InSkill.Icon.IsNull())
    {
        return nullptr;
    }

    const TObjectPtr<UTexture2D>* Cached = IconCache.Find(InSkill.SkillId);

    if (Cached)
    {
        return Cached->Get();
    }

    UTexture2D* Loaded = InSkill.Icon.LoadSynchronous();

    if (!Loaded)
    {
        return nullptr;
    }

    IconCache.Add(InSkill.SkillId, Loaded);
    return Loaded;
}

void UHB_SkillMainWidget::OnSkillIconClicked(FName ClickedSkillId)
{
    // 아직 서버엔 아무것도 안 보낸다. 미리보기만 갱신한다.
    CurrentPreviewSkillId = ClickedSkillId;
    bHasPreviewSelection = true;

    RefreshPreviewDisplay();
}

void UHB_SkillMainWidget::RefreshPreviewDisplay()
{
    UpdatePreviewSelectionHighlight();

    if (PreviewSkillCard)
    {
        FSkillCardData CardData;
        const ESkillType Type =
            (CurrentEditingSlotType == ESkillSlotType::Passive) ? ESkillType::Passive : ESkillType::Active;

        if (CurrentPreviewSkillId.IsNone())
        {
            CardData = MakeEmptyCardData(Type);
        }
        else
        {
            UHBJobData* Job = GetEditingJob();
            const FHBJobSkillData* Skill = Job ? Job->FindSkillAnywhere(CurrentPreviewSkillId) : nullptr;

            if (!Skill || !MakeCardData(*Skill, Type, CardData))
            {
                // 미리보던 스킬이 더는 유효하지 않다 (직업이 바뀌었거나 등). 미리보기를 접는다.
                ClearPreview();
                return;
            }
        }

        PreviewSkillCard->InitCard(CardData);
        PreviewSkillCard->SetHeaderText(NSLOCTEXT("SkillMain", "PreviewHeader", "선택한 스킬"));
        PreviewSkillCard->SetVisibility(ESlateVisibility::Visible);
    }

    RefreshEquipButtonState();
}

void UHB_SkillMainWidget::UpdatePreviewSelectionHighlight()
{
    for (const TPair<FName, TObjectPtr<UHB_SkillIconCardWidget>>& Pair : IconCardsBySkillId)
    {
        if (UHB_SkillIconCardWidget* Card = Pair.Value)
        {
            Card->SetPreviewSelected(bHasPreviewSelection && Pair.Key == CurrentPreviewSkillId);
        }
    }
}

void UHB_SkillMainWidget::RefreshEquipButtonState()
{
    if (!EquipButton)
    {
        return;
    }

    if (!bHasPreviewSelection || CurrentEditingSlotType == ESkillSlotType::None)
    {
        EquipButton->SetIsEnabled(false);
        return;
    }

    // 지금 이 슬롯에 이미 장착된 것과 같은 걸 미리보는 중이면 누를 이유가 없다.
    EquipButton->SetIsEnabled(CurrentPreviewSkillId != GetEquippedSkillIdForSlot(CurrentEditingSlotType));
}

void UHB_SkillMainWidget::ClearPreview()
{
    CurrentPreviewSkillId = NAME_None;
    bHasPreviewSelection = false;

    UpdatePreviewSelectionHighlight();

    if (PreviewSkillCard)
    {
        // Collapsed 가 아니라 Hidden 을 쓴다: Collapsed 는 레이아웃 공간까지 접어버려서
        // 상세정보 패널이 나타날 때 가로로 옆에 있는 SkillGridPanel 폭이 줄어드는
        // 현상이 생긴다. Hidden 은 안 보이기만 하고 자리는 그대로 차지해서
        // 그리드 폭이 처음부터 고정된다.
        PreviewSkillCard->SetVisibility(ESlateVisibility::Hidden);
    }
    if (EquipButton)
    {
        EquipButton->SetIsEnabled(false);
    }
}

void UHB_SkillMainWidget::OnEquipButtonClicked()
{
    if (!bHasPreviewSelection)
    {
        return;
    }

    AHBLobbyPlayerController* LobbyController = Cast<AHBLobbyPlayerController>(GetOwningPlayer());

    if (!LobbyController)
    {
        return;
    }

    FHBStartingLoadout NewLoadout = GetCurrentLoadout();
    const FName SkillId = CurrentPreviewSkillId;

    switch (CurrentEditingSlotType)
    {
    case ESkillSlotType::Active_Q:
        // 이미 E 에 있는 스킬을 Q 로 고르면 서로 맞바꾼다.
        // 그래야 Q == E 라는 무효 상태가 만들어지지 않는다.
        if (!SkillId.IsNone() && NewLoadout.SkillEId == SkillId)
        {
            NewLoadout.SkillEId = NewLoadout.SkillQId;
        }
        NewLoadout.SkillQId = SkillId;
        break;

    case ESkillSlotType::Active_E:
        if (!SkillId.IsNone() && NewLoadout.SkillQId == SkillId)
        {
            NewLoadout.SkillQId = NewLoadout.SkillEId;
        }
        NewLoadout.SkillEId = SkillId;
        break;

    case ESkillSlotType::Passive:
        NewLoadout.PassiveSkillId = SkillId;
        break;

    default:
        return;
    }

    // 로컬 값을 먼저 고치지 않는다. 서버가 확정하면 OnStartingLoadoutChanged 가
    // 돌아와 슬롯/그리드/미리보기를 갱신한다. 그래야 화면과 서버가 어긋날 수 없다.
    LobbyController->ApplySkillLoadout(NewLoadout);
}

void UHB_SkillMainWidget::RefreshSlotIcons()
{
    const FHBStartingLoadout Loadout = GetCurrentLoadout();

    RefreshSlotIcon(Slot_Q, Loadout.SkillQId);
    RefreshSlotIcon(Slot_E, Loadout.SkillEId);
    RefreshSlotIcon(Slot_Passive, Loadout.PassiveSkillId);
}

void UHB_SkillMainWidget::RefreshSlotIcon(UHB_SkillSlotWidget* InSlot, FName InSkillId)
{
    if (!InSlot)
    {
        return;
    }

    UHBJobData* Job = GetEditingJob();

    if (!Job)
    {
        InSlot->SetSkillIcon(nullptr);
        return;
    }
    if (InSkillId.IsNone())
    {
        InSlot->SetSkillIcon(nullptr);
        return;
    }

    // 액티브인지 패시브인지 몰라도 된다. DA 가 양쪽을 다 뒤져 준다.
    const FHBJobSkillData* Skill = Job->FindSkillAnywhere(InSkillId);

    if (!Skill)
    {
        InSlot->SetSkillIcon(nullptr);
        return;
    }

    InSlot->SetSkillIcon(ResolveIcon(*Skill));
}

void UHB_SkillMainWidget::RefreshEquippedCard()
{
    if (!EquippedSkillCard)
    {
        return;
    }

    // 슬롯을 아직 하나도 안 골랐으면 "지금 장착된 것"이라는 개념 자체가 없다.
    // Collapsed 가 아니라 Hidden - 그리드 옆 폭이 슬롯 선택 여부에 따라 흔들리지 않게.
    if (CurrentEditingSlotType == ESkillSlotType::None)
    {
        EquippedSkillCard->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    const FName EquippedSkillId = GetEquippedSkillIdForSlot(CurrentEditingSlotType);

    const ESkillType Type =
        (CurrentEditingSlotType == ESkillSlotType::Passive) ? ESkillType::Passive : ESkillType::Active;

    FSkillCardData CardData;
    UHBJobData* Job = GetEditingJob();
    const FHBJobSkillData* Skill =
        (Job && !EquippedSkillId.IsNone()) ? Job->FindSkillAnywhere(EquippedSkillId) : nullptr;

    if (!Skill || !MakeCardData(*Skill, Type, CardData))
    {
        CardData = MakeEmptyCardData(Type);
    }

    // 지금 실제로 장착된 걸 보여주는 카드다. "이미 선택됨" 흐림 처리를 할 이유가 없다.
    EquippedSkillCard->InitCard(CardData);
    EquippedSkillCard->SetHeaderText(NSLOCTEXT("SkillMain", "EquippedHeader", "장착 중인 스킬"));
    EquippedSkillCard->SetVisibility(ESlateVisibility::Visible);
}
