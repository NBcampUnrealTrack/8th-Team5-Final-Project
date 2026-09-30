#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryComponent.h"
#include "HBPlayerCorpseInventory.generated.h"

/**
 * 죽은 플레이어의 시체 가방. 죽는 순간 들고 있던 장비와 가방 아이템을 전부 받는다.
 * 팀원이 F 로 열어 가져가고, 가져간 아이템은 가져간 사람 것이 된다.
 *
 * 칸 수를 따로 복제하는 이유: 부모의 Width/Height 는 복제되지 않는다.
 * 아이템이 8x8 에 다 안 들어가 세로를 늘렸을 때 클라 화면도 같이 늘어나야 한다.
 */
UCLASS()
class HEAVENBREACH_API UHBPlayerCorpseInventory : public UInventoryComponent
{
	GENERATED_BODY()

public:
	UHBPlayerCorpseInventory();

	// 서버 전용. 장비 → 가방 순서로 담는다. 안 들어가면 한 줄씩 늘려 처음부터 다시 담는다.
	void FillFrom(const TArray<FItemInstance>& EquippedItems, const TArray<FInventoryEntry>& BagEntries);

	// 서버 전용. 연 사람 클라에 컨테이너 화면을 띄운다.
	void HandleInteract(AActor* Interactor);

	FText GetPrompt() const { return InteractPrompt; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Corpse")
	int32 CorpseColumns = 8;

	UPROPERTY(ReplicatedUsing = OnRep_CorpseRows)
	int32 CorpseRows = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Corpse")
	FText InteractPrompt = FText::FromString(TEXT("시체 뒤지기 [F]"));

	UFUNCTION()
	void OnRep_CorpseRows();

private:
	// 지금 칸 수로 전부 담아 본다. 하나라도 안 들어가면 false.
	bool TryFillAll(const TArray<FItemInstance>& EquippedItems, const TArray<FInventoryEntry>& BagEntries);
};
