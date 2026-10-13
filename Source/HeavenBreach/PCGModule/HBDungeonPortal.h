#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBDungeonPortal.generated.h"

class UBoxComponent;
class UArrowComponent;
class UAudioComponent;
class APawn;
class ULevel;

/** Place in a source room map. Links are resolved per streamed level instance by the generator. */
UCLASS(Blueprintable)
class HEAVENBREACH_API AHBDungeonPortal : public AActor
{
	GENERATED_BODY()
public:
	AHBDungeonPortal();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon|Portal") FName PortalId;
	/** Pawn origins entering this box activate the portal. Positive component scale is reflected in physical bounds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|Portal") TObjectPtr<UBoxComponent> Trigger;
	/** Destination pawn origin and facing; place outside the trigger and clear of blocking geometry. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|Portal") TObjectPtr<UArrowComponent> Arrival;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dungeon|Portal|Audio") TObjectPtr<UAudioComponent> PortalAmbience;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon|Portal", meta=(ClampMin="0.1")) float ReentryDelay = 0.5f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Dungeon|Portal") TObjectPtr<AHBDungeonPortal> TargetPortal;
	UFUNCTION(BlueprintCallable, Category="Dungeon|Portal") bool ConnectTo(AHBDungeonPortal* Other);
	UFUNCTION(BlueprintCallable, Category="Dungeon|Portal") void Disconnect();
	UFUNCTION(BlueprintCallable, Category="Dungeon|Portal") bool TryTeleport(APawn* Pawn);
	bool ValidateDefinition(FString& Error) const;
	bool Contains(const FVector& Point) const;
	/** Random portals stay inactive until the complete generation is ready. Authored portals default to enabled. */
	void SetTravelEnabled(bool bEnabled) { bTravelEnabled = bEnabled; SetActorTickEnabled(bEnabled); }
	/** Resolve only inside a particular generated level instance; never search the entire world by ID. */
	static AHBDungeonPortal* FindInLevel(ULevel* Level, FName Id, FString& Error);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	// A pawn must both leave the activation region and finish its delay before re-entry.
	TMap<TWeakObjectPtr<APawn>, double> BlockedPawns;
	bool bTravelEnabled = true;
};
