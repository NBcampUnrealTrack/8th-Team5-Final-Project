#include "UI/MultiplayerTest/HBMultiplayerTestFlow.h"
#include "UI/MultiplayerTest/HBMultiplayerTestWidget.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Player/Job/HBJobData.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

namespace
{
    void ShowScreen(APlayerController* PC, UHBMultiplayerTestWidget*& OutScreen)
    {
        auto* Sessions = PC->GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>();
        Sessions->SetLobbyReturnMap(TEXT("/Game/Levels/Test_CW/Multiplayer/L_MultiplayerTestMenu"));
        OutScreen = CreateWidget<UHBMultiplayerTestWidget>(PC, UHBMultiplayerTestWidget::StaticClass());
        if (!OutScreen) return;
        OutScreen->AddToViewport();
        PC->SetInputMode(FInputModeUIOnly());
        PC->SetShowMouseCursor(true);
    }
    void ReleaseScreen(APlayerController* PC, UHBMultiplayerTestWidget* Screen)
    {
        if (Screen) Screen->RemoveFromParent();
        if (PC->IsLocalController() && PC->GetWorld())
            if (auto* Viewport = PC->GetWorld()->GetGameViewport()) Viewport->SetIgnoreInput(false);
    }
}

AHBMultiplayerTestMenuGameMode::AHBMultiplayerTestMenuGameMode()
{
    PlayerControllerClass = AHBMultiplayerTestMenuController::StaticClass();
    DefaultPawnClass = nullptr;
}

AHBMultiplayerTestLobbyGameMode::AHBMultiplayerTestLobbyGameMode()
{
    PlayerControllerClass = AHBMultiplayerTestLobbyController::StaticClass();
    GameplayMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Levels/Test_CW/GameplaySmoke/Test_MGGamePlay.Test_MGGamePlay")));
    bUseConfiguredGameplayMap = false;
    RequiredReadyPlayerCount = 2;
    static ConstructorHelpers::FObjectFinder<UHBJobData> Job(TEXT("/Game/Characters/Warrior/DA_Job_Warrior.DA_Job_Warrior"));
    if (Job.Succeeded()) SelectableJobs.Add(Job.Object);
}

void AHBMultiplayerTestMenuController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController())
    {
        UHBMultiplayerTestWidget* Widget = nullptr;
        ShowScreen(this, Widget);
        Screen = Widget;
        GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>()->PublishGamePresence(TEXT("Menu"));
    }
}
void AHBMultiplayerTestMenuController::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseScreen(this, Screen);
    Screen = nullptr;
    Super::EndPlay(Reason);
}
void AHBMultiplayerTestLobbyController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController())
    {
        UHBMultiplayerTestWidget* Widget = nullptr;
        ShowScreen(this, Widget);
        Screen = Widget;
    }
}
void AHBMultiplayerTestLobbyController::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseScreen(this, Screen);
    Screen = nullptr;
    Super::EndPlay(Reason);
}
