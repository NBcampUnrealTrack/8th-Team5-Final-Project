#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item.h"
#include "InteractableInterface.h"
#include "ItemBase.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class USkeletalMeshComponent;

// 이 아이템 인스턴스가 어느 DataTable에서 자기 데이터를 읽어야 하는지
UENUM(BlueprintType)
enum class EItemSourceTable : uint8
{
	Item,
	Weapon,
	Shield
};

// 필드에 놓이는 모든 아이템(장비/소모품/재료 등)의 공통 베이스 클래스.
// DT_Item / DT_Weapon / DT_Shield 중 하나에서 자기 데이터를 읽어와 메쉬를 결정하고,
// 표시 정보(이름/등급/판매가/그리드크기/아이콘)는 어떤 테이블 기반이든 동일한 필드로 캐싱해둔다.
UCLASS()
class HEAVENBREACH_API AItemBase : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AItemBase();

	// ----- 에디터에서 설정 -----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ItemID, Category = "Item")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ItemID, Category = "Item")
	EItemSourceTable SourceTable = EItemSourceTable::Item;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ItemID, Category = "Item")
	UDataTable* ItemDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ItemID, Category = "Item")
	UDataTable* WeaponDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ItemID, Category = "Item")
	UDataTable* ShieldDataTable;

	// ----- 컴포넌트 -----
	// 콜리전 + 물리 시뮬레이션을 담당하는 루트. 실제 메쉬가 스태틱이면 이게 곧 시각적으로도 보이는 메쉬고,
	// 스켈레탈이면 눈에는 안 보이는(Visibility false) 충돌 전용 구체가 된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	UStaticMeshComponent* MeshComponent;

	// 실제 에셋이 스켈레탈 메쉬인 장비(갑옷/투구 등) 표시 전용. 콜리전/물리는 절대 갖지 않고 MeshComponent를 따라다니기만 한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	USkeletalMeshComponent* SkeletalMeshComponent;

	// 플레이어가 가까이 갔을 때 상호작용(줍기) 판정용
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	USphereComponent* InteractionSphere;

	// ----- DataTable에서 읽어와 캐싱하는 표시용 정보 (어떤 테이블 기반이든 동일하게 채워짐) -----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Cached")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Cached")
	EItemGrade Grade = EItemGrade::Normal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Cached")
	EEquipSlotType EquipSlotType = EEquipSlotType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Cached")
	int32 SellValue = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Cached")
	int32 GridWidth = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Cached")
	int32 GridHeight = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Cached")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Item")
	int32 StackCount = 1;

	// ----- 함수 -----
	// ItemID / SourceTable 기준으로 해당 DataTable에서 값을 읽어와 캐시 필드를 채우고,
	// EquipMeshPath(무기/방패) 또는 WorldMeshPath(잡템)를 로드해서 MeshComponent/SkeletalMeshComponent에 적용한다.
	UFUNCTION(BlueprintCallable, Category = "Item")
	void InitializeFromDataTable();

	//~ Begin AActor Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

	UFUNCTION()
	void OnRep_ItemID();

	// ----- IInteractableInterface -----
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float PickupBlockDuration = 1.0f;

private:
	/** MeshComponent(콜리전 구체)의 트랜스폼이 바뀔 때마다 호출됨 - 물리로 떨어지고
	 * 구르는 동안에만 불리고, 몸체가 잠들면(정지하면) 더 이상 안 불려서 Tick보다 훨씬 싸다. */
	void OnMeshTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

	// 전신 캐릭터 리깅에서 잘라낸 스켈레탈 메쉬 파츠는 루트본(컴포넌트 원점)과 실제 지오메트리
    // 중심이 다를 수 있어서, InitializeFromDataTable에서 계산한 보정 오프셋을 저장해둔다.
    // OnMeshTransformUpdated가 물리로 움직일 때마다 위치를 강제로 맞추는데, 이 값을 같이
    // 반영해주지 않으면 물리 틱마다 오프셋이 초기화되어버린다.
	FVector SkeletalMeshLocalOffset = FVector::ZeroVector;

	// 스폰되자마자 던진 쪽이 바로 다시 줍는 경우를 막기 위한 쿨다운 체크 포함
	bool TryPickUp(AActor* Picker);

	float SpawnTime = 0.f;
};