#include "UI/MultiplayerTest/HBMultiplayerTestWidget.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "Core/GameFlow/Lobby/HBLobbyGameState.h"
#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UHBMultiplayerTestButton::BindAction(TFunction<void()> InAction)
{
    Action = MoveTemp(InAction);
    OnClicked.AddUniqueDynamic(this, &ThisClass::Click);
}
void UHBMultiplayerTestButton::Click() { if (Action) Action(); }

UHBOnlineSessionSubsystem* UHBMultiplayerTestWidget::Sessions() const
{ return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>() : nullptr; }

UTextBlock* UHBMultiplayerTestWidget::AddText(UVerticalBox* Parent, const FString& Text, FName Name)
{
    auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Label->SetText(FText::FromString(Text));
    Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Label->SetAutoWrapText(true);
    Parent->AddChildToVerticalBox(Label)->SetPadding(FMargin(5));
    return Label;
}
UHBMultiplayerTestButton* UHBMultiplayerTestWidget::AddButton(UVerticalBox* Parent, const FString& Text, FName Name, TFunction<void()> Action)
{
    auto* Button = WidgetTree->ConstructWidget<UHBMultiplayerTestButton>(UHBMultiplayerTestButton::StaticClass(), Name);
    auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Text));
    Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
    Button->AddChild(Label);
    Button->BindAction(MoveTemp(Action));
    Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(5));
    return Button;
}
UEditableTextBox* UHBMultiplayerTestWidget::AddInput(UVerticalBox* Parent, FName Name, bool bPassword)
{
    auto* Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), Name);
    Input->SetIsPassword(bPassword);
    Parent->AddChildToVerticalBox(Input)->SetPadding(FMargin(5));
    return Input;
}

TSharedRef<SWidget> UHBMultiplayerTestWidget::RebuildWidget()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    if (!WidgetTree->RootWidget)
    {
        auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
        WidgetTree->RootWidget = Canvas;
        auto* Frame = WidgetTree->ConstructWidget<UBorder>();
        Frame->SetBrushColor(FLinearColor(.04f, .04f, .04f, 1));
        Frame->SetPadding(FMargin(20));
        auto* Position = Canvas->AddChildToCanvas(Frame);
        Position->SetAnchors(FAnchors(.5f, .5f));
        Position->SetAlignment(FVector2D(.5f, .5f));
        Position->SetAutoSize(true);
        auto* Size = WidgetTree->ConstructWidget<USizeBox>();
        Size->SetWidthOverride(780);
        Frame->SetContent(Size);
        auto* Root = WidgetTree->ConstructWidget<UVerticalBox>();
        Size->SetContent(Root);
        AddText(Root, TEXT("멀티플레이 테스트"));
        Status = AddText(Root, FString(), TEXT("StatusText"));
        Body = WidgetTree->ConstructWidget<UVerticalBox>();
        auto* BodySize = WidgetTree->ConstructWidget<USizeBox>();
        BodySize->SetHeightOverride(420);
        Root->AddChild(BodySize);
        auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
        BodySize->SetContent(Scroll);
        Scroll->AddChild(Body);
        Modal = WidgetTree->ConstructWidget<UBorder>();
        Modal->SetBrushColor(FLinearColor(.15f, .15f, .15f, 1));
        Root->AddChild(Modal);
        auto* Dialog = WidgetTree->ConstructWidget<UVerticalBox>();
        Modal->SetContent(Dialog);
        ModalText = AddText(Dialog, FString(), TEXT("PopupText"));
        JoinPassword = AddInput(Dialog, TEXT("JoinPasswordInput"), true);
        ModalJoin = AddButton(Dialog, TEXT("참가"), TEXT("PasswordJoinButton"), [this] { SubmitJoin(); });
        AddButton(Dialog, TEXT("확인 / 취소"), TEXT("PopupCloseButton"), [this]
        {
            if (bInviteJoin && Sessions()) Sessions()->DeclinePendingInvite();
            bModal = false; PendingRoom.Reset(); bInviteJoin = false;
            JoinPassword->SetText(FText::GetEmpty());
            Modal->SetVisibility(ESlateVisibility::Collapsed);
            Body->SetIsEnabled(true);
        });
        Modal->SetVisibility(ESlateVisibility::Collapsed);
    }
    return Super::RebuildWidget();
}

void UHBMultiplayerTestWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ShowPanel(Cast<AHBLobbyPlayerController>(GetOwningPlayer()) ? EPanel::Lobby : EPanel::Menu);
    if (auto* Service = Sessions())
    {
        const FString Notice = Service->ConsumeMenuNotice();
        if (!Notice.IsEmpty()) ShowMessage(Notice);
    }
    Refresh();
}
void UHBMultiplayerTestWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    RefreshTime += DeltaSeconds;
    if (RefreshTime >= .2f) { RefreshTime = 0; Refresh(); }
}

void UHBMultiplayerTestWidget::ShowPanel(EPanel InPanel)
{
    Panel = InPanel;
    Body->ClearChildren();
    Create = Start = nullptr;
    Rows = nullptr; LobbyPlayers = nullptr;
    LastList.Reset();
    auto* Service = Sessions();
    if (Panel == EPanel::Menu)
    {
        AddButton(Body, TEXT("방 생성"), TEXT("OpenCreateButton"), [this] { ShowPanel(EPanel::Create); });
        AddButton(Body, TEXT("방 찾기"), TEXT("OpenFindButton"), [this] { ShowPanel(EPanel::Find); Sessions()->FindLobbies(); });
    }
    else if (Panel == EPanel::Create)
    {
        AddText(Body, TEXT("방 이름 (최대 48자)"));
        RoomName = AddInput(Body, TEXT("RoomNameInput"));
        RoomName->SetText(FText::FromString(TEXT("테스트 방")));
        UsePassword = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("UsePasswordCheck"));
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(TEXT("비밀번호 사용")));
        UsePassword->SetContent(Label); Body->AddChild(UsePassword);
        RoomPassword = AddInput(Body, TEXT("RoomPasswordInput"), true);
        Create = AddButton(Body, TEXT("방 생성"), TEXT("CreateRoomButton"), [this]
        {
            const FString Password = UsePassword->IsChecked() ? RoomPassword->GetText().ToString() : FString();
            Sessions()->RequestHostRoom(RoomName->GetText().ToString(), Password,
                TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Levels/Test_CW/Multiplayer/L_MultiplayerTestLobby.L_MultiplayerTestLobby"))));
            RoomPassword->SetText(FText::GetEmpty());
        });
        AddButton(Body, TEXT("취소"), TEXT("CancelCreateButton"), [this] { ShowPanel(EPanel::Menu); });
    }
    else if (Panel == EPanel::Find)
    {
        AddButton(Body, TEXT("새로고침"), TEXT("RefreshRoomsButton"), [this] { Sessions()->FindLobbies(); });
        Rows = WidgetTree->ConstructWidget<UVerticalBox>(); Body->AddChild(Rows);
        AddButton(Body, TEXT("뒤로가기"), TEXT("FindBackButton"), [this] { ShowPanel(EPanel::Menu); });
    }
    else if (Panel == EPanel::Lobby)
    {
        LobbyPlayers = AddText(Body, FString(), TEXT("PlayerSlotsText"));
        Start = AddButton(Body, TEXT("게임 시작"), TEXT("StartMatchButton"), [this]
        { if (auto* PC = Cast<AHBLobbyPlayerController>(GetOwningPlayer())) PC->RequestStartGame(); });
        AddButton(Body, TEXT("친구 초대"), TEXT("OpenFriendsButton"), [this] { ShowPanel(EPanel::Friends); Sessions()->RefreshSteamFriends(); });
        AddButton(Body, TEXT("방 나가기"), TEXT("LeaveRoomButton"), [this] { Sessions()->RequestLeaveToMenu(); });
    }
    else if (Panel == EPanel::Friends)
    {
        AddText(Body, TEXT("HeavenBreach 접속 중인 Steam 친구"));
        AddButton(Body, TEXT("새로고침"), TEXT("RefreshFriendsButton"), [this] { Sessions()->RefreshSteamFriends(); });
        Rows = WidgetTree->ConstructWidget<UVerticalBox>(); Body->AddChild(Rows);
        AddButton(Body, TEXT("로비로 돌아가기"), TEXT("FriendsBackButton"), [this] { ShowPanel(EPanel::Lobby); });
    }
    Body->SetIsEnabled(Service && !Service->IsLobbyOperationInProgress() && !bModal);
    Refresh();
}

