#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HBLobbyLoadingSubsystem.generated.h"

class UGameViewportClient;
class UNetDriver;
class UUserWidget;
class UAudioComponent;
class USoundBase;
class APlayerController;
class SWidget;
struct FWorldContext;

/** Local travel presentation; existing game/session code owns travel and authoritative readiness. */
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

	/** Used only by the startup presentation; normal/session travel is observed, not initiated here. */
	UFUNCTION(BlueprintCallable, Category="Loading")
	bool TravelFromStartup(TSoftObjectPtr<UWorld> Destination);

	UFUNCTION(BlueprintPure, Category="Loading")
	FString GetLoadingDestination() const { return DestinationPackage; }

private:
	UFUNCTION()
	void HandleLobbyOperation(EHBLobbyOperationState State, const FString& Error);
	void HandleSeamlessTravelStart(UWorld* World, const FString& Destination);
	void HandlePreClientTravel(const FString& Destination, ETravelType TravelType, bool bSeamless);
	void HandlePreLoadMap(const FWorldContext& Context, const FString& Destination);
	void HandlePostLoadMap(UWorld* World);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void BeginLoading(UWorld* SourceWorld, const FString& Destination);
	void FinishLoading(bool bSucceeded);
	void StartLoadingAudio(UWorld* SourceWorld);
	void StopLoadingAudio(bool bFadeOut);
	void MuteLobbyAudio(UWorld* World);
	bool TickGameplayMusic(float DeltaSeconds);
	void StopGameplayMusic();
	bool TickLoading(float DeltaSeconds);
	bool IsDestinationReady(UWorld* World) const;
	float CalculateLoadingProgress(UWorld* World) const;
	void UpdateLoadingProgress(float Progress);
	void AttachLoadingScreen();
	void CancelReadyFade();
	bool HasArrived(UWorld* World) const;
	bool IsSessionTransitionPending() const;
	bool IsMenuReady(const APlayerController* Player, UWorld* World) const;
	static UUserWidget* ReadWidgetProperty(const UObject* Object, FName Name);
	bool OwnsWorld(const UWorld* World) const;
	static bool IsLobbyWorld(const UWorld* World);
	static FString NormalizeMap(const FString& URL);

	UPROPERTY()
	TSubclassOf<UUserWidget> LoadingWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingWidget;

	UPROPERTY()
	TObjectPtr<USoundBase> LoadingMusic;
	UPROPERTY()
	TObjectPtr<USoundBase> LoadingAmbience;
	UPROPERTY()
	TObjectPtr<USoundBase> GameplayMusic;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> GameplayMusicAudio;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> LoadingAudio;
	TMap<TWeakObjectPtr<UAudioComponent>, float> MutedLobbyAudio;

	TSharedPtr<SWidget> ViewportContent;
	TWeakObjectPtr<UGameViewportClient> AttachedViewport;
	FString DestinationPackage;
	FString PendingStartupPackage;
	TWeakObjectPtr<UWorld> OutgoingWorld;
	TWeakObjectPtr<UWorld> ArrivedWorld;
	TWeakObjectPtr<UHBOnlineSessionSubsystem> Sessions;
	TWeakPtr<SWidget> PreviousFocus;
	double LoadingStartedAt = 0.0;
	double FadeCompleteAt = 0.0;
	double DestinationReadySince = 0.0;
	float LoadingProgress = 0.0f;
	bool bLoading = false;
	bool bGameplayReady = false;
	bool bPreviousIgnoreInput = false;
	bool bJoiningSession = false;
	bool bAwaitingNetworkWorld = false;
	bool bDestinationIgnoreInput = false;
	bool bPreviousWorldRenderingDisabled = false;
	FTSTicker::FDelegateHandle TickHandle;
	FTSTicker::FDelegateHandle GameplayMusicTickHandle;
	FDelegateHandle SeamlessStartHandle;
	FDelegateHandle PreClientTravelHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetworkFailureHandle;
};
