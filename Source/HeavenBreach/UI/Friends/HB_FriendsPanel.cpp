// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Friends/HB_FriendsPanel.h"
#include "UI/Friends/HB_FriendEntryWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Algo/Sort.h"

namespace
{
	// 값이 작을수록 위에 뜬다. 지금 같이 할 수 있는 친구가 위로 가고 오프라인이 맨 아래다.
	int32 StatusRank(EHBFriendStatus Status)
	{
		switch (Status)
		{
		case EHBFriendStatus::InLobby:	return 0;
		case EHBFriendStatus::InMenu:	return 1;
		case EHBFriendStatus::InMatch:	return 2;
		case EHBFriendStatus::Online:	return 3;
		case EHBFriendStatus::Offline:
		default:						return 4;
		}
	}
}

void UHB_FriendsPanel::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime() && bPreviewSampleData)
	{
		TArray<FHBFriendListItem> Samples;

		auto AddSample = [&Samples](const TCHAR* Name, EHBFriendStatus Status, bool bInvite, bool bJoin)
		{
			FHBFriendListItem Sample;
			Sample.UserId = Name;
			Sample.DisplayName = FText::FromString(Name);
			Sample.Status = Status;
			Sample.bCanInvite = bInvite;
			Sample.bCanJoin = bJoin;
			Samples.Add(Sample);
		};

		AddSample(TEXT("Alice"), EHBFriendStatus::InLobby, true, true);
		AddSample(TEXT("Bob"), EHBFriendStatus::InMenu, true, false);
		AddSample(TEXT("Carol"), EHBFriendStatus::InMatch, false, false);
		AddSample(TEXT("Dave"), EHBFriendStatus::Online, false, false);
		AddSample(TEXT("Eve"), EHBFriendStatus::Offline, false, false);

		SetFriends(Samples);
	}
}

void UHB_FriendsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.AddUniqueDynamic(this, &UHB_FriendsPanel::HandleRefreshClicked);
	}
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UHB_FriendsPanel::HandleCloseClicked);
	}

	// 로비 화면 위에 얹어 두는 팝업이라 평소에는 닫혀 있어야 한다.
	// 디자이너에서는 모양을 볼 수 있게 그대로 둔다 (NativeConstruct 는 실행 중에만 불린다).
	SetVisibility(ESlateVisibility::Collapsed);
	SetLoading(false);
}

void UHB_FriendsPanel::Open()
{
	SetVisibility(ESlateVisibility::Visible);

	// 열 때마다 새로 읽는다. 친구 상태는 금방 바뀌므로 이전 목록을 믿지 않는다.
	SetLoading(true);
	OnRefreshRequested.Broadcast();
}

void UHB_FriendsPanel::Close()
{
	if (!IsOpen())
	{
		return;
	}

	SetVisibility(ESlateVisibility::Collapsed);
	OnClosed.Broadcast();
}

void UHB_FriendsPanel::Toggle()
{
	if (IsOpen())
	{
		Close();
	}
	else
	{
		Open();
	}
}

bool UHB_FriendsPanel::IsOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

void UHB_FriendsPanel::SetLoading(bool bLoading)
{
	if (LoadingIndicator)
	{
		LoadingIndicator->SetVisibility(bLoading ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 읽어 오는 중에 "친구가 없습니다" 가 같이 뜨면 거짓말이 된다. 결과가 오면 SetFriends 가 다시 켠다.
	if (bLoading && Text_Empty)
	{
		Text_Empty->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UHB_FriendsPanel::SetFriends(const TArray<FHBFriendListItem>& Friends)
{
	SetLoading(false);

	if (Text_Empty)
	{
		Text_Empty->SetVisibility(Friends.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!FriendListContainer || !FriendEntryClass)
	{
		return;
	}

	FriendListContainer->ClearChildren();

	TArray<FHBFriendListItem> Sorted = Friends;
	Algo::Sort(Sorted, [](const FHBFriendListItem& A, const FHBFriendListItem& B)
	{
		const int32 RankA = StatusRank(A.Status);
		const int32 RankB = StatusRank(B.Status);
		if (RankA != RankB)
		{
			return RankA < RankB;
		}
		return A.DisplayName.CompareToCaseIgnored(B.DisplayName) < 0;
	});

	for (const FHBFriendListItem& Friend : Sorted)
	{
		UHB_FriendEntryWidget* Entry = CreateWidget<UHB_FriendEntryWidget>(this, FriendEntryClass);
		if (!Entry)
		{
			continue;
		}

		Entry->SetItem(Friend);
		Entry->OnInviteClicked.AddUniqueDynamic(this, &UHB_FriendsPanel::HandleEntryInvite);
		Entry->OnJoinClicked.AddUniqueDynamic(this, &UHB_FriendsPanel::HandleEntryJoin);

		if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(FriendListContainer->AddChild(Entry)))
		{
			VSlot->SetPadding(FMargin(0.f, EntrySpacing * 0.5f));
		}
	}
}

void UHB_FriendsPanel::HandleRefreshClicked()
{
	SetLoading(true);
	OnRefreshRequested.Broadcast();
}

void UHB_FriendsPanel::HandleCloseClicked()
{
	Close();
}

void UHB_FriendsPanel::HandleEntryInvite(const FString& UserId)
{
	OnInviteRequested.Broadcast(UserId);
}

void UHB_FriendsPanel::HandleEntryJoin(const FString& UserId)
{
	OnJoinRequested.Broadcast(UserId);
}
