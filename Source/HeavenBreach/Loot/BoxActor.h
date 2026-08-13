// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "BoxActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UInventoryComponent;
class UDataTable;
class APawn;

/** 상자의 개방 상태. 모든 클라이언트에 동일하게 리플리케이트된다. */
UENUM(BlueprintType)
enum class EBoxState : uint8
{
	Closed	UMETA(DisplayName = "Closed"),
	Opened	UMETA(DisplayName = "Opened")
	// NOTE: 개방 연출(후순위) 도입 시 Opening 상태 추가 예정
};

/**
 *  파밍 상자 액터.
 *  - 서버 권한으로 최초 1회만 개방되며, 개방 시 DT_BoxLoot에서 무작위 아이템을 추첨해 상자 인벤토리에 채운다.
 *  - 개방 시 BP_OpenContainer로 [내 인벤토리 | 상자 인벤토리] 이중 UI를 연다.
 *  - 열림 외형은 OnRep_BoxState를 통해 모든 클라이언트에 동일하게 표시된다.
 *
 *  실제 배치는 이 클래스를 상속한 Blueprint(BP_Box_*)로 메시/드랍테이블을 지정해 사용한다.
 */
UCLASS(Blueprintable)
class HEAVENBREACH_API ABoxActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:

	ABoxActor();

	//~ Begin AActor Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

	//~ Begin IInteractableInterface
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	//~ End IInteractableInterface

protected:

	virtual void BeginPlay() override;

	/** 상자 본체 메시 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BoxMesh;

	/** 상호작용 감지/거리 판정용 볼륨 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionVolume;

	/** 상자 내용물(추첨된 아이템)을 담는 인벤토리. 그리드 크기는 컴포넌트 Details에서 설정. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> BoxInventory;

	/** 현재 개방 상태. 변경 시 OnRep_BoxState에서 외형을 갱신한다. */
	UPROPERTY(ReplicatedUsing = OnRep_BoxState, VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	EBoxState BoxState = EBoxState::Closed;

	/** 최초 개방 시 아이템 추첨을 1회만 수행하도록 보장하는 플래그 (서버 권한) */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	bool bLootGenerated = false;

	/** 상호작용 프롬프트 문구 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box")
	FText InteractPrompt = FText::FromString(TEXT("상자 열기 [F]"));

	/** 이 상자의 식별자. DT_BoxLoot의 BoxID와 일치하는 행만 추첨 대상이 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Loot")
	FString BoxID;

	/** 상자별 드랍 목록 데이터테이블 (FBoxLootData 행). 예: DT_BoxLoot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Loot")
	TObjectPtr<UDataTable> BoxLootTable;

	/** 아이템 정의 데이터테이블 (FItemData 행). 추첨된 ItemID로 아이템 인스턴스를 만든다. 예: DT_Item */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Loot")
	TObjectPtr<UDataTable> ItemDataTable;

	/** BoxState 리플리케이션 콜백. 모든 클라이언트에서 열림/닫힘 외형을 갱신한다. */
	UFUNCTION()
	void OnRep_BoxState();

	/**
	 *  개방 외형/연출 갱신 (메시 스왑, 머티리얼, VFX, SFX 등).
	 *  서버·클라 공통으로 호출된다. 세부 연출은 Blueprint에서 구현.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Box", meta = (DisplayName = "On Box State Changed"))
	void BP_OnBoxStateChanged(EBoxState NewState);

	/**
	 *  [서버] 최초 개방 시 DT_BoxLoot에서 무작위 아이템을 추첨해 상자 인벤토리에 채운다.
	 *  BoxID가 일치하는 행마다 DropChance로 판정, 성공 시 1~MaxItemCount개 추가. 최소 1개 보장.
	 */
	void GenerateLoot();

	/**
	 *  [서버] Interactor의 InteractionComponent에 Client RPC를 보내, 요청 클라에서 컨테이너 UI를 열게 한다.
	 *  (데디케이트 서버 대응) 실제 오픈은 클라의 OnRequestOpenContainer → BP → InventoryUI.OpenContainer.
	 */
	void OpenContainerUI(AActor* Interactor);

public:

	/** 현재 개방 상태 반환 */
	UFUNCTION(BlueprintPure, Category = "Box")
	EBoxState GetBoxState() const { return BoxState; }

	/** 이미 개방(추첨 완료)된 상자인지 반환 */
	UFUNCTION(BlueprintPure, Category = "Box")
	bool IsLootGenerated() const { return bLootGenerated; }

	/** 상자 인벤토리 컴포넌트 반환 (컨테이너 UI 바인딩용) */
	UFUNCTION(BlueprintPure, Category = "Box")
	UInventoryComponent* GetBoxInventory() const { return BoxInventory; }
};
