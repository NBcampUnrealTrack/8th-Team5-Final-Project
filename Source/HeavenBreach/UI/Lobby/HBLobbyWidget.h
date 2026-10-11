// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HBLobbyWidget.generated.h"

class UButton;
class UHB_FriendsPanel;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHBLobbyActionRequested);
/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UHBLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
#pragma region LobbyActionRequests
	/** The owning lobby controller handles these requests. */
	UPROPERTY(BlueprintAssignable, Category = "Lobby|Actions")
	FHBLobbyActionRequested OnStartRequested;

	UFUNCTION(BlueprintPure, Category = "Lobby|Actions")
	bool IsLocalLobbyHost() const;

	UFUNCTION(BlueprintCallable, Category = "Lobby|Actions")
	void RefreshLobbyActions();

	UHB_FriendsPanel* GetFriendsPanel() const { return FriendsPanel; }

	/** Designer preview only; runtime uses the current Steam lobby ownership. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby|Preview")
	bool bPreviewAsHost = true;
#pragma endregion
	
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
#pragma region LobbyActionButtons
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartMatchButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> ReadyButtonModule;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> StartMatchButtonModule;
#pragma endregion

#pragma region Friends
	/** WBP_Lobby_FriendsButton. 안에 있는 버튼을 눌렀을 때 친구 목록을 연다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> FriendsButtonModule;

	/** 이 화면 위에 얹어 둔 WBP_FriendsPanel. 친구 버튼으로 열고 닫는다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHB_FriendsPanel> FriendsPanel;
#pragma endregion
	
private:
	// FriendsButtonModule 안의 버튼. 모듈 내부 구조에 의존하지 않으려고 이름이 아니라 타입으로 찾는다.
	UPROPERTY(Transient)
	TObjectPtr<UButton> FriendsButton;

	UFUNCTION()
	void HandleFriendsClicked();

	UFUNCTION()
	void HandleStartClicked();
};
