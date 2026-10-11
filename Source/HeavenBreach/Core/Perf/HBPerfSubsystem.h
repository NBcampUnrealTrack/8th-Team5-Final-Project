#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HBPerfSubsystem.generated.h"

class UTimelineComponent;
class USceneCaptureComponent2D;
class ULocalLightComponent;
class ULevel;

UCLASS()
class HEAVENBREACH_API UHBPerfSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleLevelAdded(ULevel* Level, UWorld* World);
	void ScanLevel(const ULevel* Level);
	void UpdateTorches(bool bEnabled);
	void UpdateMinimap(float DeltaTime, bool bEnabled);
	void UpdateShadows(bool bEnabled);
	void UpdateLightCull(bool bEnabled);
	bool GetLocalView(FVector& OutLocation, APawn*& OutPawn) const;

	struct FShadowLight
	{
		TWeakObjectPtr<ULocalLightComponent> Light;
		bool bShadowOn = true;
	};

	struct FCullLight
	{
		TWeakObjectPtr<ULocalLightComponent> Light;
		float OriginalMaxDrawDistance = 0.f;
		float OriginalFadeRange = 0.f;
	};

	TArray<TWeakObjectPtr<UTimelineComponent>> TorchTimelines;
	TArray<FShadowLight> ShadowLights;
	TArray<FCullLight> CullLights;
	float LightCullTimer = 0.f;
	bool bWasLightCullEnabled = true;
	float ShadowTimer = 0.f;
	bool bWasShadowEnabled = true;
	TArray<TWeakObjectPtr<ULevel>> PendingLevels;
	float PendingScanTimer = 0.f;
	TWeakObjectPtr<USceneCaptureComponent2D> LocalCapture;
	TWeakObjectPtr<APawn> LocalCapturePawn;
	FDelegateHandle LevelAddedHandle;
	float TorchTimer = 0.f;
	float CaptureTimer = 0.f;
	float RemoteCaptureTimer = 0.f;
	bool bWasEnabled = true;
};
