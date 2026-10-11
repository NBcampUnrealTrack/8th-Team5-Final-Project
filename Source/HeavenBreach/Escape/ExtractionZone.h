// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "ExtractionZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class APawn;

UCLASS(Blueprintable)
class HEAVENBREACH_API AExtractionZone : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:

	AExtractionZone();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Extraction|Random")
	void SetExtractionActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Extraction|Random")
	bool IsExtractionActive() const { return bExtractionActive; }

	UFUNCTION(BlueprintPure, Category = "Extraction|Random")
	bool IsExtractionCandidate() const { return bExtractionCandidate; }

	float GetSelectionWeight() const { return SelectionWeight; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Zone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (ClampMin = 0, Units = "s"))
	float ExtractionDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction")
	FText InteractPrompt = FText::FromString(TEXT("탈출"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Random")
	bool bExtractionCandidate = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction|Random", meta = (ClampMin = "0.01"))
	float SelectionWeight = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Extraction|Random")
	bool bExtractionActive = true;

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction", meta = (DisplayName = "On Extraction Active Changed"))
	void BP_OnExtractionActiveChanged(bool bActive);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction", meta = (DisplayName = "On Extract Started"))
	void BP_OnExtractStarted(APawn* Player, float Duration);

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction", meta = (DisplayName = "On Extract Canceled"))
	void BP_OnExtractCanceled(APawn* Player);

	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction", meta = (DisplayName = "On Extract Completed"))
	void BP_OnExtractCompleted(APawn* Player);

private:

	TMap<APawn*, FTimerHandle> ChannelingPlayers;

	bool IsPawnInZone(const APawn* Pawn) const;

	void StartChannel(APawn* Pawn);

	void CancelChannel(APawn* Pawn);

	void CompleteChannel(APawn* Pawn);

	void ExtractPawn(APawn* Pawn);
};
