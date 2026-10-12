#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HBStartupGameMode.generated.h"

class UUserWidget;

/** Presentation-only startup: the existing splash, then a loading screen held until the lobby is ready. */
UCLASS()
class HEAVENBREACH_API AHBStartupGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHBStartupGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category="Startup")
	TSubclassOf<UUserWidget> SplashWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Startup")
	TSoftObjectPtr<UWorld> LobbyMap;

	UPROPERTY(EditDefaultsOnly, Category="Startup", meta=(ClampMin="0.0"))
	float SplashDuration = 2.7f;

private:
	void OpenLobby();
	FTimerHandle SplashTimer;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SplashWidget;
};
