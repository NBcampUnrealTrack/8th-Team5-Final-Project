// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "HeavenBreachGameMode.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AHeavenBreachGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AHeavenBreachGameMode();

	// 매치 시작
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match")
	void StartExtractionMatch();

	// 매치 종료 
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match")
	void FinishExtractionMatch();

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetConnectedPlayerCount();

protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	virtual bool ReadyToStartMatch_Implementation() override;
	virtual bool ReadyToEndMatch_Implementation() override;

	virtual void HandleMatchIsWaitingToStart() override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void HandleLeavingMap() override;
	virtual void OnMatchStateSet() override;

	// 테스트용, 시작 인원 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MinimumPlayersToStart = 2;

	// 최소 인원 접속 시 자동 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	bool bAutoStartMatch = true;
};
