#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUsed, FName, ItemId, float, HealAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUseBegan, FGuid, InstanceId, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemRevealed);

class UDataTable;
class UEquipmentComponent;
class AItemBase;

// 아이템 종류(ItemRow.RowName)별 마지막 사용 시각(서버 시간). 쿨타임을 퀵슬롯 버튼이 아니라
// "아이템 종류" 단위로 추적해서, 서로 다른 포션은 독립적으로 쿨타임이 돌고(3번 슬롯 포션을 써도
// 4번 슬롯의 다른 포션엔 영향 없음), 같은 포션이면 퀵슬롯 3/4번 어디서 쓰든 인벤토리에서 직접
// 쓰든 하나의 쿨타임을 공유한다.
USTRUCT()
struct FItemCooldownEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemId;

	UPROPERTY()
	float LastUseTime = 0.f;
};

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemUsed OnItemUsed;

	/**
	 * 소모품 사용이 "시작"됐다. 아직 소모·쿨타임·회복은 일어나지 않았다.
	 * 듣는 쪽(캐릭터의 포션 음용)이 끝나면 CommitItemUse, 끊기면 CancelItemUse 를 불러야 한다.
	 * 아무도 안 들으면 UseItem 이 예전처럼 그 자리에서 바로 소모한다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemUseBegan OnItemUseBegan;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemRevealed OnItemRevealed;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void InitializeGrid(int32 InWidth, int32 InHeight);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FIntPoint GetGridSize() const { return FIntPoint(Width, Height); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FInventoryEntry>& GetEntries() const { return Entries; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsValidCoord(FIntPoint InCoord) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetEntryIndexAt(FIntPoint InCoord) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindEntryIndexById(FGuid InInstanceId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetTotalGold(FName InGoldRowName) const;

	bool ConsumeGold(const FDataTableRowHandle& InGoldRow, int32 InAmount);

	bool AddGold(const FDataTableRowHandle& InGoldRow, int32 InAmount);

	bool CanPlaceAt(FIntPoint InSize, FIntPoint InPosition, int32 InIgnoredIndex = INDEX_NONE) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanFitItem(const FItemInstance& InItem) const;

	bool FindFirstFreeSlot(FIntPoint InSize, FIntPoint& OutPosition, int32 InIgnoredIndex = INDEX_NONE) const;

	bool TryAddItem(const FItemInstance& InItem, int32& OutRemainder);

	bool TryAddItemAt(const FItemInstance& InItem, FIntPoint InPosition);

	bool MoveItem(FGuid InInstanceId, FIntPoint InNewPosition, bool bInRotated);

	bool RemoveItem(FGuid InInstanceId, FItemInstance& OutRemoved);

	bool RemoveStackCount(FGuid InInstanceId, int32 InCount);

	/** 소모품 사용 요청. 검사를 통과하면 OnItemUseBegan 을 보내고 true. 실제 소모는 CommitItemUse 에서 한다. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseItem(FGuid InInstanceId);

	/** 시작된 사용을 마무리한다. 쿨타임 → OnItemUsed → 1개 소모. 그 사이 아이템이 사라졌으면 false. */
	bool CommitItemUse(FGuid InInstanceId);

	/** 시작된 사용을 없던 일로 한다. 아무것도 소모하지 않는다. */
	void CancelItemUse(FGuid InInstanceId);
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ToggleFavorite(FGuid InInstanceId);

	/** InItemId(ItemRow.RowName) 아이템 사용 쿨타임 남은 시간(초). 쿨타임 중이 아니면 0.
	 * 쿨타임은 아이템 종류별로 독립이라 슬롯이 아니라 "무슨 아이템인지"를 넘겨줘야 한다.
	 * UI 갱신용으로 매 프레임 호출해도 되는 가벼운 계산. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetItemUseCooldownRemaining(FName InItemId) const;

	/** InItemId 아이템 사용 쿨타임 진행률. 1(방금 사용, 꽉 참) -> 0(재사용 가능). 쿨타임 진행바 Percent에 그대로 꽂아 쓰면 된다. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetItemUseCooldownPercent(FName InItemId) const;

	/** 아이템 사용 쿨타임 총 길이(초). 기본 6초. 모든 아이템 종류가 같은 길이를 공유한다(길이만 공용, 타이머는 종류별 독립). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetItemUseCooldownDuration() const { return ItemUseCooldown; }

	/** 서칭 안 된 아이템(FItemInstance::bRevealed == false)이 하나라도 있으면, 일정 간격으로
	 * 하나씩 순서대로 공개(bRevealed = true)하는 타이머를 시작한다.
	 * 상자/몬스터 시체 같은 루팅 컨테이너를 열 때(Interact) 서버에서 호출한다.
	 * 이미 타이머가 돌고 있으면 아무것도 하지 않는다 - 화면을 닫았다가 다시 열어도 처음부터
	 * 다시 돌지 않고 이어서 진행된다. 전부 이미 공개된 상태면 타이머를 시작하지 않는다. */
	void BeginItemRevealSequence();

	/** 서칭 타이머를 멈춘다(서버 전용). 진행 상태는 그대로 남는다 - 이미 공개된 아이템은
	 * bRevealed=true 그대로, 안 된 아이템도 bRevealed=false 그대로 유지된다. 화면을 닫을 때
	 * 호출하고, 다시 열 때 BeginItemRevealSequence()를 부르면 멈춘 지점부터 이어서 진행된다.
	 * (타르코프처럼 "열어보고 있는 동안만" 서칭이 진행되게 하기 위한 것 - 화면을 닫아두면
	 * 백그라운드에서 계속 진행되지 않는다.) */
	void PauseItemRevealSequence();

	bool TransferItemFrom(UInventoryComponent* InSource, FGuid InInstanceId, FIntPoint InTargetPosition, bool bInRotated);

	bool TransferItemFromAuto(UInventoryComponent* InSource, FGuid InInstanceId);

	bool MergeStack(FGuid InSourceId, FGuid InTargetId);

	bool MergeStackFrom(UInventoryComponent* InSource, FGuid InSourceId, FGuid InTargetId);

	bool DropItem(FGuid InInstanceId);

	bool DropEquippedItem(UEquipmentComponent* InEquipment, EEquipSlotType InSlot);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearAll();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugPrintGrid() const;

	UFUNCTION(BlueprintCallable, Exec, Category = "Inventory|Debug")
	void DebugAddItem(FName InRowName, int32 InCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugMoveFirstItem(FIntPoint InNewPosition);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	void DebugRemoveFirstItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void Server_MoveItem(FGuid InInstanceId, FIntPoint InNewPosition, bool InRotated);

	UFUNCTION(Server, Reliable)
	void Server_TransferItem(UInventoryComponent* InTarget, FGuid InInstanceId, FIntPoint InTargetPosition, bool InRotated);

	UFUNCTION(Server, Reliable)
	void Server_TransferItemAuto(UInventoryComponent* InTarget, FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_EquipItem(UEquipmentComponent* InEquipment, FGuid InInstanceId, EEquipSlotType InSlot);

	UFUNCTION(Server, Reliable)
	void Server_UnequipItem(UEquipmentComponent* InEquipment, EEquipSlotType InSlot, FIntPoint InTargetPosition);

	UFUNCTION(Server, Reliable)
	void Server_TransferItemFrom(UInventoryComponent* InSource, FGuid InInstanceId, FIntPoint InTargetPosition, bool InRotated);

	UFUNCTION(Server, Reliable)
	void Server_TransferItemFromAuto(UInventoryComponent* InSource, FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_MoveItemIn(UInventoryComponent* InTarget, FGuid InInstanceId, FIntPoint InNewPosition, bool InRotated);

	UFUNCTION(Server, Reliable)
	void Server_UnequipItemTo(UEquipmentComponent* InEquipment, EEquipSlotType InSlot, UInventoryComponent* InTarget, FIntPoint InTargetPosition);

	void SetGridSize(int32 InWidth, int32 InHeight);

	void SetEntries(const TArray<FInventoryEntry>& InEntries);

	// 아직 각인이 안 발린 장비면 이 컴포넌트의 각인 표로 굴린다. 새로 발랐으면 true. 서버에서만 동작한다.
	// 입구(TryAddItem · TryAddItemAt · SetEntries)가 알아서 부른다.
	// 밖에서 부르는 곳은 불러오기(창고 · 로드아웃)뿐이다 — 새로 발린 게 있어야 저장을 요청할 수 있어서.
	bool EnsureEngraved(FItemInstance& InOutItem) const;

	UFUNCTION(Server, Reliable)
	void Server_UseItem(FGuid InInstanceId);
	UFUNCTION(Server, Reliable)
	void Server_ToggleFavorite(FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_PauseContainerReveal(UInventoryComponent* InTargetContainer);

	UFUNCTION(Server, Reliable)
	void Server_SplitStack(FGuid InInstanceId, int32 InQuantity, FIntPoint InTargetPosition);

	UFUNCTION(Server, Reliable)
	void Server_MergeStack(FGuid InSourceId, FGuid InTargetId);

	UFUNCTION(Server, Reliable)
	void Server_MergeStackFrom(UInventoryComponent* InSource, FGuid InSourceId, FGuid InTargetId);

	UFUNCTION(Server, Reliable)
	void Server_DropItem(FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_DropItemFrom(UInventoryComponent* InSource, FGuid InInstanceId);

	UFUNCTION(Server, Reliable)
	void Server_DropEquippedItem(UEquipmentComponent* InEquipment, EEquipSlotType InSlot);


protected:

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Entries();

private:
	UFUNCTION(Client, Reliable)
	void Client_PlayActionResult(bool bSuccess, bool bEquipment);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnItemRevealed();

	void RebuildCache();

	void MarkCells(int32 InEntryIndex, FIntPoint InPosition, FIntPoint InSize);

	int32 CoordToIndex(FIntPoint InCoord) const { return InCoord.Y * Width + InCoord.X; }

	bool GetEntrySize(const FItemInstance& InItem, FIntPoint& OutSize) const;

	bool SpawnDroppedItemActor(const FItemInstance& InItem);

	bool HasMergeableSpace(FName InRowName, int32 InMaxStack) const;

	/** InItemId 쿨타임을 지금(InServerTime)으로 갱신. 기존 기록 있으면 덮어쓰고, 없으면 새로 추가. */
	void SetItemCooldown(FName InItemId, float InServerTime);

	/** 사용 가능 여부만 본다(존재, 소모품, 쿨타임, 최대 체력). 상태는 바꾸지 않는다. */
	bool CanUseItem(FGuid InInstanceId, FItemInstance& OutItem, FItemViewData& OutViewData) const;

	// UseItem 으로 시작돼 Commit/Cancel 을 기다리는 아이템. 서버 전용이라 복제하지 않는다.
	// 하나만 둔다: 마시는 도중 다른 소모품을 또 쓰는 것을 막는다.
	FGuid PendingUseInstanceId;

	/** ItemRevealTimerHandle이 부른다(서버 전용). 서칭 안 된 첫 아이템을 하나 공개하고,
	 * 더 이상 없으면 타이머를 스스로 멈춘다. */
	void RevealNextItem();

	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = 1, AllowPrivateAccess = "true"))
	int32 Width = 6;

	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = 1, AllowPrivateAccess = "true"))
	int32 Height = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")

	float ItemUseCooldown = 6.0f;

	// 아이템 종류별 마지막 사용 시각 기록(FItemCooldownEntry 참고). 서버에서만 갱신되는 값이라
	// 소유 클라이언트 UI(퀵슬롯/인벤토리 쿨타임 진행바)가 읽어야 해서 복제 필요.
	// 서버·클라이언트 시계가 서로 다르므로 비교 시 GetWorld()->GetTimeSeconds() 대신
	// GameState::GetServerWorldTimeSeconds()(동기화된 서버 시간)를 써야 한다 — HeavenBreachGameState의
	// MatchEndServerTime과 동일한 패턴.
	UPROPERTY(Replicated)
	TArray<FItemCooldownEntry> ItemCooldowns;

	// 아이템 하나가 ?에서 실제 아이콘으로 서칭(공개)되는 데 걸리는 시간(초). 복제 불필요 -
	// 서버가 타이머 간격으로만 쓰고, 클라이언트는 Entries 복제로 결과(bRevealed)만 받아본다.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Loot", meta = (ClampMin = 0.1))
	float ItemRevealIntervalSeconds = 1.75f;

	// 서칭 진행 타이머(서버 전용, 복제 안 함). 루팅 컨테이너를 열 때 BeginItemRevealSequence()가
	// 시작하고, 서칭 안 된 아이템이 없어지면 RevealNextItem()이 스스로 멈춘다.
	FTimerHandle ItemRevealTimerHandle;

	UPROPERTY(EditAnywhere, Category = "Inventory|Debug")
	TObjectPtr<UDataTable> DebugItemTable;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Drop")
	TSubclassOf<AItemBase> DropItemClass;

	// 랜덤 각인 표 (DT_Engrave). 생성자가 경로로 넣는다.
	// 컨테이너(플레이어 · 창고 · 시체 · 상자)가 전부 이 컴포넌트라 BP 마다 넣지 않아도 된다.
	// 특정 컨테이너만 다른 표를 써야 하면 그 BP 의 컴포넌트 디테일에서 바꾼다.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Engrave")
	TObjectPtr<UDataTable> EngraveTable;

	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FInventoryEntry> Entries;

	TArray<int32> CellCache;
};