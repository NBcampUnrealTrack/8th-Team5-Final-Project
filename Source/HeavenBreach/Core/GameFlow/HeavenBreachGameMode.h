// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Core/GameFlow/HBPostMatchTypes.h"
#include "HeavenBreachGameMode.generated.h"

class APlayerState;
class AHBDungeonRuntime;

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AHeavenBreachGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AHeavenBreachGameMode();
    virtual void StartPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** Internal server logic; UI may invoke this later while WaitingToStart. */
    bool RegenerateDungeon();
    virtual void StartMatch() override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    AHBDungeonRuntime* GetDungeonGenerator() const;
    virtual void Tick(float DeltaSeconds) override;
    bool ReportDungeonLoaded(APlayerController* Player, int32 GenerationId, bool bSucceeded);
    bool IsPlayerDungeonLoaded(APlayerController* Player) const;
    bool AreDungeonPlayersLoaded() const;


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

	// 서버. 이 플레이어에게 매치 안에서 사망 결과 화면을 보여 줬다고 기록한다.
	void MarkDeathResultShown(APlayerState* PlayerState);
	
	// 탈출시 호출
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match")
	void NotifyPlayerExtracted(APlayerState* PlayerState);
	
	// 임시, 창고 기능 보여주기 용도
	// Reason은 ExtractionResult 맵에서 사망/탈출 UI를 구분해서 띄우는 데 쓰인다.
	// (UHBMatchResultSubsystem에 저장 -> AHBPlayerController::TryShowPostMatchResultWidget에서 사용)
	void TravelPlayersToPostMatchMap(EHBPostMatchReason Reason);
	void TravelPlayerToPostMatchMap(APlayerState* PlayerState, EHBPostMatchReason Reason);

	// 서버. 살아 있는 채로 접속이 끊길 때(Logout 전, 폰이 지워지기 전) 시체를 남긴다. 남겼으면 true.
	bool HandleAlivePlayerLeaving(AController* Controller);

protected:
    void HandleDungeonLayoutPlanned();
    void ClearDungeonPlayerLoading();
    TWeakObjectPtr<AHBDungeonRuntime> BoundDungeon;
    FDelegateHandle DungeonLayoutPlannedHandle;
    TMap<TWeakObjectPtr<APlayerController>, int32> LoadedDungeonGenerations;
    TMap<TWeakObjectPtr<APlayerController>, double> DungeonLoadWaitStarted;
    UPROPERTY(EditDefaultsOnly, Category="Match|Dungeon", meta=(ClampMin="1"))
    float ClientDungeonLoadTimeoutSeconds = 120;
    UFUNCTION() void HandleDungeonReady();
    UFUNCTION() void HandleDungeonFailed(const FString& Reason);
	// Shared entry point for new connections and players arriving via seamless travel.
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	virtual bool ReadyToStartMatch_Implementation() override;
	virtual bool ReadyToEndMatch_Implementation() override;
	
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void HandleLeavingMap() override;
	virtual void OnMatchStateSet() override;
	
	// 현재 매치 참가자 초기화
	void InitializeActivePlayers();
	
	// 매치 종료 조건 검사
	void EvaluateMatchEnd();

	// 매치 종료 후 데디케이티드 서버의 현재 맵을 다음 매치용으로 다시 로드
	void ScheduleMatchMapReload();
	void TryReloadMatchMap();
	
	// Match Timer
	void StartMatchTimer();
	void StopMatchTimer();
	void HandleMatchTimeExpired();
	
	UPROPERTY(Transient)
	TSet<TObjectPtr<APlayerState>> ActivePlayers;

	UPROPERTY(Transient)
	TSet<TObjectPtr<APlayerState>> PostMatchTravelRequestedPlayers;
	
	// 아이템을 잃은 사람(사망·시간 종료). 매치가 끝날 때 Died 사유로 보낸다.
	UPROPERTY(Transient)
	TSet<TObjectPtr<APlayerState>> FailedPlayers;

	// 사망 결과 화면을 매치 안에서 이미 본 사람. 매치가 끝날 때 결과 맵을 건너뛰고 허브로 바로 보낸다.
	// 사망 직후 결과 화면이 뜨기 전에 매치가 끝난 사람은 여기에 없어서 기존대로 결과 맵을 본다.
	UPROPERTY(Transient)
	TSet<TObjectPtr<APlayerState>> DeathResultShownPlayers;

	// 서버. 시체를 남기고 빙의를 푼다. bSpectate 면 관전으로 넘긴다.
	void LeaveCorpse(AController* Controller, APlayerState* PlayerState, bool bSpectate);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match | Timer", meta = (ClamMin = "1.0"))
	float MatchDurationSeconds = 1500.0f;
	
	FTimerHandle MatchTimerHandle;
	FTimerHandle MatchMapReloadTimerHandle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match | Reset", meta = (ClampMin = "0.1"))
	float MatchMapReloadRetrySeconds = 1.0f;

	bool bMatchMapReloadScheduled = false;

	// 테스트용, 시작 인원
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MinimumPlayersToStart = 2;

	// 최소 인원 접속 시 자동 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	bool bAutoStartMatch = true;
};
