// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HBMagicVFXPreloadSubsystem.generated.h"

struct FStreamableHandle;
class UNiagaraSystem;

/** Keeps the orb/explosion presentation resident across lobby travel and garbage collection. */
UCLASS()
class HEAVENBREACH_API UHBMagicVFXPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Systems loaded/compiled and PSO requests issued; not a gameplay or PSO-completion gate. */
	UFUNCTION(BlueprintPure, Category = "Magic|VFX")
	bool IsPreloadComplete() const { return bPreloadComplete; }

	UFUNCTION(BlueprintPure, Category = "Magic|VFX")
	int32 GetResidentAssetCount() const { return ResidentAssets.Num(); }

private:
	void OnAssetsLoaded();
	bool PollPreparation(float DeltaSeconds);

	// A soft path alone does not keep Niagara's prepared VM data alive between casts.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> ResidentAssets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraSystem>> PendingSystems;

	TSharedPtr<FStreamableHandle> LoadHandle;
	FTSTicker::FDelegateHandle PreparationHandle;
	double PreparationStartedAt = 0.0;
	bool bPreloadComplete = false;
	bool bLoadFailed = false;
};
