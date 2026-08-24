// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Interaction/InteractableInterface.h"
#include "BoxActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UInventoryComponent;
class UDataTable;
class UStaticMesh;
class APawn;

/** 상자의 개방 상태. 모든 클라이언트에 동일하게 리플리케이트된다. */
UENUM(BlueprintType)
enum class EBoxState : uint8
{
	Closed	UMETA(DisplayName = "Closed"),
	Opened	UMETA(DisplayName = "Opened")
	// NOTE: 개방 연출(후순위) 도입 시 Opening 상태 추가 예정
};

/** 상자 등급. 등급별로 드랍 풀·메시·색상이 달라진다. */
UENUM(BlueprintType)
enum class EBoxGrade : uint8
{
	Common		UMETA(DisplayName = "일반"),
	Rare		UMETA(DisplayName = "희귀"),
	Epic		UMETA(DisplayName = "영웅"),
	Celestial	UMETA(DisplayName = "천상")
};

/**
 *  등급 설정 데이터테이블(DT_BoxGrade)의 한 행.
 *  Row Name을 등급명(Common/Rare/Epic/Celestial)으로 지정한다.
 */
USTRUCT(BlueprintType)
struct FBoxGradeData : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 등급이 쓸 드랍 필터 ID (DT_BoxLoot의 BoxID). 비우면 등급명(Row Name)을 사용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString LootBoxID;

	/** 등급별 몸통 메시(내용물 있는 상태) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> FullMesh;

	/** 등급별 빈 몸통 메시(내용물 소진 시) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> EmptyMesh;

	/** 등급 색상(프롬프트/머티리얼 틴트/이펙트용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor GradeColor = FLinearColor::White;

	/** 랜덤 등급 추첨 시 이 등급의 가중치 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0))
	float SpawnWeight = 1.0f;
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

	/** 상자 등급. bRandomGrade가 꺼져 있으면 이 값이 고정 등급. 변경 시 OnRep_Grade에서 외형 적용. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Grade, BlueprintReadOnly, Category = "Box|Grade")
	EBoxGrade Grade = EBoxGrade::Common;

	/** 켜면 스폰 시 서버가 SpawnWeight로 등급을 무작위 추첨한다. 끄면 위 Grade 고정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Grade")
	bool bRandomGrade = false;

	/** 등급 설정 데이터테이블 (FBoxGradeData 행, Row Name = 등급명). 예: DT_BoxGrade */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Grade")
	TObjectPtr<UDataTable> BoxGradeTable;

	/** BoxState 리플리케이션 콜백. 모든 클라이언트에서 열림/닫힘 외형을 갱신한다. */
	UFUNCTION()
	void OnRep_BoxState();

	/** Grade 리플리케이션 콜백. 모든 클라이언트에서 등급 외형(메시/색)을 적용한다. */
	UFUNCTION()
	void OnRep_Grade();

	/** [서버] bRandomGrade면 SpawnWeight 가중치로 등급을 추첨한다. */
	void RollGradeIfNeeded();

	/** 현재 Grade의 설정(FBoxGradeData)을 적용: 몸통 메시 교체 + 색/루트ID/빈메시 캐시. 서버·클라 공통. */
	void ApplyGrade();

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

	/** 현재 상자 등급 */
	UFUNCTION(BlueprintPure, Category = "Box|Grade")
	EBoxGrade GetGrade() const { return Grade; }

	/** 현재 등급 색상 (프롬프트/이펙트용) */
	UFUNCTION(BlueprintPure, Category = "Box|Grade")
	FLinearColor GetGradeColor() const { return GradeColor; }

	/** 현재 등급의 빈 몸통 메시 (BP 빈 상자 교체용). 없으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "Box|Grade")
	UStaticMesh* GetEmptyMesh() const;

private:

	/** ApplyGrade가 캐시하는 실제 드랍 필터 ID (등급 설정 우선, 없으면 BoxID). */
	FString EffectiveLootID;

	/** ApplyGrade가 캐시하는 등급 색상 */
	FLinearColor GradeColor = FLinearColor::White;

	/** ApplyGrade가 캐시하는 빈 몸통 메시 */
	TSoftObjectPtr<UStaticMesh> CachedEmptyMesh;

	/** 등급 → 데이터테이블 Row Name */
	static FName GradeToRowName(EBoxGrade InGrade);
};
