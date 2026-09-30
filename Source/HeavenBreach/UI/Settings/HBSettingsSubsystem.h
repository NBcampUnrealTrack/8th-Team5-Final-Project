#pragma once

#include "CoreMinimal.h"
#include "AudioDeviceHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HBSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;
struct FActorsInitializedParams;

/** Local preferences only; these values never travel to the server. */
UCLASS(Config=GameUserSettings)
class HEAVENBREACH_API UHBSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	float GetMouseSensitivity() const { return MouseSensitivity; }
	bool IsCombatCameraShakeEnabled() const { return bCombatCameraShake; }

	// 1인칭 카메라가 머리 움직임을 따라가는 세기. 0~1. UHBCameraMotionComponent 가 읽는다.
	float GetCameraMotionScale() const { return CameraMotionScale; }
	void SaveCameraMotionScale(float InScale);
	float GetMasterVolume() const { return MasterVolume; }
	float GetMusicVolume() const { return MusicVolume; }
	float GetEffectsVolume() const { return EffectsVolume; }
	int32 GetDLSSMode() const { return DLSSMode; }
	bool IsDLSSModeAvailable(int32 Mode) const;
	FText GetDLSSStatusText() const;
	void SaveDLSSMode(int32 Mode, bool bQualityChanged = false);
	void ApplyDLSS();

	void SavePreferences(float Master, float Music, float Effects, float Sensitivity, bool bCameraShake);
	void ApplyAudio(UWorld* World);

private:
	void OnWorldInitialized(const FActorsInitializedParams& Params);

	// CDO soft references keep these assets discoverable by the cooker.
	UPROPERTY()
	TSoftObjectPtr<USoundClass> MusicClassAsset = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/UI/Settings/Audio/SC_SettingsMusic.SC_SettingsMusic")));
	UPROPERTY()
	TSoftObjectPtr<USoundClass> EffectsClassAsset = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/UI/Settings/Audio/SC_SettingsEffects.SC_SettingsEffects")));
	UPROPERTY()
	TSoftObjectPtr<USoundMix> VolumeMixAsset = TSoftObjectPtr<USoundMix>(FSoftObjectPath(TEXT("/Game/UI/Settings/Audio/SM_SettingsVolume.SM_SettingsVolume")));

	UPROPERTY(Config)
	float MasterVolume = 1.0f;
	UPROPERTY(Config)
	float MusicVolume = 1.0f;
	UPROPERTY(Config)
	float EffectsVolume = 1.0f;
	UPROPERTY(Config)
	float MouseSensitivity = 1.0f;
	UPROPERTY(Config)
	bool bCombatCameraShake = true;
	// 전투 카메라 흔들림(맞을 때 등)과 별개다. 걷기·공격 중 머리 움직임이 화면에 들어가는 정도.
	UPROPERTY(Config)
	float CameraMotionScale = 1.0f;
	UPROPERTY(Config)
	int32 DLSSMode = 0;
	bool bDLSSApplied = false;
	int32 PreviousAntiAliasing = 0;
	float PreviousScreenPercentage = 100.0f;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MusicClass;
	UPROPERTY(Transient)
	TObjectPtr<USoundClass> EffectsClass;
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> VolumeMix;
	FAudioDeviceHandle SettingsAudioDevice;
};
