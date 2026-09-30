#pragma once

#include "CoreMinimal.h"

struct FItemInstance;
struct FItemEngraving;
class UDataTable;
class FJsonObject;

// 랜덤 각인 규칙. 굴리는 곳은 여기 하나다.
//
// 원칙
//   숫자는 전부 DT_Engrave 에서 온다 (개수 · 범위 · 가중치 · 부위). 여기에 숫자를 박지 않는다.
//   한 번 발린 각인은 다시 굴리지 않는다 (bEngraved). 재각인 기획은 없다.
//   서버에서만 부른다. 클라가 부르면 서버와 다른 값이 나온다.
//
// 부르는 곳 (3단계에서 연결)
//   UInventoryComponent 입구 셋 — TryAddItem · TryAddItemAt · SetEntries
//   장착 장비 불러오기 — AHB_PlayerState::HandleLoadoutLoaded
namespace ItemEngraving
{
	// 아직 안 발렸으면 굴려서 바른다. 새로 발랐으면 true — 부르는 쪽이 저장을 요청한다.
	//
	// 아무것도 안 하는 경우 (false)
	//   이미 발렸다 / 각인 대상이 아니다 (악세서리 · 소모품) / 표가 없다 / 그 등급 행이 없다
	//
	// 표나 행이 없을 때 "발랐음" 으로 찍지 않는 이유:
	//   행 이름을 잘못 적은 채로 한 번 불러오면 그 등급 아이템이 전부 0개로 굳어 저장된다.
	//   안 찍어 두면 행을 고친 뒤 다음에 불러올 때 발린다.
	bool EnsureRolled(FItemInstance& Item, const UDataTable* EngraveTable);

	// ===== 저장 모양 =====
	// 창고 · 로드아웃 저장이 같이 쓴다. 각인의 JSON 모양은 여기 하나에서만 정한다.
	// 안 발린 아이템은 칸을 안 쓴다. 읽을 때 칸이 없으면 안 발린 것 — 불러온 뒤 입구에서 굴린다.
	void WriteJson(const FItemEngraving& Engraving, const TSharedRef<FJsonObject>& ItemObject);
	void ReadJson(const TSharedPtr<FJsonObject>& ItemObject, FItemEngraving& OutEngraving);
}
