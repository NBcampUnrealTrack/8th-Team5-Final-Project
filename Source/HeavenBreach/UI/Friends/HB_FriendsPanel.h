// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Friends/HB_FriendsTypes.h"
#include "HB_FriendsPanel.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UHB_FriendEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHBFriendsPanelSignal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHBFriendsPanelAction, const FString&, UserId);

/**
 * 친구 목록 팝업. 로비 화면(WBP_Lobby) 위에 얹어 두고 친구 버튼으로 열고 닫는다.
 *
 * 이 위젯은 보여주기만 한다. Steam 에서 친구를 읽어 오는 일, 초대와 참가는 하지 않는다.
 * 연결하는 쪽이 할 일은 둘이다.
 *   1. OnRefreshRequested 를 받으면 친구 목록을 읽어 SetFriends 로 넘긴다.
 *   2. OnInviteRequested / OnJoinRequested 를 받아 실제 초대와 참가를 한다.
 *
 * 열고 닫기는 Open / Close / Toggle (BlueprintCallable) 이다.
 * 친구 버튼의 OnClicked 에서 Toggle 을 부르면 된다.
 */
UCLASS()
class HEAVENBREACH_API UHB_FriendsPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 팝업을 연다. 열릴 때 한 번 OnRefreshRequested 가 나가고 로딩 상태가 된다. */
	UFUNCTION(BlueprintCallable, Category = "Friends")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "Friends")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "Friends")
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "Friends")
	bool IsOpen() const;

	/** 친구 목록을 통째로 교체한다. 정렬은 이 안에서 한다. 로딩 상태도 풀린다. */
	UFUNCTION(BlueprintCallable, Category = "Friends")
	void SetFriends(const TArray<FHBFriendListItem>& Friends);

	/** 목록을 읽어 오는 중인지 표시한다. 연결 쪽이 읽기를 시작하고 끝낼 때 부를 수 있다. */
	UFUNCTION(BlueprintCallable, Category = "Friends")
	void SetLoading(bool bLoading);

	/** 목록을 새로 읽어 달라는 요청. 열릴 때와 새로고침 버튼을 누를 때 나간다. */
	UPROPERTY(BlueprintAssignable, Category = "Friends|Events")
	FHBFriendsPanelSignal OnRefreshRequested;

	UPROPERTY(BlueprintAssignable, Category = "Friends|Events")
	FHBFriendsPanelAction OnInviteRequested;

	UPROPERTY(BlueprintAssignable, Category = "Friends|Events")
	FHBFriendsPanelAction OnJoinRequested;

	UPROPERTY(BlueprintAssignable, Category = "Friends|Events")
	FHBFriendsPanelSignal OnClosed;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	/** 친구 줄이 쌓일 곳 (VerticalBox). 스크롤이 필요하면 ScrollBox 안에 둔다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> FriendListContainer;

	/** 친구가 하나도 없을 때만 보인다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Empty;

	/** 읽어 오는 동안만 보인다. 글자든 스로버든 아무 위젯이나 된다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LoadingIndicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Refresh;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Close;

	/** 친구 한 줄로 만들 위젯. WBP_FriendEntry 를 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	TSubclassOf<UHB_FriendEntryWidget> FriendEntryClass;

	/** 줄 사이 위아래 간격(상하 합). */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	float EntrySpacing = 8.0f;

	/**
	 * 디자이너 미리보기에서 가짜 친구 몇 명을 채워 보여준다. 실행 중에는 쓰이지 않는다.
	 * 연결 없이 모양을 잡을 때 켜 둔다.
	 */
	UPROPERTY(EditAnywhere, Category = "UI Preview")
	bool bPreviewSampleData = true;

private:
	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleEntryInvite(const FString& UserId);

	UFUNCTION()
	void HandleEntryJoin(const FString& UserId);
};
