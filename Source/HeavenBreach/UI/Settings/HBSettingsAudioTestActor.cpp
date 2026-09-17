#include "UI/Settings/HBSettingsAudioTestActor.h"

#include "UI/Settings/HBSettingsWidget.h"
#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#pragma region TemporarySettingsAudioTest
// TODO: Remove this isolated listening station after sound settings acceptance.
AHBSettingsAudioTestActor::AHBSettingsAudioTestActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsEditorOnlyActor = true;
	Music = CreateDefaultSubobject<UAudioComponent>(TEXT("TestMusic"));
	SetRootComponent(Music);
	Effects = CreateDefaultSubobject<UAudioComponent>(TEXT("TestEffects"));
	Effects->SetupAttachment(Music);
	for (UAudioComponent* Component : {Music.Get(), Effects.Get()})
	{
		Component->bAutoActivate = false;
		Component->bAutoDestroy = false;
		Component->bAllowSpatialization = false;
		Component->bIsUISound = true;
	}
	MusicSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sound/BGM/InGame/Where_Walls_Still_Breathe.Where_Walls_Still_Breathe")));
	EffectSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sound/SW_Slash_Flesh1.SW_Slash_Flesh1")));
}

void AHBSettingsAudioTestActor::BeginPlay()
{
	Super::BeginPlay();
	// This actor must never add audio or input to packaged games or a dedicated server.
	if (!GIsEditor || GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}
	Music->SetSound(MusicSound.LoadSynchronous());
	Effects->SetSound(EffectSound.LoadSynchronous());
}

void AHBSettingsAudioTestActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInputReady)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (!PC || !PC->IsLocalController()) return;
		LocalController = PC;
		EnableInput(PC);
		InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ThisClass::ToggleSettings);
		InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ThisClass::ToggleMusic);
		InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ThisClass::ToggleEffectsLoop);
		InputComponent->BindKey(EKeys::Insert, IE_Pressed, this, &ThisClass::PlayEffect);
		bInputReady = true;
		if (bStartMusic) ToggleMusic();
	}
	if (bSettingsInput && (!SettingsWidget || !SettingsWidget->IsInViewport())) RestoreGameInput();
	GuideElapsed += DeltaSeconds;
	if (GuideElapsed >= 0.5f)
	{
		GuideElapsed = 0.0f;
		UpdateGuide();
	}
}

void AHBSettingsAudioTestActor::ToggleSettings()
{
	APlayerController* PC = LocalController.Get();
	if (!PC) return;
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		SettingsWidget->CloseSettings();
		RestoreGameInput();
		return;
	}
	const TSubclassOf<UHBSettingsWidget> WidgetClass = LoadClass<UHBSettingsWidget>(nullptr, TEXT("/Game/UI/Settings/WBP_Settings.WBP_Settings_C"));
	if (!WidgetClass) return;
	SettingsWidget = CreateWidget<UHBSettingsWidget>(PC, WidgetClass);
	if (!SettingsWidget) return;
	bPreviousMouseCursor = PC->bShowMouseCursor;
	SettingsWidget->AddToViewport(1000);
	if (UButton* AudioTab = Cast<UButton>(SettingsWidget->GetWidgetFromName(TEXT("Button_Audio"))))
	{
		AudioTab->OnClicked.Broadcast();
	}
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	bSettingsInput = true;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(SettingsWidget->TakeWidget());
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
}

void AHBSettingsAudioTestActor::RestoreGameInput()
{
	if (bSettingsInput)
	{
		if (APlayerController* PC = LocalController.Get())
		{
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = bPreviousMouseCursor;
		}
		bSettingsInput = false;
	}
	SettingsWidget = nullptr;
}

void AHBSettingsAudioTestActor::ToggleMusic()
{
	if (!Music->Sound) return;
	bMusicEnabled = !bMusicEnabled;
	if (bMusicEnabled) Music->Play();
	else Music->Stop();
	UpdateGuide();
}

void AHBSettingsAudioTestActor::ToggleEffectsLoop()
{
	if (GetWorldTimerManager().IsTimerActive(EffectsTimer))
	{
		GetWorldTimerManager().ClearTimer(EffectsTimer);
		Effects->Stop();
	}
	else
	{
		PlayEffect();
		GetWorldTimerManager().SetTimer(EffectsTimer, this, &ThisClass::PlayEffect, FMath::Max(0.2f, EffectsInterval), true);
	}
	UpdateGuide();
}

void AHBSettingsAudioTestActor::PlayEffect()
{
	// Reuse one component, avoiding overlapping samples and spawned-component buildup.
	if (Effects->Sound) Effects->Play();
}

void AHBSettingsAudioTestActor::UpdateGuide()
{
	if (!GEngine) return;
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 1.0f, FColor::Cyan,
		FString::Printf(TEXT("AUDIO TEST | F6: Settings | F7: BGM [%s] | F10: SFX loop [%s] | Insert: SFX once\nChange volumes in Audio, then click Apply. F6 or X: return to gameplay."),
			bMusicEnabled ? TEXT("ON") : TEXT("OFF"), GetWorldTimerManager().IsTimerActive(EffectsTimer) ? TEXT("ON") : TEXT("OFF")));
}

void AHBSettingsAudioTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(EffectsTimer);
	Music->Stop();
	Effects->Stop();
	if (SettingsWidget) SettingsWidget->CloseSettings();
	RestoreGameInput();
	if (GEngine) GEngine->RemoveOnScreenDebugMessage(GetUniqueID());
	Super::EndPlay(EndPlayReason);
}
#pragma endregion TemporarySettingsAudioTest
