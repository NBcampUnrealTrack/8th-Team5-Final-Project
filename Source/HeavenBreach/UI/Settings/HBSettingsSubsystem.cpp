#include "UI/Settings/HBSettingsSubsystem.h"

#include "Engine/GameInstance.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#if HB_WITH_DLSS
#include "DLSSLibrary.h"
#endif

#define LOCTEXT_NAMESPACE "HBSettings"

bool UHBSettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UGameInstance* GI = Cast<UGameInstance>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && GI && !GI->IsDedicatedServerInstance();
}

void UHBSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadConfig();
	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
	EffectsVolume = FMath::Clamp(EffectsVolume, 0.0f, 1.0f);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 3.0f);
	MusicClass = MusicClassAsset.LoadSynchronous();
	EffectsClass = EffectsClassAsset.LoadSynchronous();
	VolumeMix = VolumeMixAsset.LoadSynchronous();
	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &ThisClass::OnWorldInitialized);
}

void UHBSettingsSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);
	if (SettingsAudioDevice.IsValid() && VolumeMix)
	{
		SettingsAudioDevice->PopSoundMixModifier(VolumeMix);
	}
	SettingsAudioDevice.Reset();
	// Editor PIE shares rendering CVars; return them to their pre-session state.
	if (bDLSSApplied)
	{
		DLSSMode = 0;
		ApplyDLSS();
	}
	Super::Deinitialize();
}

void UHBSettingsSubsystem::OnWorldInitialized(const FActorsInitializedParams& Params)
{
	if (Params.World && Params.World->GetGameInstance() == GetGameInstance())
	{
		ApplyAudio(Params.World);
		ApplyDLSS();
	}
}

bool UHBSettingsSubsystem::IsDLSSModeAvailable(int32 Mode) const
{
	if (Mode == 0) return true;
#if HB_WITH_DLSS
	const UDLSSMode Modes[] = {UDLSSMode::Off, UDLSSMode::Quality, UDLSSMode::Balanced, UDLSSMode::Performance};
	return Mode > 0 && Mode < 4 && UDLSSLibrary::IsDLSSSupported() && UDLSSLibrary::IsDLSSModeSupported(Modes[Mode]);
#else
	return false;
#endif
}

FText UHBSettingsSubsystem::GetDLSSStatusText() const
{
#if HB_WITH_DLSS
	switch (UDLSSLibrary::QueryDLSSSupport())
	{
	case UDLSSSupport::Supported:
		return LOCTEXT("DLSSHelp", "낮은 해상도로 렌더링한 화면을 AI로 복원합니다.\n\n품질은 화질을, 성능은 프레임 속도를 우선합니다. 프레임 생성은 포함하지 않습니다.");
	case UDLSSSupport::NotSupportedIncompatibleHardware:
		return LOCTEXT("DLSSHardware", "현재 그래픽카드는 DLSS를 지원하지 않습니다. NVIDIA RTX 그래픽카드가 필요합니다.");
	case UDLSSSupport::NotSupportedDriverOutOfDate:
		return LOCTEXT("DLSSDriver", "DLSS를 사용하려면 NVIDIA 그래픽 드라이버를 업데이트해 주세요.");
	case UDLSSSupport::NotSupportedOperatingSystemOutOfDate:
		return LOCTEXT("DLSSOS", "현재 운영체제에서는 DLSS를 사용할 수 없습니다. 운영체제 업데이트가 필요합니다.");
	case UDLSSSupport::NotSupportedIncompatibleAPICaptureToolActive:
		return LOCTEXT("DLSSCapture", "그래픽 캡처 도구가 실행 중이어서 DLSS를 사용할 수 없습니다.");
	default:
		break;
	}
#endif
	return LOCTEXT("DLSSUnavailable", "현재 실행 환경에서는 DLSS를 사용할 수 없습니다.");
}

void UHBSettingsSubsystem::SaveDLSSMode(int32 Mode, bool bQualityChanged)
{
	if (bDLSSApplied && bQualityChanged)
	{
		float Normalized = 1.0f, Minimum = 0.0f, Maximum = 100.0f;
		UGameUserSettings::GetGameUserSettings()->GetResolutionScaleInformationEx(Normalized, PreviousScreenPercentage, Minimum, Maximum);
	}
	DLSSMode = IsDLSSModeAvailable(Mode) ? Mode : 0;
	ApplyDLSS();
	SaveConfig();
}

