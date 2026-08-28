// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "HBGameSession.generated.h"

/**
 * 
 */
UCLASS()
class HEAVENBREACH_API AHBGameSession : public AGameSession
{
	GENERATED_BODY()
	
public:
	virtual void RegisterServer() override;
	
private:
	void HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful);
	
	FDelegateHandle CreateSessionCompleteDelegateHandle;
};
