#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HBLobbyLoadingSubsystem.generated.h"

class UGameViewportClient;
class UNetDriver;
class UUserWidget;
class SWidget;
struct FWorldContext;

/** Local presentation for lobby departure; the existing server remains responsible for match readiness. */
UCLASS()
class HEAVENBREACH_API UHBLobbyLoadingSubsystem : public UGameInstanceSubsystem
{

	GENERATED_BODY()

public:
	UHBLobbyLoadingSubsystem();
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Loading")
	bool IsLoadingScreenActive() const { return bLoading; }

	UFUNCTION(BlueprintPure, Category="Loading")
	bool IsGameplayReady() const { return bGameplayReady; }

	UFUNCTION(BlueprintPure, Category="Loading")
	UUserWidget* GetLoadingScreenWidget() const { return LoadingWidget; }

	/** Completed loading stages, normalized to 0..1; 1 requires gameplay readiness. */
	UFUNCTION(BlueprintPure, Category="Loading")
	float GetLoadingProgress() const { return LoadingProgress; }

private:
	void HandleSeamlessTravelStart(UWorld* World, const FString& Destination);
	void HandlePreClientTravel(const FString& Destination, ETravelType TravelType, bool bSeamless);
	void HandlePreLoadMap(const FWorldContext& Context, const FString& Destination);
	void HandlePostLoadMap(UWorld* World);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void BeginLoading(UWorld* SourceWorld, const FString& Destination);
	void FinishLoading(bool bSucceeded);
	bool TickLoading(float DeltaSeconds);
	bool IsDestinationReady(UWorld* World) const;
	float CalculateLoadingProgress(UWorld* World) const;
	void UpdateLoadingProgress(float Progress);
	bool OwnsWorld(const UWorld* World) const;
	static bool IsLobbyWorld(const UWorld* World);
	static FString NormalizeMap(const FString& URL);

	UPROPERTY()
	TSubclassOf<UUserWidget> LoadingWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingWidget;

	TSharedPtr<SWidget> ViewportContent;
	TWeakObjectPtr<UGameViewportClient> AttachedViewport;
	FString DestinationPackage;
	double LoadingStartedAt = 0.0;
	double FadeCompleteAt = 0.0;
	float LoadingProgress = 0.0f;
	bool bLoading = false;
	bool bGameplayReady = false;
	bool bPreviousIgnoreInput = false;
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle SeamlessStartHandle;
	FDelegateHandle PreClientTravelHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetworkFailureHandle;
};
