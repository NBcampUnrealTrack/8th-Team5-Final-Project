#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "HBMultiplayerTestWidget.generated.h"
class UVerticalBox;
class UTextBlock;
class UEditableTextBox;
class UCheckBox;
class UBorder;
class UHBOnlineSessionSubsystem;

UCLASS()
class HEAVENBREACH_API UHBMultiplayerTestButton : public UButton
{
    GENERATED_BODY()
public:
    void BindAction(TFunction<void()> InAction);
private:
    TFunction<void()> Action;
    UFUNCTION() void Click();
};

/** Plain functional UMG screen shared by the isolated menu and listen lobby. */
UCLASS()
class HEAVENBREACH_API UHBMultiplayerTestWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
    enum class EPanel { Menu, Create, Find, Lobby, Friends };
    EPanel Panel = EPanel::Menu;
    float RefreshTime = 0;
    FString LastList;
    FString LastError;
    FString PendingRoom;
    bool bInviteJoin = false;
    bool bModal = false;
    UPROPERTY() TObjectPtr<UVerticalBox> Body;
    UPROPERTY() TObjectPtr<UVerticalBox> Rows;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UTextBlock> ModalText;
    UPROPERTY() TObjectPtr<UTextBlock> LobbyPlayers;
    UPROPERTY() TObjectPtr<UEditableTextBox> RoomName;
    UPROPERTY() TObjectPtr<UEditableTextBox> RoomPassword;
    UPROPERTY() TObjectPtr<UEditableTextBox> JoinPassword;
    UPROPERTY() TObjectPtr<UCheckBox> UsePassword;
    UPROPERTY() TObjectPtr<UBorder> Modal;
    UPROPERTY() TObjectPtr<UHBMultiplayerTestButton> ModalJoin;
    UPROPERTY() TObjectPtr<UHBMultiplayerTestButton> Create;
    UPROPERTY() TObjectPtr<UHBMultiplayerTestButton> Ready;
    UPROPERTY() TObjectPtr<UHBMultiplayerTestButton> Start;
    UHBOnlineSessionSubsystem* Sessions() const;
    void ShowPanel(EPanel InPanel);
    void Refresh();
    void ShowMessage(const FString& Message);
    void PromptJoin(const FString& SessionId, bool bInvite, const FString& Name);
    void SubmitJoin();
    UTextBlock* AddText(UVerticalBox* Parent, const FString& Text, FName Name = NAME_None);
    UHBMultiplayerTestButton* AddButton(UVerticalBox* Parent, const FString& Text, FName Name, TFunction<void()> Action);
    UEditableTextBox* AddInput(UVerticalBox* Parent, FName Name, bool bPassword = false);
};
