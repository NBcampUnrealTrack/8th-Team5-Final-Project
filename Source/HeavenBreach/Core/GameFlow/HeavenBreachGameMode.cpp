// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

#include "Core/GameFlow/HeavenBreachGameState.h"
#include "Player/HBPlayerController.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include  "Core/Online/Session/HBGameSession.h"

namespace HBMatchFlow
{
	const TCHAR* PostMatchMap = TEXT("/Game/Levels/ExtractionResult");
}

AHeavenBreachGameMode::AHeavenBreachGameMode()
{
	bDelayedStart = true;

	GameStateClass = AHeavenBreachGameState::StaticClass();
	PlayerControllerClass = AHBPlayerController::StaticClass();
	PlayerStateClass = AHB_PlayerState::StaticClass();
	DefaultPawnClass = AHBPlayerCharacter::StaticClass();
	
	GameSessionClass = AHBGameSession::StaticClass();
}

AHBDungeonRuntime* AHeavenBreachGameMode::GetDungeonGenerator() const
{
    for (TActorIterator<AHBDungeonRuntime> It(GetWorld()); It; ++It) return *It;
    return nullptr;
}

void AHeavenBreachGameMode::StartPlay()
{
    if (auto* Dungeon = GetDungeonGenerator())
    {
        Dungeon->OnReady.AddUniqueDynamic(this, &AHeavenBreachGameMode::HandleDungeonReady);
        Dungeon->OnFailed.AddUniqueDynamic(this, &AHeavenBreachGameMode::HandleDungeonFailed);
    }
    Super::StartPlay();
    if (auto* Dungeon = GetDungeonGenerator()) Dungeon->StartGeneration();
}

void AHeavenBreachGameMode::StartMatch()
{
    // Guard both automatic start and direct Blueprint/manual StartMatch calls.
    if (auto* Dungeon = GetDungeonGenerator(); Dungeon && (!Dungeon->IsReady() || !Dungeon->GetPlayerStart())) return;
    Super::StartMatch();
}

void AHeavenBreachGameMode::RestartPlayer(AController* NewPlayer)
{
    if (!NewPlayer) return;
    if (auto* Dungeon = GetDungeonGenerator())
    {
        if (!Dungeon->IsReady() || !Dungeon->GetPlayerStart()) return;
        // Replace a cached lobby/persistent-level start with the generated room start.
        NewPlayer->StartSpot = Dungeon->GetPlayerStart();
    }
    Super::RestartPlayer(NewPlayer);
    if (auto* Dungeon = GetDungeonGenerator(); Dungeon && !NewPlayer->GetPawn())
        Dungeon->Fail(TEXT("Gameplay character could not spawn at DungeonSpawn."));
}

AActor* AHeavenBreachGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (auto* Dungeon = GetDungeonGenerator()) return Dungeon->GetPlayerStart();
    return Super::ChoosePlayerStart_Implementation(Player);
}

void AHeavenBreachGameMode::HandleDungeonReady()
{
    if (ReadyToStartMatch()) StartMatch();
}

void AHeavenBreachGameMode::HandleDungeonFailed(const FString& Reason)
{
    UE_LOG(LogTemp, Error, TEXT("HeavenBreachGM | Dungeon unavailable; player start blocked | %s"), *Reason);
    if (IsMatchInProgress()) { StopMatchTimer(); AbortMatch(); }
}

void AHeavenBreachGameMode::StartExtractionMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	if (GetMatchState() != MatchState::WaitingToStart)
	{
		return;
	}

	StartMatch();
}

void AHeavenBreachGameMode::FinishExtractionMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!IsMatchInProgress())
	{
		return;
	}

	EndMatch();
}

int32 AHeavenBreachGameMode::GetConnectedPlayerCount() 
{
	return GetNumPlayers();
}

