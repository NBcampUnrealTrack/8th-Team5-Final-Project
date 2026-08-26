// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HBGameStartMenuWidget.generated.h"

class UButton;
/**
 * 
 */
UCLASS()
class HEAVENBREACH_API UHBGameStartMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton;
	
	UPROPERTY(meta = (BindWIdget))
	TObjectPtr<UButton> ExitButton;
	
private:
	UFUNCTION()
	void HandleStartClicked();
	
	UFUNCTION()
	void HandleExitClicked();
	
};