void UHBMultiplayerTestWidget::ShowMessage(const FString& Message)
{
    bModal = true; bInviteJoin = false; PendingRoom.Reset();
    FString Display = Message;
    if (Display.Contains(TEXT("HB_PASSWORD_INVALID"))) Display = TEXT("비밀번호가 올바르지 않습니다.");
    else if (Display.Contains(TEXT("HB_PASSWORD_REQUIRED"))) Display = TEXT("비밀번호를 입력해주세요.");
    else if (Display.Contains(TEXT("HB_ROOM_FULL")) || Display.Contains(TEXT("SessionIsFull"))) Display = TEXT("방이 가득 찼습니다.");
    else if (Display.Contains(TEXT("SessionDoesNotExist"))) Display = TEXT("방이 존재하지 않습니다.");
    else if (Display.Contains(TEXT("Match admission is closed"))) Display = TEXT("게임이 시작되어 참가할 수 없습니다.");
    ModalText->SetText(FText::FromString(Display));
    JoinPassword->SetVisibility(ESlateVisibility::Collapsed);
    ModalJoin->SetVisibility(ESlateVisibility::Collapsed);
    Modal->SetVisibility(ESlateVisibility::Visible);
    Body->SetIsEnabled(false);
}
void UHBMultiplayerTestWidget::PromptJoin(const FString& SessionId, bool bInvite, const FString& Name)
{
    ShowMessage(Name + TEXT("\n비밀번호를 입력해주세요."));
    PendingRoom = SessionId; bInviteJoin = bInvite;
    JoinPassword->SetText(FText::GetEmpty());
    JoinPassword->SetVisibility(ESlateVisibility::Visible);
    ModalJoin->SetVisibility(ESlateVisibility::Visible);
}
void UHBMultiplayerTestWidget::SubmitJoin()
{
    const FString Password = JoinPassword->GetText().ToString();
    const bool bAccepted = bInviteJoin ? Sessions()->AcceptPendingInvite(Password) : Sessions()->RequestJoinRoom(PendingRoom, Password);
    JoinPassword->SetText(FText::GetEmpty());
    if (bAccepted) { bModal = false; Modal->SetVisibility(ESlateVisibility::Collapsed); PendingRoom.Reset(); bInviteJoin = false; }
}

