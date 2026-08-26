// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "HeavenBreachGameMode.generated.h"

class APlayerState;

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
	
	// 전투 시스템에서 사망 시 호출 
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match")
	void NotifyPlayerDied(APlayerState* PlayerState);
	
	// 탈출시 호출 
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match")
	void NotifyPlayerExtracted(APlayerState* PlayerState);

protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	virtual bool ReadyToStartMatch_Implementation() override;
	virtual bool ReadyToEndMatch_Implementation() override;
	
	void TryStartExtractionMatch();

	virtual void HandleMatchIsWaitingToStart() override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void HandleLeavingMap() override;
	virtual void OnMatchStateSet() override;
	
	// 현재 매치 참가자 초기화
	void InitializeActivePlayers();
	
	// 매치 종료 조건 검사
	void EvaluateMatchEnd();
	
	// Match Timer
	void StartMatchTimer();
	void StopMatchTimer();
	void HandleMatchTimeExpired();
	
	UPROPERTY(Transient)
	TSet<TObjectPtr<APlayerState>> ActivePlayers;
	
	int32 ExpectedPlayerCount = 2;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match | Timer", meta = (ClamMin = "1.0"))
	float MatchDurationSeconds = 1500.0f;
	
	FTimerHandle MatchTimerHandle;

	// 테스트용, 시작 인원 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MinimumPlayersToStart = 2;

	// 최소 인원 접속 시 자동 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	bool bAutoStartMatch = true;
};
