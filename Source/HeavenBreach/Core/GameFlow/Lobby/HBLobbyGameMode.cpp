// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"

#include "HBLobbyPlayerController.h"
#include "PlayerState/HBLobbyPlayerState.h"
#include "HBLobbyGameState.h"
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
	
	DefaultPawnClass = nullptr;
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

void AHBLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority())
	{
		return;
	}

    if (auto* State = GetGameState<AHBLobbyGameState>(); State && NewPlayer && NewPlayer->IsLocalController())
    {
        State->HostPlayerState = NewPlayer->GetPlayerState<AHBLobbyPlayerState>();
        if (auto* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>()) State->RoomName = Sessions->GetHostedRoomName();
        State->ForceNetUpdate();
    }
	InitializeLobbyLoadout(NewPlayer);

	// A newly connected player changes the group that is approving the match.
	ResetStartConfirmations();
	EvaluateCanStartMatch();
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
	Super::Logout(Exiting);

	if (!HasAuthority())
	{
		return;
	}

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
