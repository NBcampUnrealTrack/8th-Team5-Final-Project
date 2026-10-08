// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Friends/HB_FriendsTypes.h"
#include "HB_FriendEntryWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHBFriendEntryAction, const FString&, UserId);

/**
 * 친구 목록의 한 줄. 이름, 상태, 초대/참가 버튼을 보여준다.
 * 눌렀다는 사실만 알린다. 실제 초대와 참가는 이 위젯이 하지 않는다.
 */
UCLASS()
class HEAVENBREACH_API UHB_FriendEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetItem(const FHBFriendListItem& InItem);

	const FHBFriendListItem& GetItem() const { return Item; }

	UPROPERTY(BlueprintAssignable, Category = "Friends|Events")
	FHBFriendEntryAction OnInviteClicked;

	UPROPERTY(BlueprintAssignable, Category = "Friends|Events")
	FHBFriendEntryAction OnJoinClicked;

protected:
	virtual void NativeOnInitialized() override;

	/**
	 * 상태가 정해질 때마다 불린다. WBP 그래프에서 구현해 상태 점의 색을 바꾸는 식으로 쓴다.
	 * 예) InLobby 면 초록, Offline 이면 회색.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Friends|Events")
	void OnStatusApplied(EHBFriendStatus Status);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Invite;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Join;

	/** 오프라인 친구 줄 전체의 불투명도. */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OfflineOpacity = 0.4f;

private:
	UFUNCTION()
	void HandleInviteClicked();

	UFUNCTION()
	void HandleJoinClicked();

	static FText GetStatusText(EHBFriendStatus Status);

	FHBFriendListItem Item;
};
