#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InventoryTypes.h"
#include "Player/Job/HBStartingLoadout.h"
#include "HBPlayerSaveGame.generated.h"

// 한 플레이어의 저장 데이터. 로컬 저장 모드에서 각자 PC 의 Saved/SaveGames/Profile_<계정>.sav 로 남는다.
//
// 왜 USaveGame 인가: UPROPERTY 를 엔진이 알아서 저장한다. 외부 서버 JSON 처럼 칸마다 쓰고 읽는 줄이 없어서
// 아이템에 칸이 늘어도(즐겨찾기 · 각인처럼) 여기는 안 바뀐다. 옛 파일에 없는 칸은 기본값으로 읽힌다.
UCLASS()
class HEAVENBREACH_API UHBPlayerSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 파일 모양 버전. 칸 이름이나 뜻이 바뀌면 올리고, 불러올 때 옛 버전을 옮긴다.
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TArray<FInventoryEntry> Stash;

	UPROPERTY()
	TArray<FInventoryEntry> Inventory;

	UPROPERTY()
	TArray<FItemInstance> Equipment;

	UPROPERTY()
	TArray<FGuid> QuickSlots;

	// 로비에서 등록한 직업 · 스킬. 직업은 DA 포인터라 애셋 경로로 저장된다.
	UPROPERTY()
	FHBStartingLoadout StartingLoadout;
};
