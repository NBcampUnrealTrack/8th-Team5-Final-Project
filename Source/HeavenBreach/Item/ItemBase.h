#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item.h"
#include "InteractableInterface.h"
#include "ItemBase.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EItemSourceTable : uint8
{
	Item,
	Weapon,
	Shield
};

USTRUCT(BlueprintType)
struct FItemPickupEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EItemSourceTable SourceTable = EItemSourceTable::Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0.0"))
	float Weight = 1.f;
};

UCLASS()
class HEAVENBREACH_API AItemBase : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AItemBase();

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	USkeletalMeshComponent* SkeletalMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	USphereComponent* InteractionSphere;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Item")
	bool bIsFavorite = false;

	// 내구도. bIsFavorite와 동일한 패턴 - 드롭되는 순간의 FItemInstance::CurrentDurability를
	// 그대로 실어 보내고(UInventoryComponent::DropItem), TryPickUp에서 다시 FItemInstance로
	// 되돌릴 때 이 값을 그대로 복원한다. 기본값 -1은 "초기화 안 됨"을 의미하며,
	// 이 경우 FItemInstance 쪽 로직이 최대 내구도로 채운다(FItemInstance::CurrentDurability 주석 참고).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Item")
	float CurrentDurability = -1.f;

	// 각인. bIsFavorite · CurrentDurability 와 같은 방식이다.
	// 버릴 때 실어 두고(UInventoryComponent::DropItem), 주울 때 되살린다(TryPickUp).
	// 레벨에 미리 놓아 둔 아이템은 비어 있고(bEngraved false), 주울 때 인벤토리 입구에서 새로 굴린다.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Item")
	FItemEngraving Engraving;

	UPROPERTY()
	TArray<FBagItemEntry> BagContents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Random")
	TArray<FItemPickupEntry> RandomCandidates;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Random")
	bool bRandomizeItemID = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Random", meta = (ClampMin = "0"))
	int32 RandomBundleRolls = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Item|Bundle")
	TArray<FItemPickupEntry> BundleItems;

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
	void OnMeshTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

	FVector SkeletalMeshLocalOffset = FVector::ZeroVector;

	bool TryPickUp(AActor* Picker);

	UDataTable* GetDataTableFor(EItemSourceTable InSource) const;

	void RollPickupContents();

	bool GrantBundleEntry(class UInventoryComponent* Inventory, class UEquipmentComponent* Equipment, FItemPickupEntry& InOutEntry);

	static bool AddWithPickupFlow(class UInventoryComponent* Inventory, class UEquipmentComponent* Equipment, const FItemInstance& InItem, EEquipSlotType InSlot, int32& OutRemainder);

	static const FItemPickupEntry* PickWeighted(const TArray<FItemPickupEntry>& InEntries);

	static int32 RollCount(const FItemPickupEntry& InEntry);

	bool bMainItemClaimed = false;

	float SpawnTime = 0.f;
};