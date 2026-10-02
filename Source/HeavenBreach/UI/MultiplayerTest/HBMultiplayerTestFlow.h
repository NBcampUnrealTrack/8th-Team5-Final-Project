#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"
#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"
#include "HBMultiplayerTestFlow.generated.h"
class UHBMultiplayerTestWidget;

UCLASS()
class HEAVENBREACH_API AHBMultiplayerTestMenuController : public APlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY() TObjectPtr<UHBMultiplayerTestWidget> Screen;
};

UCLASS()
class HEAVENBREACH_API AHBMultiplayerTestLobbyController : public AHBLobbyPlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY() TObjectPtr<UHBMultiplayerTestWidget> Screen;
};

UCLASS()
class HEAVENBREACH_API AHBMultiplayerTestMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AHBMultiplayerTestMenuGameMode();
};

UCLASS()
class HEAVENBREACH_API AHBMultiplayerTestLobbyGameMode : public AHBLobbyGameMode
{
    GENERATED_BODY()
public:
    AHBMultiplayerTestLobbyGameMode();
};
