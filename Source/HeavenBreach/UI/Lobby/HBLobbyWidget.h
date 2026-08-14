// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HBLobbyWidget.generated.h"

class UButton;
class AHBLobbyGameState;
/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UHBLobbyWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ReadyButton;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartMatchButton;
	
private:
	UFUNCTION()
	void HandleReadyClicked();
	
	UFUNCTION()
	void HandleStartMatchClicked();
	
	UFUNCTION()
	void HandleCanStartMatchChanged(bool bCanStartMatch);
	
	UPROPERTY()
	TObjectPtr<AHBLobbyGameState> LobbyGameState;
};
