#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item.h"
#include "InteractableInterface.h"
#include "ItemBase.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class USkeletalMeshComponent;

// �� ������ �ν��Ͻ��� ��� DataTable���� �ڱ� �����͸� �о�� �ϴ���
UENUM(BlueprintType)
enum class EItemSourceTable : uint8
{
	Item,
	Weapon,
	Shield
};

// �ʵ忡 ���̴� ��� ������(���/�Ҹ�ǰ/��� ��)�� ���� ���̽� Ŭ����.
// DT_Item / DT_Weapon / DT_Shield �� �ϳ����� �ڱ� �����͸� �о�� �޽��� �����ϰ�,
// ǥ�� ����(�̸�/���/�ǸŰ�/�׸���ũ��/������)�� � ���̺� ����̵� ������ �ʵ�� ĳ���صд�.
UCLASS()
class HEAVENBREACH_API AItemBase : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AItemBase();

	// ----- �����Ϳ��� ���� -----
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

	// ----- ������Ʈ -----
	// �ݸ��� + ���� �ùķ��̼��� ����ϴ� ��Ʈ. ���� �޽��� ����ƽ�̸� �̰� �� �ð������ε� ���̴� �޽���,
	// ���̷�Ż�̸� ������ �� ���̴�(Visibility false) �浹 ���� ��ü�� �ȴ�.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	UStaticMeshComponent* MeshComponent;

	// ���� ������ ���̷�Ż �޽��� ���(����/���� ��) ǥ�� ����. �ݸ���/������ ���� ���� �ʰ� MeshComponent�� ����ٴϱ⸸ �Ѵ�.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	USkeletalMeshComponent* SkeletalMeshComponent;

	// �÷��̾ ������ ���� �� ��ȣ�ۿ�(�ݱ�) ������
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	USphereComponent* InteractionSphere;

	// ----- DataTable���� �о�� ĳ���ϴ� ǥ�ÿ� ���� (� ���̺� ����̵� �����ϰ� ä����) -----
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

	// ���ã���� �������� ������ ��(�ʵ忡 ����߷��� ��) ���ã�� ���¸� ���� �ʵ���,
	// ��ӵ� ���� ���Ϳ��� ���� �Բ� ��� �ٴѴ�. TryPickUp���� �ٽ� �κ��丮��
	// ���� �� �� ���� �� FItemInstance�� �״�� �����Ѵ�.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Item")
	bool bIsFavorite = false;

	// 내구도. bIsFavorite와 동일한 패턴 - 드롭되는 순간의 FItemInstance::CurrentDurability를
	// 그대로 실어 보내고(UInventoryComponent::DropItem), TryPickUp에서 다시 FItemInstance로
	// 되돌릴 때 이 값을 그대로 복원한다. 기본값 -1은 "초기화 안 됨"을 의미하며,
	// 이 경우 FItemInstance 쪽 로직이 최대 내구도로 채운다(FItemInstance::CurrentDurability 주석 참고).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Item")
	float CurrentDurability = -1.f;

	// ----- �Լ� -----
	// ItemID / SourceTable �������� �ش� DataTable���� ���� �о�� ĳ�� �ʵ带 ä���,
	// EquipMeshPath(����/����) �Ǵ� WorldMeshPath(����)�� �ε��ؼ� MeshComponent/SkeletalMeshComponent�� �����Ѵ�.
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
	/** MeshComponent(�ݸ��� ��ü)�� Ʈ�������� �ٲ� ������ ȣ��� - ������ ��������
	 * ������ ���ȿ��� �Ҹ���, ��ü�� ����(�����ϸ�) �� �̻� �� �ҷ��� Tick���� �ξ� �δ�. */
	void OnMeshTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

	// ���� ĳ���� ���뿡�� �߶� ���̷�Ż �޽� ������ ��Ʈ��(������Ʈ ����)�� ���� ������Ʈ��
    // �߽��� �ٸ� �� �־, InitializeFromDataTable���� ����� ���� �������� �����صд�.
    // OnMeshTransformUpdated�� ������ ������ ������ ��ġ�� ������ ���ߴµ�, �� ���� ����
    // �ݿ������� ������ ���� ƽ���� �������� �ʱ�ȭ�Ǿ������.
	FVector SkeletalMeshLocalOffset = FVector::ZeroVector;

	// �������ڸ��� ���� ���� �ٷ� �ٽ� �ݴ� ��츦 ���� ���� ��ٿ� üũ ����
	bool TryPickUp(AActor* Picker);

	float SpawnTime = 0.f;
};