void UHBMultiplayerTestWidget::Refresh()
{
    auto* Service = Sessions();
    if (!Service || !Body) return;
    const bool bBusy = Service->IsLobbyOperationInProgress();
    Body->SetIsEnabled(!bBusy && !bModal);
    Status->SetText(FText::FromString(bBusy ? TEXT("처리 중입니다...") : Service->IsReadingFriends() ? TEXT("친구 목록을 불러오는 중...") : TEXT("최대 4명 / 혼자 시작 가능 / 호스트가 게임 시작")));
    FString Error = Service->GetLastLobbyOperationError();
    if (Error != LastError)
    {
        LastError = Error;
        if (!Error.IsEmpty())
        {
            if (Error.Contains(TEXT("HB_PASSWORD_INVALID"))) Error = TEXT("비밀번호가 올바르지 않습니다.");
            else if (Error.Contains(TEXT("HB_PASSWORD_REQUIRED"))) Error = TEXT("비밀번호를 입력해주세요.");
            else if (Error.Contains(TEXT("HB_ROOM_FULL")) || Error.Contains(TEXT("SessionIsFull"))) Error = TEXT("방이 가득 찼습니다.");
            else if (Error.Contains(TEXT("SessionDoesNotExist"))) Error = TEXT("방이 존재하지 않습니다.");
            else if (Error.Contains(TEXT("requires Steam")) || Error.Contains(TEXT("logged into Steam")) || Error.Contains(TEXT("initialized Steam"))) Error = TEXT("Steam에 로그인한 뒤 다시 시도해주세요.");
            ShowMessage(Error);
        }
    }
    FHBLobbySearchEntry Invite;
    if (!bBusy && !bModal && Service->GetPendingInvite(Invite))
    {
        if (GetWorld()->GetNetMode() != NM_Standalone) Service->RequestLeaveToMenu();
        else if (Invite.bPasswordProtected) PromptJoin(Invite.SessionId, true, Invite.RoomName);
        else Service->AcceptPendingInvite(FString());
    }
    if (Create)
    {
        RoomPassword->SetIsEnabled(UsePassword->IsChecked());
        Create->SetIsEnabled(!RoomName->GetText().ToString().TrimStartAndEnd().IsEmpty()
            && (!UsePassword->IsChecked() || !RoomPassword->GetText().IsEmpty()));
    }
    if (Panel == EPanel::Lobby && LobbyPlayers)
    {
        const auto* State = GetWorld()->GetGameState<AHBLobbyGameState>();
        const auto* Local = GetOwningPlayerState<AHBLobbyPlayerState>();
        const bool bHost = State && Local && State->HostPlayerState == Local;
        FString Text = State ? State->RoomName + TEXT("\n") : FString();
        Text += TEXT("Host: ") + (State && State->HostPlayerState ? State->HostPlayerState->GetPlayerName() : TEXT("Empty"));
        TArray<const AHBLobbyPlayerState*> Guests;
        if (State) for (APlayerState* Player : State->PlayerArray)
            if (const auto* Guest = Cast<AHBLobbyPlayerState>(Player); Guest && Guest != State->HostPlayerState) Guests.Add(Guest);
        for (int32 Index = 0; Index < 3; ++Index)
            Text += FString::Printf(TEXT("\nGuest %d: %s"), Index + 1, Guests.IsValidIndex(Index)
                ? *Guests[Index]->GetPlayerName() : TEXT("Empty"));
        LobbyPlayers->SetText(FText::FromString(Text));
        Start->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Start->SetIsEnabled(bHost && State->CanStartMatch());
    }
    if (Rows && Panel == EPanel::Find)
    {
        const auto Entries = Service->GetLobbySearchResults();
        FString Signature;
        for (const auto& Entry : Entries) Signature += Entry.SessionId + FString::FromInt(Entry.OpenSlots) + Entry.RoomName;
        if (Signature != LastList || Rows->GetChildrenCount() == 0)
        {
            LastList = Signature; Rows->ClearChildren();
            if (Entries.IsEmpty()) AddText(Rows, TEXT("참가 가능한 방이 없습니다."));
            for (const auto& Entry : Entries)
                AddButton(Rows, FString::Printf(TEXT("%s | %d/%d | %s | 참가"), *Entry.RoomName, Entry.CurrentPlayers, Entry.MaxPlayers,
                    Entry.bPasswordProtected ? TEXT("비밀번호 있음") : TEXT("공개")), NAME_None, [this, Entry]
                {
                    if (Entry.bPasswordProtected) PromptJoin(Entry.SessionId, false, Entry.RoomName);
                    else Sessions()->RequestJoinRoom(Entry.SessionId, FString());
                });
        }
    }
    if (Rows && Panel == EPanel::Friends)
    {
        const auto Friends = Service->GetSteamFriends();
        FString Signature;
        for (const auto& Friend : Friends) Signature += Friend.UserId + (Friend.bCanInvite ? TEXT("1") : TEXT("0"));
        if (Signature != LastList || Rows->GetChildrenCount() == 0)
        {
            LastList = Signature; Rows->ClearChildren();
            if (Friends.IsEmpty()) AddText(Rows, TEXT("초대할 수 있는 Steam 친구가 없습니다."));
            for (const auto& Friend : Friends)
            {
                auto* Button = AddButton(Rows, Friend.DisplayName + (Friend.bCanInvite ? TEXT(" / 초대") : TEXT(" / 게임 중 / 초대 불가")), NAME_None,
                    [this, Friend] { ShowMessage(Sessions()->InviteSteamFriend(Friend.UserId) ? TEXT("초대를 보냈습니다.") : TEXT("초대를 보내지 못했습니다. 목록을 새로고침해주세요.")); });
                Button->SetIsEnabled(Friend.bCanInvite);
            }
        }
    }
}