void AHeavenBreachGameMode::NotifyPlayerDied(APlayerState* PlayerState)
{
	if (!HasAuthority() || !IsMatchInProgress() || !IsValid(PlayerState))
	{
		return;
	}
	
	const int32 RemovedCount = ActivePlayers.Remove(PlayerState);
	if (RemovedCount == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("HeavenBreachGM | Player Died but was not active | Player: %s | ActivePlayers: %d"),
			*PlayerState->GetPlayerName(),
			ActivePlayers.Num()
			);

		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Player Died | ActivePlayers: %d"), 
		ActivePlayers.Num()
		);

	// 바로 결과 맵으로 보내지 않는다. 시체를 남기고 매치가 끝날 때까지 팀원을 관전한다.
	FailedPlayers.Add(PlayerState);
	LeaveCorpse(PlayerState->GetOwningController(), PlayerState, true);
	EvaluateMatchEnd();
}

void AHeavenBreachGameMode::NotifyPlayerExtracted(APlayerState* PlayerState)
{
	if (!HasAuthority() || !IsMatchInProgress() || !IsValid(PlayerState))
	{
		return;
	}
	
	const int32 RemovedCount = ActivePlayers.Remove(PlayerState);
	if (RemovedCount == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("HeavenBreachGM | Player Extracted but was not active | Player: %s | ActivePlayers: %d"),
			*PlayerState->GetPlayerName(),
			ActivePlayers.Num()
			);

		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Player Extracted | ActivePlayers: %d"),
		ActivePlayers.Num()
		);

	TravelPlayerToPostMatchMap(PlayerState, EHBPostMatchReason::Extracted);
	EvaluateMatchEnd();
}

void AHeavenBreachGameMode::TravelPlayersToPostMatchMap(EHBPostMatchReason Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();

		if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerState))
		{
			continue;
		}

		TravelPlayerToPostMatchMap(PlayerController->PlayerState, Reason);
	}
}

void AHeavenBreachGameMode::TravelPlayerToPostMatchMap(APlayerState* PlayerState, EHBPostMatchReason Reason)
{
	if (!HasAuthority() || !IsValid(PlayerState) || PostMatchTravelRequestedPlayers.Contains(PlayerState))
	{
		return;
	}

	// 아이템을 잃은 사람은 매치가 어떻게 끝났든 사망 결과를 본다. (Died 가 아니면 탈출 성공 창이 뜬다)
	if (FailedPlayers.Contains(PlayerState))
	{
		Reason = EHBPostMatchReason::Died;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!IsValid(PlayerController) || PlayerController->PlayerState != PlayerState)
		{
			continue;
		}

		PostMatchTravelRequestedPlayers.Add(PlayerState);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("HeavenBreachGM | Travel Player To PostMatch Map | Player: %s | Map: %s | Reason: %d"),
			*PlayerState->GetPlayerName(),
			HBMatchFlow::PostMatchMap,
			static_cast<int32>(Reason)
			);

		// PlayerState/GameState don't survive this travel, so hand the reason to the
		// client's GameInstance subsystem before it disconnects into the results map.
		if (AHBPlayerController* HBPlayerController = Cast<AHBPlayerController>(PlayerController))
		{
			HBPlayerController->ClientSetPostMatchReason(Reason);
		}

		PlayerController->ClientTravel(HBMatchFlow::PostMatchMap, TRAVEL_Absolute);
		return;
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("HeavenBreachGM | PostMatch travel failed: PlayerController not found | Player: %s"),
		*PlayerState->GetPlayerName()
		);
}

void AHeavenBreachGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Both PostLogin and seamless player migration reach this hook. Let the
	// engine restart players joining an active match before updating our roster.
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	if (!HasAuthority())
	{
		return;
	}
	
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("HeavenBreach | Player initialized | ConnectedPlayers: %d / %d"),
		GetConnectedPlayerCount(),
		MinimumPlayersToStart
		);

	if (IsMatchInProgress())
	{
		if (IsValid(NewPlayer) && IsValid(NewPlayer->PlayerState)
			&& !ActivePlayers.Contains(NewPlayer->PlayerState))
		{
			ActivePlayers.Add(NewPlayer->PlayerState);

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("HeavenBreachGM | Late player added to active match | Player: %s | ActivePlayers: %d"),
				*NewPlayer->PlayerState->GetPlayerName(),
				ActivePlayers.Num()
				);
		}

		return;
	}
}

