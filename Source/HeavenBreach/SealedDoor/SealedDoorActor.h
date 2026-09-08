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

	/**
	 * 디버그: 문을 다시 봉인 상태로 되돌린다. 스폰했던 내용물을 전부 제거하고
	 * 재추첨·재스폰이 가능한 상태로 만든다. PIE 를 껐다 켜지 않고 반복 실험하기 위한 것으로,
	 * 게임플레이 규칙(한 번 열린 문은 닫히지 않는다)과 무관한 에디터 전용 도구다.
	 * 액터를 선택하면 디테일 패널에 버튼이 뜬다.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "SealedDoor|Debug", meta = (DisplayName = "Debug: 다시 봉인"))
	void DebugReseal();

	/**
	 * 디버그: 열쇠 소비 없이 즉시 개방한다. 열쇠 아이템이 준비되기 전에
	 * 스폰·연출·복제를 검증하기 위한 것이다.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "SealedDoor|Debug", meta = (DisplayName = "Debug: 강제 개방"))
	void DebugForceOpen();

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

	/**
	 * 봉인 해제 연출이 시작된 서버 시각(AGameStateBase::GetServerWorldTimeSeconds() 기준).
	 * DoorState 와 같은 리플리케이션 묶음에 실려 전달되도록 상태 대입 직전에 설정한다.
	 * OnRep_DoorState 가 "방금 시작됨"과 "이미 진행/완료된 문이 지금 막 relevant 해짐"을
	 * 구분하는 데 쓰인다.
	 */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "SealedDoor")
	float SequenceStartServerTime = 0.0f;

	/** 서버 전용. 봉인 해제 연출 종료 타이머 */
	FTimerHandle OpenTimerHandle;

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

	/**
	 * 디버그: 방 결과를 강제한다. Undetermined 면 정상 추첨한다.
	 * 일반 봉인문은 50:50 이라 몇 번 열어보는 것만으로는 "안 나온다"와 "아직 안 걸렸다"를
	 * 구분할 수 없다. 한쪽 경로만 반복 검증할 때 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SealedDoor|Debug")
	ESealedRoomResult DebugForcedResult = ESealedRoomResult::Undetermined;

	/** 디버그: 켜면 열쇠 검사·소비를 건너뛰고 개방한다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SealedDoor|Debug")
	bool bDebugSkipKeyCheck = false;

	/**
	 * 서버 전용. 이 문이 스폰한 액터들. DebugReseal 이 정리 대상으로 쓴다.
	 * 게임플레이에서는 읽지 않는다.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SealedDoor|Debug")
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UFUNCTION()
	void OnRep_DoorState();

	/**
	 * 원샷 연출 훅. 봉인 해제 연출이 "방금" 시작됐을 때만 호출된다 (뒤늦게 relevant 해진
	 * 클라이언트나 늦게 접속한 플레이어에게는 호출되지 않는다). 기획서 8장의 시퀀스 —
	 * 봉인 문양 활성화, 열쇠 사용 연출, 문양 분해, 봉인 빛 소멸, 개방 효과음 — 이 자리다.
	 * 개방 사운드는 기획서상 PvP 위치 신호이므로, 이미 열린 지 오래된 문에서 재생되면 안 된다.
	 *
	 * InDurationSeconds 는 연출에 주어진 시간이다. 이 시간이 지나면 문이 열리므로,
	 * 몽타주·타임라인 길이를 여기에 맞춘다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "SealedDoor", meta = (DisplayName = "On Seal Broken"))
	void BP_OnSealBroken(ESealedDoorGrade InGrade, float InDurationSeconds);

	/**
	 * 상태 훅. 문이 열려 있는 동안(막 열렸을 때는 물론, 이미 열려 있던 문이 나중에
	 * relevant 해졌을 때도) 매번 호출된다. 멱등해야 한다 — 교체된 머티리얼, 열린 상태
	 * 프롭 배치처럼 "계속 유지되는" 시각 요소는 여기에 넣는다.
	 */
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

	/** ApplyOpenedVisual 의 역방향. DebugReseal 전용 */
	void ApplySealedVisual();

	/**
	 * 서버 전용. 봉인 해제 연출을 시작한다 — Opening 상태로 전환하고 등급별 개방 시간만큼
	 * 타이머를 건다. 시간이 0 이하면 곧바로 FinishOpening 을 호출한다.
	 * 정상 개방과 디버그 개방이 공유한다.
	 */
	void BeginOpening();

	/** 서버 전용. 연출이 끝난 시점. 결과 확정 → 스폰 → Opened 전환 → 복제 */
	void FinishOpening();

	/** 서버 전용. 개방 시점에 방 결과를 추첨한다 */
	void RollRoomResult();

	/** 서버 전용. 추첨된 결과에 따라 내용물을 배치한다. 한 번만 실행된다 */
	void GenerateRoom();

	/** 스폰 목록을 소비한다. 포인트가 모자라면 남은 스폰을 버린다 */
	void SpawnEntries(const TArray<FSealedRoomSpawnEntry>& InEntries, TArray<AActor*>& InOutAvailablePoints);

	/** OnRep_DoorState 에서 원샷 연출(BP_OnSealBroken) 재생 여부를 판단하는 시간 창 (초) */
	static constexpr float RecentlyStartedWindowSeconds = 2.0f;

	/**
	 * 봉인 해제 연출이 "방금" 시작됐는지 판정한다. SequenceStartServerTime 이 현재 서버 시각과
	 * RecentlyStartedWindowSeconds 이내로 가까우면 true. GameState 를 가져올 수 없으면 false.
	 */
	bool WasRecentlyStarted() const;
};
