// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"

#include "PlayerState/HBLobbyPlayerState.h"
#include "UI/Lobby/HBLobbyWidget.h"
#include "UI/Lobby/HBLobbyHUDWidget.h"
#include "HBLobbyGameMode.h"
#include "HBLobbyGameState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

void AHBLobbyPlayerController::SetReady(bool bNewReady)
{
	if (!IsLocalController())
	{
		return;
	}

	// All machines are clients in the VM topology.  Only the authoritative
	// dedicated server is allowed to change the replicated ready state.
	ServerSetReady(bNewReady);
}

void AHBLobbyPlayerController::RequestStartGame()
{
	if (!IsLocalController())
	{
		return;
	}

	RequestStartDedicatedMatch();
}

bool AHBLobbyPlayerController::RequestStartDedicatedMatch()
{
	if (!IsLocalController())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Match start rejected: Controller is not local")
			);

		return false;
	}

	const AHBLobbyGameState* LobbyGameState =
		GetWorld() != nullptr
			? GetWorld()->GetGameState<AHBLobbyGameState>()
			: nullptr;

	if (!IsValid(LobbyGameState) || !LobbyGameState->CanStartMatch())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | Match start rejected: Ready conditions are not satisfied")
			);

		return false;
	}

	// This is a per-player confirmation, not a host command.  The server RPC
	// identifies the sender from its owning controller and never trusts a
	// client-supplied player or Steam ID.
	ServerRequestStartGame();
	return true;
}

void AHBLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	GetWorldTimerManager().SetTimer(
		LobbyDebugTimer, this, &AHBLobbyPlayerController::RefreshLobbyDebug, 0.25f, true);
#endif

	LobbyWidget = CreateWidget<UHBLobbyHUDWidget>(
		this,
		LobbyWidgetClass
		);
	
	if (!IsValid(LobbyWidget))
	{
		return;
	}
	
	LobbyWidget->AddToViewport();
	
	FInputModeUIOnly InputMode;
	//InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());
	
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AHBLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LobbyDebugTimer);
	if (IsLocalController() && GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(uint64(GetUniqueID()) | (uint64(1) << 32));
	}
	LobbyDebugPlayers.Empty();
	Super::EndPlay(EndPlayReason);
}

void AHBLobbyPlayerController::ShowLobbyDebugEvent(const FString& Message)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Lobby | Client Debug | %s"), *Message);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.0f, FColor::Yellow, Message);
	}
#endif
}

void AHBLobbyPlayerController::RefreshLobbyDebug()
{
#if !UE_BUILD_SHIPPING
	const AHBLobbyGameState* State = GetWorld()->GetGameState<AHBLobbyGameState>();
	if (!IsLocalController() || !IsValid(State))
	{
		return;
	}

	TMap<uint32, FLobbyDebugPlayer> CurrentPlayers;
	TArray<FString> Events;
	FString Roster;
	int32 ReadyCount = 0;
	int32 ConfirmedCount = 0;
	for (const APlayerState* Player : State->PlayerArray)
	{
		const AHBLobbyPlayerState* LobbyPlayer = Cast<AHBLobbyPlayerState>(Player);
		if (!IsValid(LobbyPlayer))
		{
			continue;
		}
		FLobbyDebugPlayer Snapshot;
		Snapshot.Name = LobbyPlayer->GetPlayerName();
		if (Snapshot.Name.IsEmpty())
		{
			Snapshot.Name = TEXT("이름 수신 중");
		}
		Snapshot.bReady = LobbyPlayer->IsReady();
		ReadyCount += Snapshot.bReady ? 1 : 0;
		ConfirmedCount += LobbyPlayer->IsStartConfirmed() ? 1 : 0;
		Roster += FString::Printf(TEXT("\n  %s [%s]"), *Snapshot.Name,
			Snapshot.bReady ? TEXT("Ready") : TEXT("대기"));

		const uint32 Key = LobbyPlayer->GetUniqueID();
		const FLobbyDebugPlayer* Previous = LobbyDebugPlayers.Find(Key);
		if (!Previous)
		{
			Events.Add(FString::Printf(TEXT("[Lobby] %s: %s"),
				bLobbyDebugInitialized ? TEXT("입장") : TEXT("현재 플레이어"), *Snapshot.Name));
		}
		else if (Previous->bReady != Snapshot.bReady)
		{
			Events.Add(FString::Printf(TEXT("[Lobby] Ready 변경: %s -> %s"),
				*Snapshot.Name, Snapshot.bReady ? TEXT("Ready") : TEXT("대기")));
		}
		CurrentPlayers.Add(Key, MoveTemp(Snapshot));
	}
	for (const auto& Previous : LobbyDebugPlayers)
	{
		if (!CurrentPlayers.Contains(Previous.Key))
		{
			Events.Add(FString::Printf(TEXT("[Lobby] 퇴장: %s"), *Previous.Value.Name));
		}
	}
	const FString Counts = FString::Printf(
		TEXT("현재 인원: %d명 | Ready: %d/%d명 | 시작 확인: %d/%d명"),
		CurrentPlayers.Num(), ReadyCount, CurrentPlayers.Num(), ConfirmedCount, CurrentPlayers.Num());
	for (const FString& Event : Events)
	{
		ShowLobbyDebugEvent(Event + TEXT("\n") + Counts);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(uint64(GetUniqueID()) | (uint64(1) << 32),
			0.5f, FColor::Cyan, TEXT("[Lobby Debug] ") + Counts + Roster);
	}
	bLobbyDebugInitialized |= !CurrentPlayers.IsEmpty();
	LobbyDebugPlayers = MoveTemp(CurrentPlayers);
#endif
}

void AHBLobbyPlayerController::ServerRequestStartGame_Implementation()
{
	AHBLobbyGameMode* LobbyGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AHBLobbyGameMode>()
		: nullptr;

	if (!IsValid(LobbyGameMode))
	{
		return;
	}

	// The controller's own PlayerState is the server-side identity of the
	// caller, so a client cannot confirm on behalf of another player.
	LobbyGameMode->ConfirmStart(
		GetPlayerState<AHBLobbyPlayerState>()
		);
}

void AHBLobbyPlayerController::ServerSetReady_Implementation(bool bNewReady)
{
	AHBLobbyPlayerState* LobbyPlayerState = GetPlayerState<AHBLobbyPlayerState>();
	
	if (!IsValid(LobbyPlayerState))
	{
		return;
	}
	
	const bool bReadyStateChanged =
		LobbyPlayerState->IsReady() != bNewReady;

	LobbyPlayerState->SetReady(bNewReady);
	
	AHBLobbyGameMode* LobbyGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AHBLobbyGameMode>()
		: nullptr;

	if (IsValid(LobbyGameMode) && bReadyStateChanged)
	{
		// Any ready-list change invalidates votes cast for the previous group.
		LobbyGameMode->HandleReadyStateChanged();
	}
}
