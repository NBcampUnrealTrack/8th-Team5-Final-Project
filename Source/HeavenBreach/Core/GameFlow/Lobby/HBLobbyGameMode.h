// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "HBLobbyGameMode.generated.h"

class UWorld;
class UHBJobData;
/**
 *
 */
UCLASS()
class HEAVENBREACH_API AHBLobbyGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AHBLobbyGameMode();
	virtual void StartPlay() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	/** Shared anchor lookup for authority spawning and owning-client seat updates. */
	static AActor* FindLobbySeat(UWorld* World, int32 SeatIndex);
	
	void EvaluateCanStartMatch();
	void HandleReadyStateChanged();
	void ConfirmStart(class AHBLobbyPlayerState* RequestingPlayerState);
	void TryStartGame();

	/** Authority-only internal start API; does not check UI Ready/confirmation. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Travel")
	bool RequestMatchTravel(TSoftObjectPtr<UWorld> Destination);

	/**
	 * JobId 로 직업 DA 를 찾는다. 못 찾으면 nullptr 이고 경고를 남긴다.
	 *
	 * 왜 필요한가: 직업 선택 UI(UHB_CharacterSelectionWidget)는 버튼이 하드코딩돼
	 * 있고 FName 만 서버로 보낸다. 그 이름을 실제 DA 로 바꿀 표가 서버 어딘가에는
	 * 있어야 한다.
	 *
	 * 왜 여기인가: 이 변환이 필요한 곳은 로비의 ServerSelectJob 하나뿐이다.
	 * 매치 쪽은 PlayerState 가 DA 포인터를 그대로 들고 건너오므로 변환이 없다.
	 * 로비 서버에서만 쓰는 표라 로비 GameMode 가 자리다.
	 *
	 * 쳐낼 조건: 직업 UI 가 DA 를 직접 다루게 되면 이 배열째로 사라진다.
	 */
	UFUNCTION(BlueprintPure, Category = "Lobby|Jobs")
	UHBJobData* FindJobById(FName JobId) const;

	/** 아무것도 안 골랐을 때 쓸 직업. SelectableJobs 의 첫 유효 항목이다. */
	UFUNCTION(BlueprintPure, Category = "Lobby|Jobs")
	UHBJobData* GetDefaultJob() const;

protected:
	/** Automatic Steam hub hosting; disable for an explicitly managed lobby. */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Hub")
	bool bAutomaticallyHostInviteHub = true;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Match destination configured in the lobby GameMode Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Travel")
	TSoftObjectPtr<UWorld> GameplayMap;

	/**
	 * 이 로비에서 고를 수 있는 직업들. BP_HBLobbyGameMode 에서 채운다.
	 *
	 * 작동방식: 순서가 곧 우선순위다. 첫 항목이 기본 직업이 된다.
	 * 등록하지 않은 직업은 UI 에 버튼이 있어도 서버가 거절한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Jobs")
	TArray<TObjectPtr<UHBJobData>> SelectableJobs;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FHBLobbySeatsTest;
#endif
	void RefreshLobbySeats(APlayerController* Joining = nullptr, AController* Exiting = nullptr);

	/**
	 * 입장한 플레이어에게 기본 시작 구성을 넣는다.
	 *
	 * 왜 필요한가: 직업 선택 UI 가 아직 로비에 배치되지 않았다. 아무것도 안 하면
	 * 모든 플레이어가 직업 없이 매치에 들어가 스킬을 하나도 못 받는다.
	 *
	 * 작동방식: SelectableJobs 의 첫 직업과 그 DA 의 InitialSkillIds 로 구성을
	 * 만들어 PlayerState 에 확정한다. 나중에 플레이어가 직업을 바꾸면
	 * ServerSelectJob 이 통째로 교체한다.
	 *
	 * 쳐낼 조건: 직업 선택을 반드시 거치게 하는 기획이 되면 이 기본값은 없앤다.
	 */
	void InitializeLobbyLoadout(APlayerController* NewPlayer);

	void ResetStartConfirmations();
	bool IsHostStartConfirmed() const;

};
