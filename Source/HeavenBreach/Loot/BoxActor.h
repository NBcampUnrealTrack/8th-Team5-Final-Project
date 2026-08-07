// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "BoxActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** 상자의 개방 상태. 모든 클라이언트에 동일하게 리플리케이트된다. */
UENUM(BlueprintType)
enum class EBoxState : uint8
{
	Closed	UMETA(DisplayName = "Closed"),
	Opened	UMETA(DisplayName = "Opened")
	// NOTE: 개방 연출(후순위) 도입 시 Opening 상태 추가 예정
};

/**
 *  파밍 상자 액터 (골격).
 *  - 서버 권한으로 최초 1회만 개방되며, 개방 시 무작위 아이템을 상자 인벤토리에 채운다. (Loot 연동은 TODO)
 *  - 개방 시 [내 인벤토리 | 상자 인벤토리] 이중 UI를 연다. (Inventory/UI 연동은 TODO)
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

	/** 현재 개방 상태. 변경 시 OnRep_BoxState에서 외형을 갱신한다. */
	UPROPERTY(ReplicatedUsing = OnRep_BoxState, VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	EBoxState BoxState = EBoxState::Closed;

	/** 최초 개방 시 아이템 추첨을 1회만 수행하도록 보장하는 플래그 (서버 권한) */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	bool bLootGenerated = false;

	/** 상호작용 프롬프트 문구 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box")
	FText InteractPrompt = FText::FromString(TEXT("상자 열기 [E]"));

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
	 *  [서버] 최초 개방 시 무작위 아이템을 상자 인벤토리에 채운다.
	 *  TODO(loot): FLootTableRow + DataTable 가중치 랜덤, 최소 1개 보장, 상자 UInventoryComponent에 추가.
	 */
	void GenerateLoot();

	/**
	 *  [서버] Interactor에게 이중 인벤토리 UI를 열도록 지시하고 이동/공격을 잠근다.
	 *  TODO(inventory/ui): 상자 인벤토리 참조 전달 + Client RPC로 UI 오픈 + GAS 잠금 태그 부여.
	 */
	void OpenContainerUI(AActor* Interactor);

public:

	/** 현재 개방 상태 반환 */
	UFUNCTION(BlueprintPure, Category = "Box")
	EBoxState GetBoxState() const { return BoxState; }

	/** 이미 개방(추첨 완료)된 상자인지 반환 */
	UFUNCTION(BlueprintPure, Category = "Box")
	bool IsLootGenerated() const { return bLootGenerated; }
};
