// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillMainWidget.h"
#include "UI/Skill/HB_SkillSlotWidget.h"
#include "UI/Skill/HB_SkillCardWidget.h"
#include "UI/Skill/HB_SkillIconCardWidget.h"
#include "UI/Skill/HB_SkillData.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/Job/HBJobData.h"
#include "Player/Job/HBJobAttributeEffects.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "Components/TextBlock.h"
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

    // UUniformGridSlot 은 칸별 Padding 을 지원하지 않는다 (Row/Column/정렬만 있음).
    // 대신 패널 전체에 적용되는 SlotPadding 을 한 번만 설정한다 - 모든 칸이 균일한
    // 간격을 가지므로 어차피 칸마다 다르게 줄 이유도 없다.
    if (ActiveSkillGrid)
    {
        ActiveSkillGrid->SetSlotPadding(CardPadding);
    }
    if (PassiveSkillGrid)
    {
        PassiveSkillGrid->SetSlotPadding(CardPadding);
    }

    // 처음에는 선택된 슬롯이 없다. 슬롯을 먼저 눌러야 목록의 스킬이 그 슬롯에 들어간다.
    SelectSlot(ESkillSlotType::None);

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
    RefreshJobStats();
    EnsureSkillLists();
    RefreshEquippedHighlights();
}

