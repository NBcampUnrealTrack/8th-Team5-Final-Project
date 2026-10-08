// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameFlow/Lobby/HBLobbyPlayerController.h"

#include "PlayerState/HBLobbyPlayerState.h"
#include "UI/Lobby/HBLobbyWidget.h"
#include "UI/Lobby/HBLobbyHUDWidget.h"
#include "HBLobbyGameMode.h"
#include "HBLobbyGameState.h"
#include "Player/Job/HBJobData.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Inventory/StashComponent.h"
#include "Craft/CraftComponent.h"
#include "Craft/CraftTypes.h"
#include "Engine/DataTable.h"

void AHBLobbyPlayerController::ClientLobbyClosed_Implementation()
{
	if (auto* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>())
		Sessions->RequestLeaveToMenu(TEXT("호스트가 방을 나갔습니다."));
}

void AHBLobbyPlayerController::SetReady(bool bNewReady)
{
	if (!IsLocalController())
	{
		return;
	}

	// The listen host validates the guest's request and updates replicated Ready.
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

	// The server identifies the caller from its owning controller and accepts
	// the start command only from the host.
	ServerRequestStartGame();
	return true;
}

void AHBLobbyPlayerController::SelectJob(FName JobId)
{
	if (!IsLocalController())
	{
		return;
	}

	// The authoritative host validates and writes the replicated selection.
	ServerSelectJob(JobId);
}

void AHBLobbyPlayerController::ApplySkillLoadout(const FHBStartingLoadout& InLoadout)
{
	if (!IsLocalController())
	{
		return;
	}

	// SetReady / SelectJob 과 같은 이유로 클라는 요청만 하고 서버가 쓴다.
	ServerApplySkillLoadout(InLoadout);
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

	if (!LobbyWidgetClass) return;
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
	LobbyInputViewport = GetWorld()->GetGameViewport();
	SetShowMouseCursor(true);
}

void AHBLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Undo the viewport input block set by UIOnly. CommonUI's Game config does
	// not clear it, and SetInputMode may no longer work on a swapped controller.
	if (UGameViewportClient* Viewport = LobbyInputViewport.Get())
	{
		Viewport->SetIgnoreInput(false);
		UE_LOG(LogTemp, Log, TEXT("Lobby | Exit cleanup | Viewport IgnoreInput: %s"),
			Viewport->IgnoreInput() ? TEXT("True") : TEXT("False"));
	}
	LobbyInputViewport.Reset();

	if (IsValid(LobbyWidget))
	{
		LobbyWidget->RemoveFromParent();
		LobbyWidget = nullptr;
	}

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
	for (const APlayerState* ListedPlayerState : State->PlayerArray)
	{
		const AHBLobbyPlayerState* LobbyPlayer = Cast<AHBLobbyPlayerState>(ListedPlayerState);
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
	// The host starts the match; only guests toggle Ready.
	if (IsLocalController() || !GetWorld() || GetNetMode() != NM_ListenServer) return;
	if (auto* Sessions = GetGameInstance()->GetSubsystem<UHBOnlineSessionSubsystem>(); !Sessions || Sessions->IsMatchAdmissionClosed()) return;
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

void AHBLobbyPlayerController::ServerSelectJob_Implementation(FName JobId)
{
	AHBLobbyPlayerState* LobbyPlayerState = GetPlayerState<AHBLobbyPlayerState>();

	if (!IsValid(LobbyPlayerState))
	{
		return;
	}

	// 준비를 누른 뒤에는 직업을 못 바꾼다. 바꾸려면 준비를 해제해야 한다.
	// 그래야 다른 플레이어가 본 구성과 실제로 매치에 들어가는 구성이 어긋나지 않는다.
	if (LobbyPlayerState->IsReady())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | 준비 상태에서는 직업을 바꿀 수 없다. Player: %s"),
			*LobbyPlayerState->GetPlayerName()
			);
		return;
	}

	AHBLobbyGameMode* LobbyGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<AHBLobbyGameMode>()
		: nullptr;

	if (!IsValid(LobbyGameMode))
	{
		return;
	}

	// 클라가 보낸 것은 이름뿐이다. 등록된 직업인지 서버가 확인한다.
	UHBJobData* Job = LobbyGameMode->FindJobById(JobId);

	if (!Job)
	{
		// 요청한 직업이 등록돼 있지 않다. 선택 UI 에는 아직 만들지 않은 직업
		// 버튼도 있기 때문이다.
		//
		// 왜 거절이 아니라 대체인가: 거절하면 그 플레이어는 직업 없이 남아
		// 매치에서 스킬을 하나도 못 받는다. 기본 직업으로 밀어 주는 편이 낫다.
		//
		// 쳐낼 조건: 모든 직업이 실제로 구현되면 이 대체가 오히려 설정 실수를
		// 숨기게 된다. 그때는 거절로 바꾼다.
		Job = LobbyGameMode->GetDefaultJob();

		if (!Job)
		{
			// GetDefaultJob 이 이미 오류를 남겼다.
			return;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | 요청 직업 '%s' 가 없어 기본 직업 '%s' 로 대체한다."),
			*JobId.ToString(),
			*Job->JobId.ToString()
			);
	}

	// 직업을 바꾸면 그 직업의 기본 구성으로 통째로 교체한다.
	// 이전 직업의 스킬 ID 를 남기면 새 직업 목록에 없어서 매치에서 거절된다.
	FHBStartingLoadout NewLoadout;
	FText Error;

	if (!MakeDefaultStartingLoadout(Job, NewLoadout, Error))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | 직업 '%s' 의 기본 구성을 만들지 못했다: %s"),
			*Job->JobId.ToString(),
			*Error.ToString()
			);
		return;
	}

	if (!LobbyPlayerState->TrySetStartingLoadout(NewLoadout, Error))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | 직업 '%s' 확정 실패: %s"),
			*Job->JobId.ToString(),
			*Error.ToString()
			);
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Lobby | Player: %s | SelectedJob: %s"),
		*LobbyPlayerState->GetPlayerName(),
		*Job->JobId.ToString()
		);
}

