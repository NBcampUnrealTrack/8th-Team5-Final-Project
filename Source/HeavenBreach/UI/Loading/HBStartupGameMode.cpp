#include "UI/Loading/HBStartupGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/Loading/HBLobbyLoadingSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AHBStartupGameMode::AHBStartupGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Levels/Lobby/L_Lobby.L_Lobby")));
	static ConstructorHelpers::FClassFinder<UUserWidget> Splash(TEXT("/Game/UI/Startup/WBP_StudioSplash"));
	if (Splash.Succeeded()) SplashWidgetClass = Splash.Class;
}

void AHBStartupGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer) return;
	if (SplashWidgetClass)
	{
		SplashWidget = CreateWidget<UUserWidget>(GetGameInstance(), SplashWidgetClass);
		if (SplashWidget) SplashWidget->AddToViewport(100);
	}
	if (SplashDuration > 0.0f)
		GetWorldTimerManager().SetTimer(SplashTimer, this, &ThisClass::OpenLobby, SplashDuration, false);
	else OpenLobby();
}

void AHBStartupGameMode::OpenLobby()
{
	if (auto* Loading = GetGameInstance()->GetSubsystem<UHBLobbyLoadingSubsystem>())
	{
		if (Loading->TravelFromStartup(LobbyMap))
		{
			if (SplashWidget) SplashWidget->RemoveFromParent();
			SplashWidget = nullptr;
		}
	}
}

void AHBStartupGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SplashTimer);
	if (SplashWidget) SplashWidget->RemoveFromParent();
	SplashWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}
