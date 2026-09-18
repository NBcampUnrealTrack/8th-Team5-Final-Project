#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HBSettingsAudioTestActor.generated.h"

class UAudioComponent;
class USoundBase;
class UHBSettingsWidget;

/** Optional, editor-only listening station placed in L_Test. */
UCLASS()
class HEAVENBREACH_API AHBSettingsAudioTestActor : public AActor
{
	GENERATED_BODY()

#pragma region TemporarySettingsAudioTest
	// TODO: Remove this test actor from L_Test after sound settings acceptance.
public:
	AHBSettingsAudioTestActor();
	virtual bool IsEditorOnly() const override { return true; }
	virtual bool IsEditorOnlyLoadedInPIE() const override { return true; }
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Settings Audio Test") void ToggleSettings();
	UFUNCTION(BlueprintCallable, Category="Settings Audio Test") void ToggleMusic();
	UFUNCTION(BlueprintCallable, Category="Settings Audio Test") void ToggleEffectsLoop();
	UFUNCTION(BlueprintCallable, Category="Settings Audio Test") void PlayEffect();

	UPROPERTY(EditAnywhere, Category="Settings Audio Test")
	TSoftObjectPtr<USoundBase> MusicSound;
	UPROPERTY(EditAnywhere, Category="Settings Audio Test")
	TSoftObjectPtr<USoundBase> EffectSound;
	UPROPERTY(EditAnywhere, Category="Settings Audio Test", meta=(ClampMin="0.2"))
	float EffectsInterval = 2.0f;
	UPROPERTY(EditAnywhere, Category="Settings Audio Test")
	bool bStartMusic = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RestoreGameInput();
	void UpdateGuide();
	UPROPERTY(VisibleAnywhere, Category="Settings Audio Test") TObjectPtr<UAudioComponent> Music;
	UPROPERTY(VisibleAnywhere, Category="Settings Audio Test") TObjectPtr<UAudioComponent> Effects;
	UPROPERTY(Transient) TObjectPtr<UHBSettingsWidget> SettingsWidget;
	TWeakObjectPtr<APlayerController> LocalController;
	FTimerHandle EffectsTimer;
	bool bInputReady = false;
	bool bSettingsInput = false;
	bool bPreviousMouseCursor = false;
	bool bMusicEnabled = false;
	float GuideElapsed = 1.0f;
#pragma endregion TemporarySettingsAudioTest
};
