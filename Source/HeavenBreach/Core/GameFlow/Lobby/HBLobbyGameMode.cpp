// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyGameMode.h"

#include "HBLobbyPlayerController.h"
#include "PlayerState/HBLobbyPlayerState.h"
#include "HBLobbyGameState.h"
#include "Core/Online/Session/HBGameSession.h"
#include "Player/Job/HBJobData.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Misc/ConfigCacheIni.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"

namespace HBLobbyFlow
{
	const TCHAR* ConfigSection = TEXT("HeavenBreach.Lobby");
	const TCHAR* GameplayMapKey = TEXT("GameplayMap");
	const TCHAR* DefaultGameplayMap =
		TEXT("/Game/Levels/TierA/Maps/Floor_A.Floor_A");
}

AHBLobbyGameMode::AHBLobbyGameMode()
{
	PlayerControllerClass = AHBLobbyPlayerController::StaticClass();
	PlayerStateClass = AHBLobbyPlayerState::StaticClass();
	GameStateClass = AHBLobbyGameState::StaticClass();
	GameSessionClass = AHBGameSession::StaticClass();
	
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = true;

	// Keep the native default usable without editing the lobby map or its
	// Blueprint GameMode asset.
	GameplayMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(
		HBLobbyFlow::DefaultGameplayMap
		));
}

void AHBLobbyGameMode::EvaluateCanStartMatch()
{
	AHBLobbyGameState* LobbyGameState = GetGameState<AHBLobbyGameState>();
	
	if (!IsValid(LobbyGameState))
	{
		return;
	}
	
	const int32 PlayerCount = LobbyGameState->PlayerArray.Num();
	
	if (PlayerCount < RequiredReadyPlayerCount)
	{
		LobbyGameState->SetCanStartMatch(false);
		return;
	}
	
	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		AHBLobbyPlayerState* LobbyPlayerState = Cast<AHBLobbyPlayerState>(PlayerState);
		
		if (!IsValid(LobbyPlayerState) || !LobbyPlayerState->IsReady())
		{
			LobbyGameState->SetCanStartMatch(false);
			return;
		}
	}
	
	LobbyGameState->SetCanStartMatch(true);
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

void AHBLobbyGameMode::ConfirmStart(
	AHBLobbyPlayerState* RequestingPlayerState)
{
	if (!HasAuthority() || !IsValid(RequestingPlayerState))
	{
		return;
	}

	EvaluateCanStartMatch();

	const AHBLobbyGameState* LobbyGameState =
		GetGameState<AHBLobbyGameState>();

	if (!IsValid(LobbyGameState) ||
		!LobbyGameState->CanStartMatch() ||
		!LobbyGameState->PlayerArray.Contains(RequestingPlayerState))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Start confirmation rejected: ready roster is incomplete")
			);
		return;
	}

	RequestingPlayerState->SetStartConfirmed(true);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Lobby | Player confirmed match start: %s"),
		*RequestingPlayerState->GetPlayerName()
		);

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

	FString ConfiguredGameplayMap;
	const bool bHasConfiguredGameplayMap = GConfig != nullptr &&
		GConfig->GetString(
			HBLobbyFlow::ConfigSection,
			HBLobbyFlow::GameplayMapKey,
			ConfiguredGameplayMap,
			GEngineIni
			);

	RequestMatchTravel(bHasConfiguredGameplayMap
		? TSoftObjectPtr<UWorld>(FSoftObjectPath(ConfiguredGameplayMap)) : GameplayMap);
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
	const AHBLobbyGameState* LobbyGameState =
		GetGameState<AHBLobbyGameState>();

	if (!IsValid(LobbyGameState) ||
		LobbyGameState->PlayerArray.Num() < RequiredReadyPlayerCount)
	{
		return false;
	}

	for (const APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		const AHBLobbyPlayerState* LobbyPlayerState =
			Cast<AHBLobbyPlayerState>(PlayerState);

		if (!IsValid(LobbyPlayerState) ||
			!LobbyPlayerState->IsReady() ||
			!LobbyPlayerState->IsStartConfirmed())
		{
			return false;
		}
	}

	return true;
}
