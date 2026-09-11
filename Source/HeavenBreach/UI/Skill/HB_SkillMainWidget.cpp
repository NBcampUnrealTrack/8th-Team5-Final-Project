// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Skill/HB_SkillMainWidget.h"
#include "UI/Skill/HB_SkillSlotWidget.h"
#include "UI/Skill/HB_SkillCardWidget.h"
#include "UI/Skill/HB_SkillData.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/Job/HBJobData.h"
#include "Combat/Ability/HBPlayerActiveGABase.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
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

    // 목록이 열려 있으면 선택 표시도 같이 갱신한다.
    if (!SkillListScrollBox)
    {
        return;
    }
    if (SkillListScrollBox->GetChildrenCount() == 0)
    {
        return;
    }

    PopulateSkillList(CurrentEditingSlotType == ESkillSlotType::Passive);
}

void UHB_SkillMainWidget::HandleStartingLoadoutChanged()
{
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

bool UHB_SkillMainWidget::IsSkillAlreadySelected(FName SkillId) const
{
    // 빈 슬롯끼리 맞아떨어져 "없음" 카드가 선택된 것처럼 보이는 것을 막는다.
    if (SkillId.IsNone())
    {
        return false;
    }

    const FHBStartingLoadout Loadout = GetCurrentLoadout();

    if (SkillId == Loadout.SkillQId)
    {
        return true;
    }
    if (SkillId == Loadout.SkillEId)
    {
        return true;
    }
    return SkillId == Loadout.PassiveSkillId;
}

void UHB_SkillMainWidget::OnSkillSlotClicked(ESkillSlotType ClickedSlotType)
{
    CurrentEditingSlotType = ClickedSlotType;

    PopulateSkillList(ClickedSlotType == ESkillSlotType::Passive);
}

void UHB_SkillMainWidget::PopulateSkillList(bool bIsPassive)
{
    if (!SkillListScrollBox)
    {
        return;
    }

    SkillListScrollBox->ClearChildren();

    // 목록의 출처는 직업 DA 다. 예전에는 SkillCatalog(DataTable)를 봤는데,
    // 그러면 직업과 무관한 스킬까지 섞여 나오고 값의 주인이 둘이 된다.
    UHBJobData* Job = GetEditingJob();

    if (!Job)
    {
        return;
    }
    if (!SkillCardClass)
    {
        return;
    }

    ESkillType Type = ESkillType::Active;
    TArray<FHBJobSkillData> Skills = Job->GetActiveSkillCatalog();

    if (bIsPassive)
    {
        Type = ESkillType::Passive;
        Skills = Job->GetPassiveSkillCatalog();
    }

    // 슬롯을 비우는 유일한 방법이다. 슬롯 위젯에는 버튼이 하나뿐이라 우클릭
    // 같은 취소 입구가 없고, WBP 는 건드리지 않기로 했다.
    //
    // 쳐낼 조건: UI 담당자가 슬롯에 비우기 버튼을 달면 이 카드는 지운다.
    FSkillCardData EmptyCard;
    EmptyCard.SkillId = NAME_None;
    EmptyCard.SkillType = Type;
    EmptyCard.DisplayName = FText::FromString(TEXT("없음"));
    EmptyCard.Description = FText::FromString(TEXT("이 슬롯을 비웁니다."));
    EmptyCard.Icon = nullptr;
    EmptyCard.ManaCost = 0;
    EmptyCard.DisplayCooldown = 0.f;
    AddSkillCard(EmptyCard);

    for (const FHBJobSkillData& Skill : Skills)
    {
        FSkillCardData CardData;

        if (!MakeCardData(Skill, Type, CardData))
        {
            continue;
        }

        AddSkillCard(CardData);
    }
}

void UHB_SkillMainWidget::AddSkillCard(const FSkillCardData& InCardData)
{
    UHB_SkillCardWidget* NewCard = CreateWidget<UHB_SkillCardWidget>(GetOwningPlayer(), SkillCardClass);

    if (!NewCard)
    {
        return;
    }

    NewCard->InitCard(InCardData, IsSkillAlreadySelected(InCardData.SkillId));
    NewCard->OnCardClickedDelegate.AddUniqueDynamic(this, &UHB_SkillMainWidget::OnSkillCardClicked);

    UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(SkillListScrollBox->AddChild(NewCard));

    if (!ScrollSlot)
    {
        return;
    }

    ScrollSlot->SetPadding(CardPadding);
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
    OutCard.Description = InSkill.Description;
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

void UHB_SkillMainWidget::OnSkillCardClicked(FName ClickedSkillId)
{
    AHBLobbyPlayerController* LobbyController = Cast<AHBLobbyPlayerController>(GetOwningPlayer());

    if (!LobbyController)
    {
        return;
    }

    FHBStartingLoadout NewLoadout = GetCurrentLoadout();

    switch (CurrentEditingSlotType)
    {
    case ESkillSlotType::Active_Q:
        // 이미 E 에 있는 스킬을 Q 로 고르면 서로 맞바꾼다.
        // 그래야 Q == E 라는 무효 상태가 만들어지지 않는다.
        if (!ClickedSkillId.IsNone() && NewLoadout.SkillEId == ClickedSkillId)
        {
            NewLoadout.SkillEId = NewLoadout.SkillQId;
        }
        NewLoadout.SkillQId = ClickedSkillId;
        break;

    case ESkillSlotType::Active_E:
        if (!ClickedSkillId.IsNone() && NewLoadout.SkillQId == ClickedSkillId)
        {
            NewLoadout.SkillQId = NewLoadout.SkillEId;
        }
        NewLoadout.SkillEId = ClickedSkillId;
        break;

    case ESkillSlotType::Passive:
        NewLoadout.PassiveSkillId = ClickedSkillId;
        break;

    default:
        return;
    }

    // 로컬 값을 먼저 고치지 않는다. 서버가 확정하면 OnStartingLoadoutChanged 가
    // 돌아와 슬롯을 갱신한다. 그래야 화면과 서버가 어긋날 수 없다.
    LobbyController->ApplySkillLoadout(NewLoadout);

    if (SkillListScrollBox)
    {
        SkillListScrollBox->ClearChildren();
    }
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
