#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "Player/Job/HBStartingLoadout.h"
#include "HBPlayerSaveComponent.generated.h"

class UHBPlayerSaveGame;

// 저장 방식. 바꿀 때는 로비 · 매치 PlayerState 가 같은 값이어야 한다 (둘 다 AHB_PlayerState 라 C++ 기본값이면 같다).
UENUM()
enum class EHBSaveMode : uint8
{
	Local    UMETA(DisplayName = "로컬 (각자 PC)"),
	Backend  UMETA(DisplayName = "외부 서버 (HTTP)"),
};

// 로컬 저장 흐름 전부. AHB_PlayerState 에 붙는다.
//
//   내 PC  : 내 PlayerState 면 파일을 읽어 서버로 보낸다 → "적용 끝" 을 받은 뒤부터 바뀌면 저장
//   서버   : 받은 데이터를 검사해 PlayerState 에 적용을 맡긴다
//
// Backend 모드면 아무것도 안 한다 — 지금까지처럼 AHB_PlayerState 가 HTTP 로 불러오고 저장한다.
//
// 쳐낼 조건: 외부 서버 방식을 완전히 버리면 SaveMode 칸과 AHB_PlayerState 의 Backend 분기를 지운다.
UCLASS(ClassGroup = (Save))
class HEAVENBREACH_API UHBPlayerSaveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHBPlayerSaveComponent();

	EHBSaveMode GetSaveMode() const { return SaveMode; }
	// 로컬 소지품 복제가 도착하기 전이어도 서버가 승인한 손실을 저장한다.
	// 진행 중인 이전 저장까지 끝낸 뒤 완료를 알린다. 실제 이동은 Controller가 맡는다.
	void FlushBeforeTravel(bool bForfeitCarriedItems, TFunction<void(bool)> OnSaved);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Save")
	EHBSaveMode SaveMode = EHBSaveMode::Local;

	// 바뀐 뒤 몇 초 모아서 저장하나. 연달아 바뀔 때 파일을 매번 쓰지 않게.
	UPROPERTY(EditDefaultsOnly, Category = "Save", meta = (ClampMin = "0.1"))
	float SaveDelay = 1.f;

	// 창고를 몇 줄씩 나눠 보내나. RPC 하나에 다 실으면 크기 제한에 걸릴 수 있다.
	UPROPERTY(EditDefaultsOnly, Category = "Save", meta = (ClampMin = "1"))
	int32 StashChunkSize = 40;

private:
	// ----- 내 PC -----

	// 내 PlayerState 인지 확인되면 파일을 읽어 보낸다. 클라는 소유자가 늦게 복제될 수 있어 몇 번 다시 본다.
	void TryBeginLocalLoad();
	bool IsMine(bool& bOutUndecided) const;
	FString MakeSlotName() const;
	void SubmitToServer(const UHBPlayerSaveGame& Save);

	// "적용 끝" 뒤에만 묶는다. 그 전에는 복제된 데이터가 비어 있어서, 저장하면 파일이 빈 데이터로 덮인다.
	void BindSaveTriggers();

	UFUNCTION()
	void RequestSave();

	// bAsync = false 는 레벨 이동 · 종료 때. 비동기로 쓰면 끝나기 전에 사라질 수 있다.
	bool SaveNow(bool bAsync);
	void FinishTravelSave();
	int32 SavesInFlight = 0;
	bool bTravelSaveRequested = false;
	bool bForfeitOnSave = false;
	TFunction<void(bool)> TravelSaveCompleted;

	FTimerHandle LoadRetryTimer;
	int32 LoadRetryCount = 0;
	FTimerHandle SaveTimer;
	bool bLoadApplied = false;

	// 파일은 있는데 못 읽었다. 이때는 절대 저장하지 않는다 — 빈 데이터로 덮으면 원래 파일이 사라진다.
	bool bLoadFailed = false;

	// ----- 서버 -----

	UFUNCTION(Server, Reliable)
	void Server_BeginSubmit();

	UFUNCTION(Server, Reliable)
	void Server_SubmitStashChunk(const TArray<FInventoryEntry>& Chunk);

	UFUNCTION(Server, Reliable)
	void Server_FinishSubmit(const TArray<FInventoryEntry>& InInventory, const TArray<FItemInstance>& InEquipment,
		const TArray<FGuid>& InQuickSlots, const FHBStartingLoadout& InStartingLoadout);

	UFUNCTION(Client, Reliable)
	void Client_SubmitApplied();

	// 1차 검사. 협력 PvE 라 조작을 다 막지는 않는다. 없는 아이템과 말이 안 되는 수치만 거른다.
	// false = 없는 아이템이라 빼야 한다.
	bool SanitizeItem(FItemInstance& InOutItem, TSet<FGuid>& InOutSeenIds) const;
	void SanitizeEntries(TArray<FInventoryEntry>& InOutEntries, TSet<FGuid>& InOutSeenIds) const;

	TArray<FInventoryEntry> PendingStash;

	// 한 PlayerState 에 한 번만 받는다. 매치 중에 다시 보내 창고를 바꾸는 것을 막는다.
	bool bSubmitAccepted = false;
};
