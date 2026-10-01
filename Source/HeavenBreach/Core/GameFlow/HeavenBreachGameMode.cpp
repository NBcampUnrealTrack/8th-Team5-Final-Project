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
    PrimaryActorTick.bCanEverTick = true;

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
    if (!AreDungeonPlayersLoaded()) return;
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
        if (!IsPlayerDungeonLoaded(Cast<APlayerController>(NewPlayer))) return;
        if (!IsMatchInProgress() && !AreDungeonPlayersLoaded()) return;
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

bool AHeavenBreachGameMode::IsPlayerDungeonLoaded(APlayerController* Player) const
{
    auto* Dungeon = GetDungeonGenerator();
    if (!Dungeon) return true;
    if (!IsValid(Player) || !Dungeon->IsReady()) return false;
    // The listen host uses this same world; no network acknowledgement is needed.
    if (Player->IsLocalController()) return true;
    const auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>();
    const int32* Loaded = LoadedDungeonGenerations.Find(Player);
    return Source && Loaded && *Loaded > 0 && *Loaded == Source->GetDungeonSnapshot().GenerationId
        && *Loaded == Dungeon->GetGenerationId();
}

bool AHeavenBreachGameMode::AreDungeonPlayersLoaded() const
{
    auto* Dungeon = GetDungeonGenerator();
    if (!Dungeon) return true;
    if (!Dungeon->IsReady() || NumTravellingPlayers != 0) return false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        auto* Player = It->Get();
        if (!IsValid(Player)) continue;
        if (!Player->PlayerState) return false;
        if (Player->PlayerState->IsOnlyASpectator()) continue;
        if (!IsPlayerDungeonLoaded(Player)) return false;
    }
    return true;
}

bool AHeavenBreachGameMode::ReportDungeonLoaded(APlayerController* Player, int32 GenerationId, bool bSucceeded)
{
    const auto* Source = GetWorld()->GetGameState<AHeavenBreachGameState>();
    auto* Dungeon = GetDungeonGenerator();
    if (!HasAuthority() || GetNetMode() == NM_Client || !IsValid(Player) || Player->GetWorld() != GetWorld() || !Player->PlayerState
        || Player->PlayerState->IsOnlyASpectator() || !Source || !Dungeon || GenerationId <= 0
        || GenerationId != Source->GetDungeonSnapshot().GenerationId
        || Dungeon->State == EHBDungeonRuntimeState::Failed) return false;
    bool bConnected = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (It->Get() == Player) { bConnected = true; break; }
    if (!bConnected) return false;
    if (const int32* Existing = LoadedDungeonGenerations.Find(Player); Existing && *Existing == GenerationId) return true;
    if (!bSucceeded)
    {
        Dungeon->Fail(TEXT("A client failed to prepare the current dungeon generation."));
        return false;
    }
    // Clients can finish before the host. Preserve their acknowledgement until host readiness.
    LoadedDungeonGenerations.Add(Player, GenerationId);
    DungeonLoadWaitStarted.Remove(Player);
    if (IsMatchInProgress() && IsPlayerDungeonLoaded(Player) && !Player->GetPawn())
    {
        RestartPlayer(Player);
        if (Player->GetPawn()) ActivePlayers.Add(Player->PlayerState);
    }
    else if (ReadyToStartMatch()) StartMatch();
    return true;
}

void AHeavenBreachGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* Dungeon = GetDungeonGenerator();
    if (!Dungeon || !Dungeon->IsReady() || (GetMatchState() != MatchState::WaitingToStart && !IsMatchInProgress())) return;
    const double Now = FPlatformTime::Seconds();
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        auto* Player = It->Get();
        if (!IsValid(Player) || !Player->PlayerState || Player->PlayerState->IsOnlyASpectator() || IsPlayerDungeonLoaded(Player)) continue;
        double& Started = DungeonLoadWaitStarted.FindOrAdd(Player, Now);
        if (Now - Started >= FMath::Max(1.f, ClientDungeonLoadTimeoutSeconds))
        {
            Dungeon->Fail(TEXT("Timed out waiting for client dungeon loading acknowledgement."));
            return;
        }
    }
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

	TravelPlayerToPostMatchMap(PlayerState, EHBPostMatchReason::Died);
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
            && IsPlayerDungeonLoaded(NewPlayer) && NewPlayer->GetPawn()
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
    if (auto* Player = Cast<APlayerController>(Exiting))
    {
        LoadedDungeonGenerations.Remove(Player);
        DungeonLoadWaitStarted.Remove(Player);
    }
	APlayerState* ExitingPlayerState = Exiting ? Exiting->PlayerState : nullptr;
	
	if (IsMatchInProgress() && IsValid(ExitingPlayerState))
	{
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
		&& AreDungeonPlayersLoaded()
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
	if (auto* Dungeon = GetDungeonGenerator())
	{
		FString Error;
		if (!Dungeon->ActivateEncounters(Error))
		{
			Dungeon->Fail(FString::Printf(TEXT("Dungeon encounter activation failed: %s"), *Error));
			return;
		}
	}
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
	
	FinishExtractionMatch();
}
