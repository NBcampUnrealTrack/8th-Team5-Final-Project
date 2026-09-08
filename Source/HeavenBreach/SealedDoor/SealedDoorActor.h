// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "SealedDoorTypes.h"
#include "SealedDoorActor.generated.h"

class UStaticMeshComponent;
class UDataTable;
class UInventoryComponent;

/**
 * 등급이 있는 봉인문.
 *
 * 같은 등급의 천계 봉인 열쇠로만 열리며, 개방 시 열쇠 1개를 소비한다.
 * 한 번 열린 문은 세션이 끝날 때까지 다시 닫히지 않는다.
 */
UCLASS(Blueprintable)
class HEAVENBREACH_API ASealedDoorActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:

	ASealedDoorActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UFUNCTION(BlueprintPure, Category = "SealedDoor")
	ESealedDoorGrade GetGrade() const { return Grade; }

	UFUNCTION(BlueprintPure, Category = "SealedDoor")
	ESealedDoorState GetDoorState() const { return DoorState; }

	UFUNCTION(BlueprintPure, Category = "SealedDoor")
	ESealedRoomResult GetRoomResult() const { return RoomResult; }

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SealDecal;

	/** 레벨 배치 시 고정한다. 상자와 달리 랜덤 등급이 없다 — 위치가 등급을 정한다 */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "SealedDoor")
	ESealedDoorGrade Grade = ESealedDoorGrade::Common;

	UPROPERTY(ReplicatedUsing = OnRep_DoorState, VisibleAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	ESealedDoorState DoorState = ESealedDoorState::Sealed;

	/** 개방 전에는 반드시 Undetermined 로 남는다 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	ESealedRoomResult RoomResult = ESealedRoomResult::Undetermined;

	/** 방 안에 배치한 ATargetPoint 들. 서버에서만 쓰므로 복제하지 않는다 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "SealedDoor|Room")
	TArray<TObjectPtr<AActor>> SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor|Data")
	TObjectPtr<UDataTable> DoorGradeTable;

	/** 열쇠 이름 표시용. DT_Item */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SealedDoor|Data")
	TObjectPtr<UDataTable> ItemDataTable;

	/** 서버 전용. 내용물 재생성을 막는다 */
	bool bRoomGenerated = false;

	UFUNCTION()
	void OnRep_DoorState();

	UFUNCTION(BlueprintImplementableEvent, Category = "SealedDoor", meta = (DisplayName = "On Seal Broken"))
	void BP_OnSealBroken(ESealedDoorGrade InGrade);

	UFUNCTION(BlueprintImplementableEvent, Category = "SealedDoor", meta = (DisplayName = "On Door Opened"))
	void BP_OnDoorOpened(ESealedDoorGrade InGrade);

	/** DoorGradeTable 에서 현재 등급 행을 찾는다. 실패 시 nullptr */
	const FSealedDoorGradeData* FindGradeData() const;

	/** 상호작용자(플레이어 캐릭터)의 인벤토리. 플레이어가 아니면 nullptr */
	UInventoryComponent* GetInteractorInventory(AActor* Interactor) const;

	/** 인벤토리에서 지정한 행 이름의 아이템을 찾는다 */
	bool FindMatchingKey(UInventoryComponent* Inventory, FName RequiredKeyRowName, FGuid& OutInstanceId) const;

	/** 필요한 열쇠의 표시 이름. DT_Item 조회 */
	FText GetRequiredKeyDisplayName() const;

	/** 문짝 콜리전 해제 + 봉인 문양 숨김. 서버·클라이언트 양쪽에서 호출된다 */
	void ApplyOpenedVisual();

	/** 서버 전용. 개방 시점에 방 결과를 추첨한다 */
	void RollRoomResult();

	/** 서버 전용. 추첨된 결과에 따라 내용물을 배치한다. 한 번만 실행된다 */
	void GenerateRoom();

	/** 스폰 목록을 소비한다. 포인트가 모자라면 남은 스폰을 버린다 */
	void SpawnEntries(const TArray<FSealedRoomSpawnEntry>& InEntries, TArray<AActor*>& InOutAvailablePoints);
};
