#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item.h"
#include "ItemBase.generated.h"

class UStaticMeshComponent;
class USphereComponent;

// 이 아이템이 어느 DataTable에서 자기 데이터를 읽어와야 하는지
UENUM(BlueprintType)
enum class EItemSourceTable : uint8
{
	Item,
	Weapon,
	Shield
};

// 월드에 떨어져 있는 모든 아이템(잡템/재료/포션/무기/방패/장신구)의 공통 베이스 클래스.
// DT_Item / DT_Weapon / DT_Shield 중 하나에서 자신의 데이터를 읽어와 메쉬를 세팅하고,
// 공통 정보(이름/등급/판매가/격자크기/장착슬롯)를 어떤 테이블 출신인지 몰라도 꺼내 쓸 수 있게 캐싱해둠.
UCLASS()
class HEAVENBREACH_API AItemBase : public AActor
{
	GENERATED_BODY()

public:
	AItemBase();

	// ----- 에디터에서 지정 -----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EItemSourceTable SourceTable = EItemSourceTable::Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	UDataTable* ItemDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	UDataTable* WeaponDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	UDataTable* ShieldDataTable;

	// ----- 컴포넌트 -----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	UStaticMeshComponent* MeshComponent;

	// 플레이어가 가까이 왔을 때 상호작용(줍기) 판정용
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	USphereComponent* InteractionSphere;

	// ----- DataTable에서 읽어와 캐싱하는 공통 정보 (어느 테이블 출신이든 동일하게 사용 가능) -----

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

	// ----- 함수 -----

	// ItemID / SourceTable 기준으로 해당 DataTable에서 값을 읽어와 위 캐시 변수들을 채우고,
	// EquipMeshPath(무기/방패) 또는 WorldMeshPath(아이템)를 로드해서 MeshComponent에 세팅
	UFUNCTION(BlueprintCallable, Category = "Item")
	void InitializeFromDataTable();

protected:
	virtual void BeginPlay() override;

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
};
