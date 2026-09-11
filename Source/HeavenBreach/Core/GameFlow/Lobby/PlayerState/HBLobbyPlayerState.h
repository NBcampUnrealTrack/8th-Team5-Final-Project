// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "HBLobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnReadyStateChanged,
	bool,
	bNewReadyState
	);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnStartConfirmationChanged,
	bool,
	bNewStartConfirmed
	);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSelectedJobChanged,
	FName,
	NewSelectedJobId
	);
/**
 *
 */
UCLASS()
class HEAVENBREACH_API AHBLobbyPlayerState : public AHB_PlayerState
{
	GENERATED_BODY()

public:
	AHBLobbyPlayerState();

	bool IsReady() const { return bIsReady; }
	bool IsStartConfirmed() const { return bStartConfirmed; }

	/**
	 * 부모의 StartingLoadout.Job 에서 이름만 꺼낸다.
	 *
	 * 왜 남겼나: UHB_CharacterSelectionWidget 이 이 이름으로 버튼 강조와
	 * "현재 직업" 표시를 한다. 저장 위치가 부모로 옮겨갔어도 그 호출은 유지한다.
	 */
	FName GetSelectedJobId() const;

	bool HasSelectedJob() const { return GetSelectedJob() != nullptr; }

	void SetReady(bool bNewReady);
	void SetStartConfirmed(bool bNewStartConfirmed);

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Ready")
	FOnReadyStateChanged OnReadyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Start")
	FOnStartConfirmationChanged OnStartConfirmationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Job")
	FOnSelectedJobChanged OnSelectedJobChanged;

protected:
	UPROPERTY(
		ReplicatedUsing = OnRep_IsReady,
		BlueprintReadOnly,
		Category = "Lobby|Ready")
	bool bIsReady = false;

	/** Kept separate from ready so every player must explicitly approve the final match start. */
	UPROPERTY(
		ReplicatedUsing = OnRep_StartConfirmed,
		BlueprintReadOnly,
		Category = "Lobby|Start")
	bool bStartConfirmed = false;

	/**
	 * 부모가 시작 구성 변경을 알릴 때 직업 변경 이벤트를 덧붙인다.
	 *
	 * 왜 재정의인가: 직업 ID 필드가 부모의 StartingLoadout 으로 옮겨갔지만
	 * OnSelectedJobChanged 를 구독하는 UI 가 있다. 자식이 BeginPlay 에서
	 * 부모 델리게이트를 자가 구독하는 것보다 순서가 확실하다.
	 */
	virtual void HandleStartingLoadoutChanged() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_IsReady();

	UFUNCTION()
	void OnRep_StartConfirmed();

private:
	/** 직업이 실제로 바뀐 경우에만 알리려고 직전 값을 기억한다. */
	FName LastNotifiedJobId = NAME_None;
};
