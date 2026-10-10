// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"

#include "HBLobbyPlayerController.h"
#include "PlayerState/HBLobbyPlayerState.h"
#include "HBLobbyGameState.h"
#include "Player/HBPlayerCharacter.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Core/Online/Session/HBGameSession.h"
#include "Player/Job/HBJobData.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"

AHBLobbyGameMode::AHBLobbyGameMode()
{
	PlayerControllerClass = AHBLobbyPlayerController::StaticClass();
	PlayerStateClass = AHBLobbyPlayerState::StaticClass();
	GameStateClass = AHBLobbyGameState::StaticClass();
	GameSessionClass = AHBGameSession::StaticClass();
	
	DefaultPawnClass = AHBPlayerCharacter::StaticClass();
	bUseSeamlessTravel = true;
}

void AHBLobbyGameMode::StartPlay()
{
	Super::StartPlay();
	if (bAutomaticallyHostInviteHub && HasAuthority())
		if (auto* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
			Sessions->NotifyInviteHubEntered(GetWorld());
}

void AHBLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    // PreLogin only runs for network guests, so the local host never has to
    // send its own password through the startup URL.
    if (ErrorMessage.IsEmpty())
        if (auto* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
            ErrorMessage = Sessions->CheckLobbyPassword(Options);
}

void AHBLobbyGameMode::EvaluateCanStartMatch()
{
    auto* State = GetGameState<AHBLobbyGameState>();
    if (!State || !HasAuthority()) return;
    auto* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>();
    const bool bReady = GetNetMode() == NM_ListenServer && State->AreGuestsReady(RequiredReadyPlayerCount)
        && Sessions && Sessions->IsLocalLobbyHost()
        && Sessions->GetLobbyOperationState() == EHBLobbyOperationState::InLobby
        && !Sessions->IsLobbyOperationInProgress() && !Sessions->IsMatchAdmissionClosed();
    State->SetCanStartMatch(bReady);
}

void AHBLobbyGameMode::HandleReadyStateChanged()
{
	if (!HasAuthority())
	{
		return;
	}

	// A confirmation applies to one exact ready roster.  If anyone toggles
	// ready, every player must approve the new roster again.
	ResetStartConfirmations();
	EvaluateCanStartMatch();
}

void AHBLobbyGameMode::ConfirmStart(AHBLobbyPlayerState* RequestingPlayerState)
{
    if (!HasAuthority()) return;
    EvaluateCanStartMatch();
    auto* State = GetGameState<AHBLobbyGameState>();
    if (!State || !State->CanStartMatch() || !RequestingPlayerState || State->HostPlayerState != RequestingPlayerState) return;
    RequestingPlayerState->SetStartConfirmed(true);
    TryStartGame();
}

void AHBLobbyGameMode::TryStartGame()
{
	if (!HasAuthority())
	{
		return;
	}

	EvaluateCanStartMatch();

	AHBLobbyGameState* LobbyGameState =
		GetGameState<AHBLobbyGameState>();

	if (!IsValid(LobbyGameState) ||
		!LobbyGameState->CanStartMatch() ||
		!AreAllPlayersReadyAndConfirmed())
	{
		return;
	}

	RequestMatchTravel(GameplayMap);
}

bool AHBLobbyGameMode::RequestMatchTravel(TSoftObjectPtr<UWorld> Destination)
{
	if (!HasAuthority() || !GetWorld() || GetNetMode() != NM_ListenServer) return false;
	UHBOnlineSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>();
	return Sessions && Sessions->RequestServerMatchTravel(Destination);
}

UHBJobData* AHBLobbyGameMode::FindJobById(FName JobId) const
{
	if (JobId.IsNone())
	{
		return nullptr;
	}

	for (const TObjectPtr<UHBJobData>& Job : SelectableJobs)
	{
		if (!Job)
		{
			continue;
		}
		if (Job->JobId != JobId)
		{
			continue;
		}
		return Job.Get();
	}

	// 조용히 실패하면 "직업 버튼을 눌렀는데 아무 일도 안 난다" 가 된다.
	UE_LOG(LogTemp, Warning,
		TEXT("Lobby | 직업 '%s' 를 SelectableJobs 에서 찾지 못했다. "
			 "BP_HBLobbyGameMode 의 Selectable Jobs 에 해당 DA 가 있는지 확인할 것."),
		*JobId.ToString());
	return nullptr;
}

UHBJobData* AHBLobbyGameMode::GetDefaultJob() const
{
	for (const TObjectPtr<UHBJobData>& Job : SelectableJobs)
	{
		if (!Job)
		{
			continue;
		}
		return Job.Get();
	}

	UE_LOG(LogTemp, Error,
		TEXT("Lobby | SelectableJobs 가 비어 있다. "
			 "BP_HBLobbyGameMode 에 직업 DA 를 최소 하나 등록해야 한다."));
	return nullptr;
}

void AHBLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!HasAuthority() || !NewPlayer)
	{
		return;
	}

    if (auto* State = GetGameState<AHBLobbyGameState>(); State && NewPlayer && NewPlayer->IsLocalController())
    {
        State->HostPlayerState = NewPlayer->GetPlayerState<AHBLobbyPlayerState>();
        if (auto* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>()) State->RoomName = Sessions->GetHostedRoomName();
        State->ForceNetUpdate();
    }
	if (auto* Player = NewPlayer->GetPlayerState<AHBLobbyPlayerState>(); Player && !Player->HasSelectedJob())
		InitializeLobbyLoadout(NewPlayer);
	RefreshLobbySeats(NewPlayer);

	// A newly connected player changes the group that is approving the match.
	ResetStartConfirmations();
	EvaluateCanStartMatch();
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