void AHBLobbyPlayerController::ServerApplySkillLoadout_Implementation(const FHBStartingLoadout& InLoadout)
{
	AHBLobbyPlayerState* LobbyPlayerState = GetPlayerState<AHBLobbyPlayerState>();

	if (!IsValid(LobbyPlayerState))
	{
		return;
	}

	if (LobbyPlayerState->IsReady())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | 준비 상태에서는 스킬을 바꿀 수 없다. Player: %s"),
			*LobbyPlayerState->GetPlayerName()
			);
		return;
	}

	if (LobbyPlayerState->IsStartConfirmed())
	{
		return;
	}

	// 이 RPC 는 스킬만 바꾼다. 클라가 직업을 같이 보내도 무시하고 현재 확정
	// 직업으로 덮는다. 직업 변경 경로가 SelectJob 과 둘로 갈리면 어느 쪽이
	// 이겼는지 추적할 수 없다.
	FHBStartingLoadout Sanitized = InLoadout;
	Sanitized.Job = LobbyPlayerState->GetSelectedJob();

	FText Error;

	if (!LobbyPlayerState->TrySetStartingLoadout(Sanitized, Error))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Lobby | 스킬 구성 거절: %s"),
			*Error.ToString()
			);
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Lobby | Player: %s | Skills | Q=%s E=%s P=%s"),
		*LobbyPlayerState->GetPlayerName(),
		*Sanitized.SkillQId.ToString(),
		*Sanitized.SkillEId.ToString(),
		*Sanitized.PassiveSkillId.ToString()
		);
}

void AHBLobbyPlayerController::HBGiveItem(FName RowName, int32 Count)
{
	ServerGiveItem(RowName, FMath::Max(1, Count));
}

void AHBLobbyPlayerController::HBGiveRecipe(FName RecipeOrResultRow, int32 Times)
{
	ServerGiveRecipe(RecipeOrResultRow, FMath::Max(1, Times));
}

void AHBLobbyPlayerController::ServerGiveItem_Implementation(FName RowName, int32 Count)
{
#if !UE_BUILD_SHIPPING
	const AHB_PlayerState* HBPlayerState = GetPlayerState<AHB_PlayerState>();
	UStashComponent* Stash = HBPlayerState ? HBPlayerState->GetStash() : nullptr;
	if (Stash)
	{
		Stash->DebugGiveItemByRow(RowName, FMath::Max(1, Count));
	}
#endif
}

void AHBLobbyPlayerController::ServerGiveRecipe_Implementation(FName RecipeOrResultRow, int32 Times)
{
#if !UE_BUILD_SHIPPING
	const AHB_PlayerState* HBPlayerState = GetPlayerState<AHB_PlayerState>();
	UStashComponent* Stash = HBPlayerState ? HBPlayerState->GetStash() : nullptr;
	const UCraftComponent* Craft = HBPlayerState ? HBPlayerState->GetCraft() : nullptr;
	const UDataTable* RecipeTable = Craft ? Craft->GetRecipeTable() : nullptr;
	if (!Stash || !RecipeTable)
	{
		return;
	}

	const FCraftRecipeRow* Recipe = RecipeTable->FindRow<FCraftRecipeRow>(RecipeOrResultRow, TEXT("HBGiveRecipe"), false);
	if (!Recipe)
	{
		for (const TPair<FName, uint8*>& Pair : RecipeTable->GetRowMap())
		{
			const FCraftRecipeRow* Candidate = reinterpret_cast<const FCraftRecipeRow*>(Pair.Value);
			if (Candidate && Candidate->ResultItem.RowName == RecipeOrResultRow)
			{
				Recipe = Candidate;
				break;
			}
		}
	}
	if (!Recipe)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GiveRecipe] '%s' not found (recipe row or result item row)"), *RecipeOrResultRow.ToString());
		return;
	}

	const int32 SafeTimes = FMath::Max(1, Times);
	if (!Recipe->RecipeItem.RowName.IsNone())
	{
		Stash->DebugGiveItemByRow(Recipe->RecipeItem.RowName, 1);
	}
	for (const FCraftIngredient& Ingredient : Recipe->Ingredients)
	{
		Stash->DebugGiveItemByRow(Ingredient.ItemRow.RowName, Ingredient.Count * SafeTimes);
	}
	if (Recipe->GoldCost > 0)
	{
		Stash->DebugGiveItemByRow(TEXT("ITM_002"), Recipe->GoldCost * SafeTimes);
	}
#endif
}