void AHeavenBreachGameMode::Logout(AController* Exiting)
{
	APlayerState* ExitingPlayerState = Exiting ? Exiting->PlayerState : nullptr;
	
	if (IsMatchInProgress() && IsValid(ExitingPlayerState))
	{
		// 살아 있는 채로 나가면 죽은 것과 같다. 강제 종료로 아이템을 지키지 못하게 한다.
		if (ActivePlayers.Contains(ExitingPlayerState))
		{
			LeaveCorpse(Exiting, ExitingPlayerState, false);
		}

		const int32 RemovedCount =
		   ActivePlayers.Remove(ExitingPlayerState);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("HeavenBreachGM | Player Logout | Removed: %d | ActivePlayers: %d"),
			RemovedCount,
			ActivePlayers.Num()
		);
		//ActivePlayers.Remove(ExitingPlayerState);
	}
	
	Super::Logout(Exiting);
	
	EvaluateMatchEnd();

	if (GetMatchState() == MatchState::WaitingPostMatch)
	{
		ScheduleMatchMapReload();
	}
}

bool AHeavenBreachGameMode::ReadyToStartMatch_Implementation()
{
	// GetNumPlayers() includes clients still travelling with their lobby state.
	// Wait for migration to finish before taking the initial active-player roster.
	// Keep this a predicate: AGameMode starts the match after the current state
	// transition completes, or when an initialized player is ready to start.
	return HasAuthority()
		&& GetMatchState() == MatchState::WaitingToStart
		&& bAutoStartMatch
		&& NumTravellingPlayers == 0
		&& (!GetDungeonGenerator() || GetDungeonGenerator()->IsReady())
        && NumPlayers >= (GetDungeonGenerator() && GetNetMode() == NM_Standalone ? 1 : FMath::Max(1, MinimumPlayersToStart));
}

bool AHeavenBreachGameMode::ReadyToEndMatch_Implementation()
{
	// 매치 종료 조건 추가 예정

	return false;
}

void AHeavenBreachGameMode::HandleMatchHasStarted()
{
	UE_LOG(LogTemp, Log,
		TEXT("HeavenBreachGM | Starting match | InitializedPlayers: %d | Required: %d"),
		NumPlayers, FMath::Max(1, MinimumPlayersToStart));

	Super::HandleMatchHasStarted();
	
	if (auto* Dungeon = GetDungeonGenerator(); Dungeon && !Dungeon->IsReady()) return;
	InitializeActivePlayers();
	StartMatchTimer();
}

void AHeavenBreachGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	
	StopMatchTimer();
	ActivePlayers.Reset();

	TravelPlayersToPostMatchMap(EHBPostMatchReason::MatchEnded);
	ScheduleMatchMapReload();
}

void AHeavenBreachGameMode::HandleLeavingMap()
{
	Super::HandleLeavingMap();
}

void AHeavenBreachGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();
}

void AHeavenBreachGameMode::InitializeActivePlayers()
{
	ActivePlayers.Reset();
	PostMatchTravelRequestedPlayers.Reset();
	FailedPlayers.Reset();
	
	AHeavenBreachGameState* HBGameState = GetGameState<AHeavenBreachGameState>();
	if (!HBGameState)
	{
		return;
	}
	
	for (APlayerState* PlayerState : HBGameState->PlayerArray)
	{
		if (IsValid(PlayerState))
		{
			ActivePlayers.Add(PlayerState);
		}
	}
	
	UE_LOG(
	LogTemp,
	Log,
	TEXT("HeavenBreachGM | ActivePlayers Initialized : %d"),
	ActivePlayers.Num()
);
}

void AHeavenBreachGameMode::EvaluateMatchEnd()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (!IsMatchInProgress())
	{
		return;
	}
	
	if (!ActivePlayers.IsEmpty())
	{
		return;
	}
	
	FinishExtractionMatch();
}

