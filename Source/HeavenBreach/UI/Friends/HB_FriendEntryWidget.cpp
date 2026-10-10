// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Friends/HB_FriendEntryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UHB_FriendEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Invite)
	{
		Button_Invite->OnClicked.AddUniqueDynamic(this, &UHB_FriendEntryWidget::HandleInviteClicked);
	}
	if (Button_Join)
	{
		Button_Join->OnClicked.AddUniqueDynamic(this, &UHB_FriendEntryWidget::HandleJoinClicked);
	}
}

FText UHB_FriendEntryWidget::GetStatusText(EHBFriendStatus Status)
{
	switch (Status)
	{
	case EHBFriendStatus::Online:
		return NSLOCTEXT("Friends", "StatusOnline", "온라인");
	case EHBFriendStatus::InMenu:
		return NSLOCTEXT("Friends", "StatusInMenu", "메뉴");
	case EHBFriendStatus::InLobby:
		return NSLOCTEXT("Friends", "StatusInLobby", "로비");
	case EHBFriendStatus::InMatch:
		return NSLOCTEXT("Friends", "StatusInMatch", "게임 중");
	case EHBFriendStatus::Offline:
	default:
		return NSLOCTEXT("Friends", "StatusOffline", "오프라인");
	}
}

void UHB_FriendEntryWidget::SetItem(const FHBFriendListItem& InItem)
{
	Item = InItem;

	if (Text_Name)
	{
		Text_Name->SetText(Item.DisplayName);
	}
	if (Text_Status)
	{
		Text_Status->SetText(GetStatusText(Item.Status));
	}

	// 못 누르는 동작은 숨기지 않고 비활성화한다. 숨기면 줄마다 버튼 위치가 달라진다.
	if (Button_Invite)
	{
		Button_Invite->SetIsEnabled(Item.bCanInvite);
	}
	if (Button_Join)
	{
		Button_Join->SetIsEnabled(Item.bCanJoin);
	}

	// 오프라인 친구는 줄 전체를 흐리게 한다.
	SetRenderOpacity(Item.Status == EHBFriendStatus::Offline ? OfflineOpacity : 1.f);

	OnStatusApplied(Item.Status);
}

void UHB_FriendEntryWidget::HandleInviteClicked()
{
	OnInviteClicked.Broadcast(Item.UserId);
}

void UHB_FriendEntryWidget::HandleJoinClicked()
{
	OnJoinClicked.Broadcast(Item.UserId);
}