AActor* AHBLobbyGameMode::FindLobbySeat(UWorld* World, int32 SeatIndex)
{
	if (!World || SeatIndex < 0 || SeatIndex >= 4) return nullptr;
	const FName SeatTag(*FString::Printf(TEXT("LobbySlot_%d"), SeatIndex));
	AActor* Found = nullptr;
	for (TActorIterator<ATargetPoint> It(World); It; ++It)
		if (It->ActorHasTag(TEXT("LobbyCharacterSlot")) && It->ActorHasTag(SeatTag))
		{
			if (Found)
			{
				UE_LOG(LogTemp, Warning, TEXT("Lobby | Duplicate seat anchor: %s"), *SeatTag.ToString());
				return nullptr;
			}
			Found = *It;
		}
	return Found;
}

void AHBLobbyGameMode::RefreshLobbySeats(APlayerController* Joining, AController* Exiting)
{
	auto* State = GetGameState<AHBLobbyGameState>();
	if (!HasAuthority() || !State || !State->HostPlayerState) return;
	TArray<AHBLobbyPlayerState*> Players;
	for (APlayerState* Player : State->PlayerArray)
	{
		auto* LobbyPlayer = Cast<AHBLobbyPlayerState>(Player);
		auto* Controller = LobbyPlayer ? Cast<APlayerController>(LobbyPlayer->GetOwner()) : nullptr;
		if (IsValid(Controller) && Controller != Exiting && !LobbyPlayer->IsInactive()
			&& Controller->PlayerState == LobbyPlayer
			&& (Controller == Joining || LobbyPlayer->GetLobbySeatIndex() != INDEX_NONE))
			Players.Add(LobbyPlayer);
	}
	// Preserve accepted join order using the existing seats, even if PlayerArray changes order.
	Players.StableSort([State](const AHBLobbyPlayerState& A, const AHBLobbyPlayerState& B)
	{
		auto Rank = [State](const AHBLobbyPlayerState& Player)
		{
			if (&Player == State->HostPlayerState) return 0;
			return Player.GetLobbySeatIndex() == INDEX_NONE ? MAX_int32 : Player.GetLobbySeatIndex();
		};
		return Rank(A) < Rank(B);
	});
	int32 NextGuestSeat = 1;
	for (auto* Player : Players)
	{
		const int32 Seat = Player == State->HostPlayerState ? 0 : NextGuestSeat++;
		Player->SetLobbySeatIndex(Seat < 4 ? Seat : INDEX_NONE);
		if (Seat >= 4) continue;
		RestartPlayer(Cast<APlayerController>(Player->GetOwner()));
	}
}

