// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "HBGameSession.generated.h"

/** Retains GameMode/BP class references; OSS sessions are owned by HBOnlineSessionSubsystem. */
UCLASS()
class HEAVENBREACH_API AHBGameSession : public AGameSession
{
	GENERATED_BODY()

public:
	virtual void RegisterServer() override;
	virtual FString ApproveLogin(const FString& Options) override;
};
