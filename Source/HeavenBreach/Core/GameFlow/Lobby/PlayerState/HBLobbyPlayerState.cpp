// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/PlayerState/HBLobbyPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "Player/Job/HBJobData.h"

AHBLobbyPlayerState::AHBLobbyPlayerState()
{
	bIsReady = false;
	bStartConfirmed = false;

	// 기본 직업은 여기서 정하지 않는다.
	// 직업은 이제 부모의 StartingLoadout 이 들고 있고, 그 값은 실제 DA 를
	// 가리켜야 하는데 생성자 시점에는 GameMode 의 직업 목록에 닿을 수 없다.
	// AHBLobbyGameMode::PostLogin 이 입장 시 채운다.
}

FName AHBLobbyPlayerState::GetSelectedJobId() const
{
	const UHBJobData* Job = GetSelectedJob();
	if (!Job)
	{
		return NAME_None;
	}
	return Job->JobId;
}

void AHBLobbyPlayerState::HandleStartingLoadoutChanged()
{
	Super::HandleStartingLoadoutChanged();

	const FName CurrentJobId = GetSelectedJobId();

	// 스킬 슬롯만 바뀐 경우에는 직업 변경 이벤트를 보내지 않는다.
	// 직업 선택 화면이 스킬을 고를 때마다 다시 그려질 이유가 없다.
	if (LastNotifiedJobId == CurrentJobId)
	{
		return;
	}

	LastNotifiedJobId = CurrentJobId;
	OnSelectedJobChanged.Broadcast(CurrentJobId);
}

void AHBLobbyPlayerState::SetReady(bool bNewReady)
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (bIsReady == bNewReady)
	{
		return;
	}
	
	bIsReady = bNewReady;
	
	UE_LOG(
	LogTemp,
	Log,
	TEXT("Lobby | Player: %s | Ready: %s"),
	*GetPlayerName(),
	bIsReady ? TEXT("True") : TEXT("False")
	);
	
	OnReadyStateChanged.Broadcast(bIsReady);
	ForceNetUpdate();
}

void AHBLobbyPlayerState::SetStartConfirmed(bool bNewStartConfirmed)
{
	if (!HasAuthority() || bStartConfirmed == bNewStartConfirmed)
	{
		return;
	}

	// The dedicated server owns this flag; clients can only request their own
	// confirmation through their PlayerController RPC.
	bStartConfirmed = bNewStartConfirmed;
	OnStartConfirmationChanged.Broadcast(bStartConfirmed);
	ForceNetUpdate();
}

void AHBLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHBLobbyPlayerState, bIsReady);
	DOREPLIFETIME(AHBLobbyPlayerState, bStartConfirmed);
}

void AHBLobbyPlayerState::OnRep_IsReady()
{
	OnReadyStateChanged.Broadcast(bIsReady);
}

void AHBLobbyPlayerState::OnRep_StartConfirmed()
{
	OnStartConfirmationChanged.Broadcast(bStartConfirmed);
}