void AHeavenBreachGameMode::ScheduleMatchMapReload()
{
	if (!HasAuthority() || GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	if (GetMatchState() != MatchState::WaitingPostMatch || bMatchMapReloadScheduled)
	{
		return;
	}

	bMatchMapReloadScheduled = true;

	GetWorldTimerManager().SetTimer(
		MatchMapReloadTimerHandle,
		this,
		&AHeavenBreachGameMode::TryReloadMatchMap,
		MatchMapReloadRetrySeconds,
		false
	);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Match map reload scheduled | RetryDelay: %.2f"),
		MatchMapReloadRetrySeconds
	);
}

void AHeavenBreachGameMode::TryReloadMatchMap()
{
	bMatchMapReloadScheduled = false;

	if (!HasAuthority() || GetNetMode() != NM_DedicatedServer)
	{
		return;
	}

	if (GetMatchState() != MatchState::WaitingPostMatch)
	{
		return;
	}

	const int32 ConnectedPlayerCount = GetConnectedPlayerCount();
	if (ConnectedPlayerCount > 0)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("HeavenBreachGM | Waiting to reload match map | ConnectedPlayers: %d"),
			ConnectedPlayerCount
		);

		ScheduleMatchMapReload();
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("HeavenBreachGM | Reloading match map for next match")
	);

	RestartGame();
}

void AHeavenBreachGameMode::StartMatchTimer()
{
	if (!HasAuthority())
	{
		return;
	}
	
	AHeavenBreachGameState* HBGameState = GetGameState<AHeavenBreachGameState>();
	if (!HBGameState)
	{
		return;
	}
	
	const double EndServerTime = HBGameState->GetServerWorldTimeSeconds() + static_cast<double>(MatchDurationSeconds);
	HBGameState->SetMatchEndServerTime(EndServerTime);
	
	GetWorldTimerManager().SetTimer(
		MatchTimerHandle,
		this,
		&AHeavenBreachGameMode::HandleMatchTimeExpired,
		MatchDurationSeconds,
		false
		);
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Match Timer Started | Duration : %.2f"), MatchDurationSeconds
		);
}

void AHeavenBreachGameMode::StopMatchTimer()
{
	if (!HasAuthority())
	{
		return;
	}
	
	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	
	if (AHeavenBreachGameState* HBGameState = GetGameState<AHeavenBreachGameState>())
	{
		HBGameState->SetMatchEndServerTime(0.0f);
	}
}

void AHeavenBreachGameMode::HandleMatchTimeExpired()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (!IsMatchInProgress())
	{
		return;
	}
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("HeavenBreachGM | Match Time Expired")
		);
	
	// 시간 안에 못 나간 사람은 들고 있던 것을 잃는다.
	for (APlayerState* PlayerState : ActivePlayers)
	{
		if (AHB_PlayerState* HBPlayerState = Cast<AHB_PlayerState>(PlayerState))
		{
			HBPlayerState->ForfeitCarriedItems(nullptr);
			FailedPlayers.Add(PlayerState);
		}
	}

	FinishExtractionMatch();
}

void AHeavenBreachGameMode::LeaveCorpse(AController* Controller, APlayerState* PlayerState, bool bSpectate)
{
	AHBPlayerCharacter* Character = PlayerState ? Cast<AHBPlayerCharacter>(PlayerState->GetPawn()) : nullptr;
	if (Character)
	{
		Character->BecomeCorpse();
	}
	else if (AHB_PlayerState* HBPlayerState = Cast<AHB_PlayerState>(PlayerState))
	{
		// 폰이 없으면 옮길 곳이 없다. 잃기만 한다.
		HBPlayerState->ForfeitCarriedItems(nullptr);
	}

	// 빙의를 풀어야 접속이 끊겨도 폰(=시체)이 같이 지워지지 않는다.
	if (Controller)
	{
		Controller->UnPossess();
	}

	if (bSpectate)
	{
		if (AHBPlayerController* HBPlayerController = Cast<AHBPlayerController>(Controller))
		{
			HBPlayerController->StartSpectatingTeammates();
		}
	}
}
