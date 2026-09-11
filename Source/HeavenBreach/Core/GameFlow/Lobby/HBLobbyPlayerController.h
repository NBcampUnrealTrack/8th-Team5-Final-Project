// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
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
	UFUNCTION(BlueprintCallable, Category = "Lobby|Ready")
	void SetReady(bool bNewReady);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestStartGame();

	/** Compatibility entry point used by the existing UI; the dedicated server decides whether the vote is accepted. */
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|UI")
	TSubclassOf<UHBLobbyHUDWidget> LobbyWidgetClass;
	
private:
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
