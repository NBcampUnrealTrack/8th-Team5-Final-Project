// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item/Item.h"
#include "Interaction/InteractableInterface.h"
#include "BoxActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UInventoryComponent;
class UDataTable;
class APawn;

UENUM(BlueprintType)
enum class EBoxState : uint8
{
	Closed	UMETA(DisplayName = "Closed"),
	Opened	UMETA(DisplayName = "Opened")
};

UENUM(BlueprintType)
enum class EBoxGrade : uint8
{
	Common		UMETA(DisplayName = "일반"),
	Rare		UMETA(DisplayName = "희귀"),
	Epic		UMETA(DisplayName = "영웅"),
	Celestial	UMETA(DisplayName = "천상")
};

USTRUCT(BlueprintType)
struct FBoxGradeLootData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0))
	int32 MinDraws = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0))
	int32 MaxDraws = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<EItemGrade, float> ItemGradeWeights;
};

UCLASS(Blueprintable)
class HEAVENBREACH_API ABoxActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:

	ABoxActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BoxMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> BoxInventory;

	UPROPERTY(ReplicatedUsing = OnRep_BoxState, VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	EBoxState BoxState = EBoxState::Closed;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	bool bLootGenerated = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box")
	FText InteractPrompt = FText::FromString(TEXT("상자 열기 [F]"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Loot")
	TObjectPtr<UDataTable> ItemDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Loot")
	TObjectPtr<UDataTable> WeaponDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Loot")
	TObjectPtr<UDataTable> ShieldDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Loot")
	TObjectPtr<UDataTable> BoxGradeLootTable;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Grade, BlueprintReadOnly, Category = "Box|Grade")
	EBoxGrade Grade = EBoxGrade::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Grade")
	bool bRandomGrade = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Grade")
	TMap<EBoxGrade, float> GradeSpawnWeights;

	UFUNCTION()
	void OnRep_BoxState();

	UFUNCTION()
	void OnRep_Grade();

	UFUNCTION(BlueprintImplementableEvent, Category = "Box", meta = (DisplayName = "On Box State Changed"))
	void BP_OnBoxStateChanged(EBoxState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Box", meta = (DisplayName = "On Grade Changed"))
	void BP_OnGradeChanged(EBoxGrade NewGrade);

	void GenerateLoot();

	void OpenContainerUI(AActor* Interactor);

	void RollGradeIfNeeded();

	void RefreshGrade();

public:

	UFUNCTION(BlueprintPure, Category = "Box")
	EBoxState GetBoxState() const { return BoxState; }

	UFUNCTION(BlueprintPure, Category = "Box")
	bool IsLootGenerated() const { return bLootGenerated; }

	UFUNCTION(BlueprintPure, Category = "Box")
	UInventoryComponent* GetBoxInventory() const { return BoxInventory; }

	UFUNCTION(BlueprintPure, Category = "Box|Grade")
	EBoxGrade GetGrade() const { return Grade; }

private:

	static FName GradeToRowName(EBoxGrade InGrade);
};