void UHB_SkillMainWidget::HandleStartingLoadoutChanged()
{
    // 서버가 확정한 뒤에 오는 신호다. 슬롯 아이콘과 목록의 장착 표시를 다시 맞춘다.
    // 저장 완료 문구는 따로 띄우지 않는다 (BP_OnLoadoutSaved 는 더 이상 부르지 않는다).
    RefreshFromPlayerState();
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

void UHB_SkillMainWidget::SelectSlot(ESkillSlotType NewSelection)
{
    SelectedSlot = NewSelection;

    // 선택한 슬롯의 표시만 켠다. (표시 연출은 슬롯 WBP 의 On Slot Selected Changed 가 맡는다)
    if (Slot_Q)
    {
        Slot_Q->SetSlotSelected(SelectedSlot == ESkillSlotType::Active_Q);
    }
    if (Slot_E)
    {
        Slot_E->SetSlotSelected(SelectedSlot == ESkillSlotType::Active_E);
    }
    if (Slot_Passive)
    {
        Slot_Passive->SetSlotSelected(SelectedSlot == ESkillSlotType::Passive);
    }

    // 슬롯을 골랐으면 그 슬롯에 맞는 목록만 선명하게 두고 반대쪽은 흐리게 한다.
    // 아무것도 고르지 않았으면 둘 다 그대로 둔다.
    const bool bPassiveSelected = SelectedSlot == ESkillSlotType::Passive;
    const bool bActiveSelected = SelectedSlot == ESkillSlotType::Active_Q || SelectedSlot == ESkillSlotType::Active_E;

    if (ActiveSkillGrid)
    {
        ActiveSkillGrid->SetRenderOpacity(bPassiveSelected ? InactiveListOpacity : 1.0f);
    }
    if (PassiveSkillGrid)
    {
        PassiveSkillGrid->SetRenderOpacity(bActiveSelected ? InactiveListOpacity : 1.0f);
    }
}

void UHB_SkillMainWidget::OnSkillSlotClicked(ESkillSlotType ClickedSlotType)
{
    // ClickedSlotType 이 항상 None 으로 찍히면, WBP_SkillMain 에 배치한
    // Slot_Q/Slot_E/Slot_Passive 인스턴스의 "Slot Type" 값을 디자이너에서
    // 안 정해준 것이다 (전부 기본값 None 인 상태).
    UE_LOG(LogTemp, Log, TEXT("SkillMain | 슬롯 클릭됨: %d"), static_cast<int32>(ClickedSlotType));

    if (ClickedSlotType == ESkillSlotType::None)
    {
        return;
    }

    // 이미 선택된 슬롯을 다시 누르면 선택이 풀린다.
    SelectSlot(SelectedSlot == ClickedSlotType ? ESkillSlotType::None : ClickedSlotType);
}
void UHB_SkillMainWidget::EnsureSkillLists()
{
    UHBJobData* Job = GetEditingJob();

    if (!Job)
    {
        // PlayerState.StartingLoadout.Job 이 비어있다는 뜻이다. 보통
        // BP_HBLobbyGameMode 의 SelectableJobs 가 비어 있을 때 이렇게 된다
        // (AHBLobbyGameMode::InitializeLobbyLoadout 참고 - 그쪽에도 별도 로그가 남는다).
        UE_LOG(LogTemp, Error, TEXT("SkillMain | 목록을 만들 수 없음: GetEditingJob() == nullptr (직업 미확정)"));
        return;
    }

    // 직업이 같고 이미 만들었다면 다시 만들지 않는다. 장착 표시만 바뀐다.
    if (ListedJob.Get() == Job && IconCardsBySkillId.Num() > 0)
    {
        return;
    }

    if (!ActiveSkillGrid || !PassiveSkillGrid)
    {
        // WBP_SkillMain 에 ActiveSkillGrid / PassiveSkillGrid 라는 이름의 UniformGridPanel 이
        // 없거나 이름이 다르면 BindWidget 이 실패해서 여기서 막힌다.
        UE_LOG(LogTemp, Error, TEXT("SkillMain | 목록을 만들 수 없음: ActiveSkillGrid / PassiveSkillGrid 바인딩 실패"));
        return;
    }
    if (!SkillIconCardClass)
    {
        // WBP_SkillMain 블루프린트의 클래스 디폴트(Details 패널)에서
        // "Skill Icon Card Class" 를 WBP_SkillIconCard 로 지정했는지 확인할 것.
        UE_LOG(LogTemp, Error, TEXT("SkillMain | 목록을 만들 수 없음: SkillIconCardClass 가 클래스 디폴트에 설정되지 않음"));
        return;
    }

    ActiveSkillGrid->ClearChildren();
    PassiveSkillGrid->ClearChildren();
    IconCardsBySkillId.Empty();

    // 목록의 출처는 직업 DA 다. 예전에는 SkillCatalog(DataTable)를 봤는데,
    // 그러면 직업과 무관한 스킬까지 섞여 나오고 값의 주인이 둘이 된다.
    int32 ActiveIndex = 0;
    for (const FHBJobSkillData& Skill : Job->GetActiveSkillCatalog())
    {
        FSkillCardData CardData;
        if (MakeCardData(Skill, ESkillType::Active, CardData))
        {
            AddSkillIconCard(ActiveSkillGrid, ActiveIndex++, CardData);
        }
    }

    int32 PassiveIndex = 0;
    for (const FHBJobSkillData& Skill : Job->GetPassiveSkillCatalog())
    {
        FSkillCardData CardData;
        if (MakeCardData(Skill, ESkillType::Passive, CardData))
        {
            AddSkillIconCard(PassiveSkillGrid, PassiveIndex++, CardData);
        }
    }

    ListedJob = Job;
}

void UHB_SkillMainWidget::AddSkillIconCard(UUniformGridPanel* Grid, int32 Index, const FSkillCardData& InCardData)
{
    if (!Grid || !SkillIconCardClass)
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

    // 이름, 설명, 마나, 쿨타임은 마우스를 올렸을 때 툴팁으로 보여 준다.
    // 툴팁을 아이콘 카드 자체에 달아서 카드 안의 어느 부분 위에 올려도 뜬다.
    if (SkillTooltipCardClass)
    {
        if (UHB_SkillCardWidget* Tooltip = CreateWidget<UHB_SkillCardWidget>(GetOwningPlayer(), SkillTooltipCardClass))
        {
            Tooltip->InitCard(InCardData);
            // 툴팁에는 "장착 중인 스킬" 같은 구분 글자가 필요 없다.
            Tooltip->SetHeaderText(FText::GetEmpty());
            NewCard->SetDetailTooltip(Tooltip);
        }
    }

    const int32 Columns = FMath::Max(1, GridColumnCount);
    const int32 Row = Index / Columns;
    const int32 Column = Index % Columns;

    Grid->AddChildToUniformGrid(NewCard, Row, Column);

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
    if (ClickedSkillId.IsNone())
    {
        return;
    }

    // 슬롯을 먼저 골라야 한다. 고르지 않았으면 어디에 넣을지 모르므로 아무것도 하지 않는다.
    if (SelectedSlot == ESkillSlotType::None)
    {
        return;
    }

    AHBLobbyPlayerController* LobbyController = Cast<AHBLobbyPlayerController>(GetOwningPlayer());
    UHBJobData* Job = GetEditingJob();

    if (!LobbyController || !Job)
    {
        return;
    }

    FHBStartingLoadout NewLoadout = GetCurrentLoadout();

    if (SelectedSlot == ESkillSlotType::Passive)
    {
        // 패시브 칸에는 패시브만 들어간다.
        if (!Job->FindPassiveSkill(ClickedSkillId))
        {
            return;
        }

        // 이미 이 칸에 있는 스킬을 다시 누르면 장착이 풀린다.
        NewLoadout.PassiveSkillId = (NewLoadout.PassiveSkillId == ClickedSkillId) ? NAME_None : ClickedSkillId;
    }
    else
    {
        // Q/E 칸에는 액티브만 들어간다.
        if (!Job->FindActiveSkill(ClickedSkillId))
        {
            return;
        }

        FName& Target = (SelectedSlot == ESkillSlotType::Active_Q) ? NewLoadout.SkillQId : NewLoadout.SkillEId;
        FName& Other = (SelectedSlot == ESkillSlotType::Active_Q) ? NewLoadout.SkillEId : NewLoadout.SkillQId;

        if (Target == ClickedSkillId)
        {
            // 이미 이 칸에 있는 스킬을 다시 누르면 장착이 풀린다.
            Target = NAME_None;
        }
        else
        {
            // 반대 칸에 있는 스킬을 고르면 서로 맞바꾼다. 그래야 Q == E 라는 무효 상태가 만들어지지 않는다.
            if (Other == ClickedSkillId)
            {
                Other = Target;
            }
            Target = ClickedSkillId;
        }
    }

    // 로컬 값을 먼저 고치지 않는다. 서버가 확정하면 OnStartingLoadoutChanged 가
    // 돌아와 슬롯/장착 표시를 갱신한다. 그래야 화면과 서버가 어긋날 수 없다.
    // 이 요청이 곧 저장이다 (장착 버튼은 없다).
    LobbyController->ApplySkillLoadout(NewLoadout);

    if (bDeselectSlotAfterEquip)
    {
        SelectSlot(ESkillSlotType::None);
    }
}
void UHB_SkillMainWidget::RefreshEquippedHighlights()
{
    const FHBStartingLoadout Loadout = GetCurrentLoadout();

    for (const TPair<FName, TObjectPtr<UHB_SkillIconCardWidget>>& Pair : IconCardsBySkillId)
    {
        if (UHB_SkillIconCardWidget* Card = Pair.Value)
        {
            const bool bEquipped = Pair.Key == Loadout.SkillQId
                || Pair.Key == Loadout.SkillEId
                || Pair.Key == Loadout.PassiveSkillId;
            Card->SetPreviewSelected(bEquipped);
        }
    }
}

void UHB_SkillMainWidget::RefreshJobStats()
{
    const UHBJobData* Job = GetEditingJob();

    // 소수점 한 자리까지만. 130.0 이 아니라 130 으로, 1.2 는 1.2 로 보인다.
    FNumberFormattingOptions Options;
    Options.MaximumFractionalDigits = 1;

    auto Fill = [Job, &Options](UTextBlock* Text, const FGameplayAttribute& Attribute)
    {
        if (!Text)
        {
            return;
        }

        float Value = 0.f;
        if (HBJobAttributes::GetJobBaseAttribute(Job, Attribute, Value))
        {
            Text->SetText(FText::AsNumber(Value, &Options));
        }
        else
        {
            // 직업을 아직 모르거나 그 능력치를 정한 GE 가 없다.
            Text->SetText(FText::FromString(TEXT("-")));
        }
    };

    Fill(Text_HP, UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute());
    Fill(Text_Stamina, UHB_PlayerAttributeSet::GetPlayer_MaxManaAttribute());
    Fill(Text_AttackPower, UHB_PlayerAttributeSet::GetPlayer_AttackPowerAttribute());
    Fill(Text_Defense, UHB_PlayerAttributeSet::GetPlayer_DefenseAttribute());
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

    // 비어 있는 슬롯에는 보여 줄 스킬 정보가 없으므로 툴팁도 없앤다.
    InSlot->SetToolTip(nullptr);

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

    // 장착 중인 스킬도 마우스를 올리면 목록의 아이콘과 같은 정보(이름, 설명, 마나, 쿨타임)를 보여 준다.
    if (SkillTooltipCardClass)
    {
        const ESkillType Type = Job->FindActiveSkill(InSkillId) ? ESkillType::Active : ESkillType::Passive;

        FSkillCardData CardData;
        if (MakeCardData(*Skill, Type, CardData))
        {
            if (UHB_SkillCardWidget* Tooltip = CreateWidget<UHB_SkillCardWidget>(GetOwningPlayer(), SkillTooltipCardClass))
            {
                Tooltip->InitCard(CardData);
                Tooltip->SetHeaderText(FText::GetEmpty());
                InSlot->SetToolTip(Tooltip);
            }
        }
    }
}
