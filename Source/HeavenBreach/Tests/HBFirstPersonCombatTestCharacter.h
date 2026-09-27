#pragma once

#include "CoreMinimal.h"
#include "Player/HBPlayerCharacter.h"
#include "Engine/DataTable.h"
#include "HBFirstPersonCombatTestCharacter.generated.h"

// Isolated combat lab. Production player classes never derive from this class.
UCLASS()
class HEAVENBREACH_API AHBFirstPersonCombatTestCharacter : public AHBPlayerCharacter
{
	GENERATED_BODY()

public:
	AHBFirstPersonCombatTestCharacter();

protected:
#pragma region TemporaryFirstPersonCombatTest
	// TODO: Remove this test adapter when dual-view equipment is integrated into the production character.
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Test|FirstPerson")
	TObjectPtr<USkeletalMeshComponent> TestFirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Test|FirstPerson")
	TObjectPtr<UStaticMeshComponent> TestFirstPersonSword;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Test|FirstPerson")
	TObjectPtr<UStaticMeshComponent> TestFirstPersonShield;

	UPROPERTY(EditDefaultsOnly, Category="Test|Loadout")
	FDataTableRowHandle TestSwordRow;

	UPROPERTY(EditDefaultsOnly, Category="Test|Loadout")
	FDataTableRowHandle TestShieldRow;

	UFUNCTION()
	void SyncTestEquipment();

	void BindTestEquipment();
	void EquipTestLoadout();
	bool bTestLoadoutGranted = false;
#pragma endregion
};