void AHBLobbyGameMode::RestartPlayer(AController* NewPlayer)
{
	auto* Player = NewPlayer ? NewPlayer->GetPlayerState<AHBLobbyPlayerState>() : nullptr;
	if (!HasAuthority() || !Player || !DefaultPawnClass || Player->GetLobbySeatIndex() == INDEX_NONE) return;
	AActor* Seat = FindLobbySeat(GetWorld(), Player->GetLobbySeatIndex());
	if (!Seat)
	{
		UE_LOG(LogTemp, Warning, TEXT("Lobby | Missing seat anchor for player '%s', seat=%d."),
			*Player->GetPlayerName(), Player->GetLobbySeatIndex());
		return;
	}
	NewPlayer->StartSpot = Seat;
	// Compaction only moves an existing pawn; possession and ClientRestart are for spawning.
	if (!NewPlayer->GetPawn()) Super::RestartPlayer(NewPlayer);
	Player->ApplyLobbySeat();
}

void AHBLobbyGameMode::InitializeLobbyLoadout(APlayerController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	AHBLobbyPlayerState* LobbyPlayerState = NewPlayer->GetPlayerState<AHBLobbyPlayerState>();

	if (!IsValid(LobbyPlayerState))
	{
		return;
	}

	UHBJobData* DefaultJob = GetDefaultJob();

	if (!DefaultJob)
	{
		// GetDefaultJob 이 이미 오류를 남겼다.
		return;
	}

	FHBStartingLoadout DefaultLoadout;
	FText Error;

	if (!MakeDefaultStartingLoadout(DefaultJob, DefaultLoadout, Error))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Lobby | 기본 직업 '%s' 의 시작 구성을 만들지 못했다: %s"),
			*DefaultJob->JobId.ToString(),
			*Error.ToString()
			);
		return;
	}

	if (!LobbyPlayerState->TrySetStartingLoadout(DefaultLoadout, Error))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Lobby | 기본 시작 구성 확정 실패: %s"),
			*Error.ToString()
			);
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Lobby | 기본 구성 | Job=%s Q=%s E=%s P=%s"),
		*DefaultJob->JobId.ToString(),
		*DefaultLoadout.SkillQId.ToString(),
		*DefaultLoadout.SkillEId.ToString(),
		*DefaultLoadout.PassiveSkillId.ToString()
		);
}

void AHBLobbyGameMode::Logout(AController* Exiting)
{
	AHBLobbyPlayerState* Leaving = Exiting ? Exiting->GetPlayerState<AHBLobbyPlayerState>() : nullptr;
	if (HasAuthority())
	{
		if (Leaving) Leaving->SetLobbySeatIndex(INDEX_NONE);
		if (Exiting && Exiting->GetPawn()) Exiting->UnPossess();
	}
	Super::Logout(Exiting);

	if (!HasAuthority())
	{
		return;
	}
	if (auto* State = GetGameState<AHBLobbyGameState>())
	{
		State->RemovePlayerState(Leaving);
		if (State->HostPlayerState == Leaving)
		{
			State->HostPlayerState = nullptr;
			State->ForceNetUpdate();
		}
	}
	RefreshLobbySeats(nullptr, Exiting);

	// Removing a player also changes the approved group; stale confirmations
	// must never make the smaller group start automatically.
	ResetStartConfirmations();
	EvaluateCanStartMatch();
}

void AHBLobbyGameMode::ResetStartConfirmations()
{
	AHBLobbyGameState* LobbyGameState = GetGameState<AHBLobbyGameState>();
	if (!IsValid(LobbyGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		if (AHBLobbyPlayerState* LobbyPlayerState =
			Cast<AHBLobbyPlayerState>(PlayerState))
		{
			LobbyPlayerState->SetStartConfirmed(false);
		}
	}
}

bool AHBLobbyGameMode::AreAllPlayersReadyAndConfirmed() const
{
    const auto* State = GetGameState<AHBLobbyGameState>();
    return State && State->CanStartMatch() && IsValid(State->HostPlayerState)
        && State->HostPlayerState->IsStartConfirmed();
}