void UHBSettingsSubsystem::ApplyDLSS()
{
#if HB_WITH_DLSS
	if (!UDLSSLibrary::IsDLSSSupported()) return;
	IConsoleVariable* AA = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
	IConsoleVariable* Screen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	if (!AA || !Screen) return;
	const bool bEnable = DLSSMode != 0 && IsDLSSModeAvailable(DLSSMode);
	if (bEnable)
	{
		const UDLSSMode Modes[] = {UDLSSMode::Off, UDLSSMode::Quality, UDLSSMode::Balanced, UDLSSMode::Performance};
		bool bSupported = false, bFixed = false;
		float Optimal = 100.0f, Min = 0.0f, Max = 100.0f, Sharpness = 0.0f;
		UGameUserSettings* Graphics = UGameUserSettings::GetGameUserSettings();
		const FIntPoint Resolution = Graphics->GetScreenResolution();
		UDLSSLibrary::GetDLSSModeInformation(Modes[DLSSMode], FVector2D(Resolution.X, Resolution.Y),
			bSupported, Optimal, bFixed, Min, Max, Sharpness);
		if (!bSupported) return;
		if (!bDLSSApplied)
		{
			PreviousAntiAliasing = AA->GetInt();
			PreviousScreenPercentage = Screen->GetFloat();
		}
		Graphics->SetDynamicResolutionEnabled(false);
		// DLSS needs a temporal AA path; restore the project's previous path when disabled.
		AA->Set(2, ECVF_SetByGameOverride);
		Screen->Set(Optimal, ECVF_SetByGameOverride);
		UDLSSLibrary::EnableDLSS(true);
		bDLSSApplied = true;
	}
	else
	{
		UDLSSLibrary::EnableDLSS(false);
		if (bDLSSApplied)
		{
			AA->Set(PreviousAntiAliasing, ECVF_SetByGameOverride);
			Screen->Set(PreviousScreenPercentage, ECVF_SetByGameOverride);
			bDLSSApplied = false;
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE

void UHBSettingsSubsystem::SavePreferences(float Master, float Music, float Effects, float Sensitivity, bool bCameraShake)
{
	MasterVolume = FMath::Clamp(Master, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(Music, 0.0f, 1.0f);
	EffectsVolume = FMath::Clamp(Effects, 0.0f, 1.0f);
	MouseSensitivity = FMath::Clamp(Sensitivity, 0.1f, 3.0f);
	bCombatCameraShake = bCameraShake;
	SaveConfig();
	ApplyAudio(GetWorld());
}

void UHBSettingsSubsystem::SaveCameraMotionScale(float InScale)
{
	CameraMotionScale = FMath::Clamp(InScale, 0.0f, 1.0f);
	SaveConfig();
}

void UHBSettingsSubsystem::ApplyAudio(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer || !VolumeMix || !MusicClass || !EffectsClass)
	{
		return;
	}
	// Multiply the master into each category once, avoiding inherited double attenuation.
	UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, MusicClass, MasterVolume * MusicVolume, 1.0f, 0.1f, true);
	UGameplayStatics::SetSoundMixClassOverride(World, VolumeMix, EffectsClass, MasterVolume * EffectsVolume, 1.0f, 0.1f, true);
	const FAudioDeviceHandle WorldAudioDevice = World->GetAudioDevice();
	if (WorldAudioDevice.IsValid() && (!SettingsAudioDevice.IsValid() || SettingsAudioDevice.GetDeviceID() != WorldAudioDevice.GetDeviceID()))
	{
		if (SettingsAudioDevice.IsValid())
		{
			SettingsAudioDevice->PopSoundMixModifier(VolumeMix);
		}
		// Map travel can reuse the audio device. Push once per device, not once per map.
		SettingsAudioDevice = WorldAudioDevice;
		SettingsAudioDevice->PushSoundMixModifier(VolumeMix);
	}
}
