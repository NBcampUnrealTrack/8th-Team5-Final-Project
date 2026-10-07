// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HB_FriendsTypes.generated.h"

/**
 * 친구가 지금 어떤 상태인지. 친구 목록 UI 가 표시하는 용도다.
 *
 * 왜 UI 쪽에 따로 두나: 세션 서브시스템의 FHBSteamFriendEntry 는 Steam 쪽 사정에 맞춘
 * 구조라 UI 가 직접 읽으면 서브시스템이 바뀔 때 UI 도 같이 깨진다. UI 는 이 구조체만 안다.
 * 서브시스템 값을 이 구조체로 옮겨 담는 일은 연결을 맡은 쪽이 한다.
 */
UENUM(BlueprintType)
enum class EHBFriendStatus : uint8
{
	// Steam 에 접속하지 않았다. 목록에서 흐리게 보인다.
	Offline,
	// Steam 은 켜져 있지만 이 게임은 하지 않는다.
	Online,
	// 이 게임의 메뉴에 있다.
	InMenu,
	// 이 게임의 방 안에 있다.
	InLobby,
	// 이 게임의 매치 중이다.
	InMatch
};

/** 친구 목록 한 줄이 필요로 하는 값 전부. */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBFriendListItem
{
	GENERATED_BODY()

	// 초대/참가 요청을 돌려보낼 때 쓰는 식별자 (Steam ID 문자열).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	FString UserId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	EHBFriendStatus Status = EHBFriendStatus::Offline;

	// 지금 초대할 수 있는가. false 면 초대 버튼이 비활성화된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	bool bCanInvite = false;

	// 지금 이 친구의 방에 참가할 수 있는가. false 면 참가 버튼이 비활성화된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	bool bCanJoin = false;
};
