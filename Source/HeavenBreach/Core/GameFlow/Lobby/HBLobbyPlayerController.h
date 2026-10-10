// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/Friends/HB_FriendsTypes.h"
#include "HBLobbyPlayerController.generated.h"

class UHBLobbyHUDWidget;
class UHBLobbyWidget;
class UGameViewportClient;
struct FHBStartingLoadout;
/**
 *
 */
UCLASS()
class HEAVENBREACH_API AHBLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	UFUNCTION(Client, Reliable)
	void ClientLobbyClosed();
	UFUNCTION(BlueprintCallable, Category = "Lobby|Ready")
	void SetReady(bool bNewReady);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestStartGame();

	/** Compatibility entry point used by existing UI; the listen host validates the caller and guest readiness. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Match")
	bool RequestStartDedicatedMatch();

	/** JobId must match a UHBJobData::JobId (e.g. "Warrior", "Mage", "Archer"). The server re-validates it. */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Job")
	void SelectJob(FName JobId);

	/**
	 * 스킬 슬롯만 바꾼다. 직업은 서버가 현재 확정값으로 덮는다.
	 *
	 * 왜 직업을 안 받나: 직업 변경 경로가 SelectJob 과 둘로 갈리면 어느 쪽이
	 * 이겼는지 추적할 수 없다. 직업은 SelectJob 하나로만 바꾼다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby|Skills")
	void ApplySkillLoadout(const FHBStartingLoadout& InLoadout);

	UFUNCTION(Exec)
	void HBGiveItem(FName RowName, int32 Count = 1);

	UFUNCTION(Exec)
	void HBGiveRecipe(FName RecipeOrResultRow, int32 Times = 1);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerSetReady(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartGame();

	UFUNCTION(Server, Reliable)
	void ServerSelectJob(FName JobId);

	UFUNCTION(Server, Reliable)
	void ServerApplySkillLoadout(const FHBStartingLoadout& InLoadout);

	UFUNCTION(Server, Reliable)
	void ServerGiveItem(FName RowName, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerGiveRecipe(FName RecipeOrResultRow, int32 Times);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|UI")
	TSubclassOf<UHBLobbyHUDWidget> LobbyWidgetClass;
	
private:
	void RefreshLobbyUI();
	void UpdateFriendsPanel(bool bForce = false);
	UFUNCTION()
	void HandleReadyRequested();
	UFUNCTION()
	void HandleStartRequested();
	UFUNCTION()
	void HandleFriendsRefresh();
	UFUNCTION()
	void HandleFriendInvite(const FString& UserId);
	UFUNCTION()
	void HandleFriendJoin(const FString& UserId);
	FTimerHandle LobbyUITimer;
	TArray<FHBFriendListItem> DisplayedFriends;
	bool bWaitingForFriends = false;
	double NextFriendsRefresh = 0.;

	UPROPERTY(Transient)
	TObjectPtr<UHBLobbyWidget> LobbyContent;

	// Client-side diagnostics use replicated state; no extra server RPCs.
	struct FLobbyDebugPlayer
	{
		FString Name;
		bool bReady = false;
	};
	void RefreshLobbyDebug();
	void ShowLobbyDebugEvent(const FString& Message);
	FTimerHandle LobbyDebugTimer;
	TMap<uint32, FLobbyDebugPlayer> LobbyDebugPlayers;
	bool bLobbyDebugInitialized = false;

	// The old controller can lose its LocalPlayer before EndPlay during a swap.
	TWeakObjectPtr<UGameViewportClient> LobbyInputViewport;

	UPROPERTY()
	TObjectPtr<UHBLobbyHUDWidget> LobbyWidget;
};
