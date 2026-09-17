#include "UI/Settings/HBSettingsWidget.h"

#include "UI/Settings/HBSettingsSubsystem.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "HBSettings"

namespace
{
	void ResetOptions(UComboBoxString* Combo)
	{
		// UE 5.7 ClearOptions clears the Slate content but retains SelectedOption.
		// Reset that value too so selecting the same label regenerates its text.
		Combo->AddOption(TEXT(""));
		Combo->SetSelectedOption(TEXT(""));
		Combo->ClearOptions();
	}
}

UHBSettingsSubsystem* UHBSettingsWidget::GetSettings() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UHBSettingsSubsystem>() : nullptr;
}

void UHBSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	Button_Display->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowDisplay);
	Button_Audio->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowAudio);
	Button_Controls->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowControls);
	Button_Defaults->OnClicked.AddUniqueDynamic(this, &ThisClass::ResetCurrentTab);
	Button_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseSettings);
	Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseSettings);
	Button_Apply->OnClicked.AddUniqueDynamic(this, &ThisClass::ApplyChanges);
	Button_KeepVideo->OnClicked.AddUniqueDynamic(this, &ThisClass::KeepVideo);
	Button_RevertVideo->OnClicked.AddUniqueDynamic(this, &ThisClass::RevertVideo);
	Combo_WindowMode->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::OnWindowModeChanged);
	for (USlider* Slider : {Slider_Master.Get(), Slider_Music.Get(), Slider_Effects.Get(), Slider_Sensitivity.Get()})
	{
		Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::OnSliderChanged);
	}
	Check_VSync->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::OnToggleChanged);
	Check_CameraShake->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::OnToggleChanged);
}

void UHBSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	VideoConfirmation->SetVisibility(ESlateVisibility::Collapsed);
	LoadValues();
	Text_Status->SetText(LOCTEXT("PendingHint", "변경 후 적용을 눌러 저장하세요."));
	SelectTab(0);
	SetKeyboardFocus();
}

void UHBSettingsWidget::NativeDestruct()
{
	if (bVideoPending)
	{
		RevertVideo();
	}
	Super::NativeDestruct();
}

void UHBSettingsWidget::PopulateResolutions(FIntPoint SelectedResolution)
{
	Resolutions.Reset();
	if (Combo_WindowMode->GetSelectedIndex() == 2)
	{
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Resolutions);
	}
	else
	{
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	}
	const UGameUserSettings* Graphics = UGameUserSettings::GetGameUserSettings();
	if (SelectedResolution.X <= 0 || SelectedResolution.Y <= 0)
	{
		SelectedResolution = Graphics->GetDesktopResolution();
	}
	if (SelectedResolution.X <= 0 || SelectedResolution.Y <= 0)
	{
		SelectedResolution = FIntPoint(1280, 720);
	}
	Resolutions.AddUnique(SelectedResolution);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X != B.X ? A.X < B.X : A.Y < B.Y;
	});
	ResetOptions(Combo_Resolution);
	for (const FIntPoint& Resolution : Resolutions)
	{
		Combo_Resolution->AddOption(FString::Printf(TEXT("%d × %d"), Resolution.X, Resolution.Y));
	}
	Combo_Resolution->SetSelectedIndex(Resolutions.IndexOfByKey(SelectedResolution));
	// Borderless uses desktop dimensions; the selectable render scale belongs to DLSS.
	Combo_Resolution->SetIsEnabled(Combo_WindowMode->GetSelectedIndex() != 1);
}

void UHBSettingsWidget::LoadValues()
{
	UGameUserSettings* Graphics = UGameUserSettings::GetGameUserSettings();
	UHBSettingsSubsystem* Settings = GetSettings();
	if (!Graphics || !Settings)
	{
		Button_Apply->SetIsEnabled(false);
		return;
	}
	bLoadingValues = true;
	ResetOptions(Combo_WindowMode);
	for (const TCHAR* Name : {TEXT("전체 화면"), TEXT("테두리 없는 창"), TEXT("창 모드")})
	{
		Combo_WindowMode->AddOption(Name);
	}
	Combo_WindowMode->SetSelectedIndex(static_cast<int32>(Graphics->GetFullscreenMode()));
	PopulateResolutions(Graphics->GetFullscreenMode() == EWindowMode::WindowedFullscreen
		? Graphics->GetDesktopResolution() : Graphics->GetScreenResolution());
	ResetOptions(Combo_Quality);
	for (const TCHAR* Name : {TEXT("낮음"), TEXT("보통"), TEXT("높음"), TEXT("매우 높음")})
	{
		Combo_Quality->AddOption(Name);
	}
	InitialQuality = Graphics->GetOverallScalabilityLevel();
	if (InitialQuality < 0 || InitialQuality > 3)
	{
		Combo_Quality->AddOption(TEXT("사용자 지정"));
		Combo_Quality->SetSelectedIndex(4);
	}
	else
	{
		Combo_Quality->SetSelectedIndex(InitialQuality);
	}
	FrameRates = {30.0f, 60.0f, 120.0f, 144.0f, 0.0f};
	const float CurrentRate = Graphics->GetFrameRateLimit();
	FrameRates.AddUnique(CurrentRate);
	ResetOptions(Combo_FrameRate);
	for (float Rate : FrameRates)
	{
		Combo_FrameRate->AddOption(Rate == 0.0f ? TEXT("제한 없음") : FString::Printf(TEXT("%.0f FPS"), Rate));
	}
	Combo_FrameRate->SetSelectedIndex(FrameRates.IndexOfByKey(CurrentRate));
	Check_VSync->SetIsChecked(Graphics->IsVSyncEnabled());
	Slider_Master->SetValue(Settings->GetMasterVolume());
	Slider_Music->SetValue(Settings->GetMusicVolume());
	Slider_Effects->SetValue(Settings->GetEffectsVolume());
	Slider_Sensitivity->SetMinValue(0.1f);
	Slider_Sensitivity->SetMaxValue(3.0f);
	Slider_Sensitivity->SetValue(Settings->GetMouseSensitivity());
	Check_CameraShake->SetIsChecked(Settings->IsCombatCameraShakeEnabled());
	ResetOptions(Combo_DLSS);
	Combo_DLSS->AddOption(TEXT("끄기"));
	DLSSModes = {0};
	const TCHAR* ModeNames[] = {TEXT("끄기"), TEXT("품질"), TEXT("균형"), TEXT("성능")};
	for (int32 Mode = 1; Mode < 4; ++Mode)
	{
		if (Settings->IsDLSSModeAvailable(Mode))
		{
			DLSSModes.Add(Mode);
			Combo_DLSS->AddOption(ModeNames[Mode]);
		}
	}
	Combo_DLSS->SetSelectedIndex(FMath::Max(0, DLSSModes.IndexOfByKey(Settings->GetDLSSMode())));
	Combo_DLSS->SetIsEnabled(DLSSModes.Num() > 1);
	Combo_DLSS->SetToolTipText(Settings->GetDLSSStatusText());
	if (UWidget* Row = GetWidgetFromName(TEXT("Row_DLSS"))) Row->SetToolTipText(Settings->GetDLSSStatusText());
	if (DLSSModes.Num() == 1)
	{
		ResetOptions(Combo_DLSS);
		Combo_DLSS->AddOption(TEXT("지원되지 않음"));
		Combo_DLSS->SetSelectedIndex(0);
	}
	bLoadingValues = false;
	RefreshValueLabels();
}

void UHBSettingsWidget::ShowDisplay() { SelectTab(0); }
void UHBSettingsWidget::ShowAudio() { SelectTab(1); }
void UHBSettingsWidget::ShowControls() { SelectTab(2); }

void UHBSettingsWidget::SelectTab(int32 Index)
{
	SettingsPages->SetActiveWidgetIndex(Index);
	UButton* Tabs[] = {Button_Display, Button_Audio, Button_Controls};
	for (int32 Tab = 0; Tab < 3; ++Tab)
	{
		Tabs[Tab]->SetBackgroundColor(Tab == Index ? FLinearColor(1.0f, 0.68f, 0.22f) : FLinearColor(0.38f, 0.38f, 0.38f));
	}
	const FText Descriptions[] = {
		LOCTEXT("DisplayHelp", "화면과 그래픽 품질을 조절합니다.\n\n해상도·화면 모드 변경 후 15초 안에 유지 여부를 확인하세요. 확인하지 않으면 이전 설정으로 돌아갑니다."),
		LOCTEXT("AudioHelp", "전체 음량과 배경음, 효과음의 크기를 조절합니다.\n\n배경음과 효과음에는 전체 음량이 함께 적용됩니다."),
		LOCTEXT("ControlsHelp", "마우스 감도와 전투 화면 흔들림을 조절합니다.\n\n전투 화면 흔들림을 끄면 공격·피격 시의 카메라 흔들림이 비활성화됩니다.")
	};
	Text_Description->SetText(Descriptions[Index]);
	if (UScrollBox* DescriptionScroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ScrollBox_Description")))) DescriptionScroll->ScrollToStart();
	LastDescribedWidget.Reset();
}

void UHBSettingsWidget::RefreshValueLabels()
{
	Text_Master->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Slider_Master->GetValue() * 100))));
	Text_Music->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Slider_Music->GetValue() * 100))));
	Text_Effects->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Slider_Effects->GetValue() * 100))));
	Text_Sensitivity->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Slider_Sensitivity->GetValue())));
	Text_VSync->SetText(Check_VSync->IsChecked() ? LOCTEXT("On", "켜기") : LOCTEXT("Off", "끄기"));
	Text_CameraShake->SetText(Check_CameraShake->IsChecked() ? LOCTEXT("On", "켜기") : LOCTEXT("Off", "끄기"));
}

void UHBSettingsWidget::OnSliderChanged(float Value) { RefreshValueLabels(); }
void UHBSettingsWidget::OnToggleChanged(bool bChecked) { RefreshValueLabels(); }

void UHBSettingsWidget::OnWindowModeChanged(FString Value, ESelectInfo::Type SelectionType)
{
	if (bLoadingValues) return;
	const int32 ResolutionIndex = Combo_Resolution->GetSelectedIndex();
	FIntPoint Resolution = Resolutions.IsValidIndex(ResolutionIndex) ? Resolutions[ResolutionIndex] : FIntPoint::ZeroValue;
	if (Combo_WindowMode->GetSelectedIndex() == 1)
	{
		Resolution = UGameUserSettings::GetGameUserSettings()->GetDesktopResolution();
	}
	PopulateResolutions(Resolution);
}

void UHBSettingsWidget::ResetCurrentTab()
{
	switch (SettingsPages->GetActiveWidgetIndex())
	{
	case 0:
		Combo_WindowMode->SetSelectedIndex(1);
		PopulateResolutions(UGameUserSettings::GetGameUserSettings()->GetDesktopResolution());
		Combo_Quality->SetSelectedIndex(2);
		Combo_FrameRate->SetSelectedIndex(1);
		Check_VSync->SetIsChecked(false);
		Combo_DLSS->SetSelectedIndex(0);
		break;
	case 1:
		Slider_Master->SetValue(1.0f);
		Slider_Music->SetValue(1.0f);
		Slider_Effects->SetValue(1.0f);
		break;
	case 2:
		Slider_Sensitivity->SetValue(1.0f);
		Check_CameraShake->SetIsChecked(true);
		break;
	}
	RefreshValueLabels();
	Text_Status->SetText(LOCTEXT("DefaultsPending", "현재 탭의 기본값을 선택했습니다. 적용하면 저장됩니다."));
}

void UHBSettingsWidget::ApplyChanges()
{
	UGameUserSettings* Graphics = UGameUserSettings::GetGameUserSettings();
	if (!Graphics || !GetSettings() || bVideoPending) return;
	const int32 Index = Combo_Resolution->GetSelectedIndex();
	if (!Resolutions.IsValidIndex(Index)) return;
	PreviousResolution = Graphics->GetScreenResolution();
	PreviousWindowMode = static_cast<int32>(Graphics->GetFullscreenMode());
	const int32 Mode = Combo_WindowMode->GetSelectedIndex();
	const FIntPoint Resolution = Resolutions[Index];
	if (PreviousWindowMode != Mode || PreviousResolution != Resolution)
	{
		// Preview only. No graphics or personal preference is saved before confirmation.
		Graphics->SetFullscreenMode(static_cast<EWindowMode::Type>(Mode));
		Graphics->SetScreenResolution(Resolution);
		Graphics->ApplyResolutionSettings(false);
		bVideoPending = true;
		VideoDeadline = FPlatformTime::Seconds() + 15.0;
		VideoConfirmation->SetVisibility(ESlateVisibility::Visible);
		if (UWidget* Window = GetWidgetFromName(TEXT("Overlay_Window"))) Window->SetIsEnabled(false);
		Button_KeepVideo->SetKeyboardFocus();
	}
	else
	{
		CommitChanges();
	}
}

void UHBSettingsWidget::KeepVideo()
{
	if (!bVideoPending) return;
	bVideoPending = false;
	VideoConfirmation->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* Window = GetWidgetFromName(TEXT("Overlay_Window"))) Window->SetIsEnabled(true);
	UGameUserSettings::GetGameUserSettings()->ConfirmVideoMode();
	CommitChanges();
	Button_Apply->SetKeyboardFocus();
}

void UHBSettingsWidget::RevertVideo()
{
	if (!bVideoPending) return;
	bVideoPending = false;
	if (UGameUserSettings* Graphics = UGameUserSettings::GetGameUserSettings())
	{
		Graphics->SetFullscreenMode(static_cast<EWindowMode::Type>(PreviousWindowMode));
		Graphics->SetScreenResolution(PreviousResolution);
		Graphics->ApplyResolutionSettings(false);
	}
	VideoConfirmation->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* Window = GetWidgetFromName(TEXT("Overlay_Window"))) Window->SetIsEnabled(true);
	bLoadingValues = true;
	Combo_WindowMode->SetSelectedIndex(PreviousWindowMode);
	PopulateResolutions(PreviousResolution);
	bLoadingValues = false;
	Text_Status->SetText(LOCTEXT("VideoRestored", "이전 화면 설정으로 돌아왔습니다. 변경 내용은 저장되지 않았습니다."));
	Button_Apply->SetKeyboardFocus();
}

void UHBSettingsWidget::CommitChanges()
{
	UGameUserSettings* Graphics = UGameUserSettings::GetGameUserSettings();
	UHBSettingsSubsystem* Settings = GetSettings();
	if (!Graphics || !Settings) return;
	const int32 Quality = Combo_Quality->GetSelectedIndex();
	const bool bQualityChanged = Quality >= 0 && Quality <= 3 && Quality != InitialQuality;
	bool bGraphicsChanged = bQualityChanged || Graphics->IsVSyncEnabled() != Check_VSync->IsChecked();
	if (bQualityChanged)
	{
		Graphics->SetOverallScalabilityLevel(Quality);
		InitialQuality = Quality;
	}
	const int32 RateIndex = Combo_FrameRate->GetSelectedIndex();
	if (FrameRates.IsValidIndex(RateIndex))
	{
		bGraphicsChanged |= !FMath::IsNearlyEqual(Graphics->GetFrameRateLimit(), FrameRates[RateIndex]);
		Graphics->SetFrameRateLimit(FrameRates[RateIndex]);
	}
	Graphics->SetVSyncEnabled(Check_VSync->IsChecked());
	if (bGraphicsChanged) Graphics->ApplyNonResolutionSettings();
	const int32 DLSSIndex = Combo_DLSS->GetSelectedIndex();
	if (DLSSModes.IsValidIndex(DLSSIndex)) Settings->SaveDLSSMode(DLSSModes[DLSSIndex], bQualityChanged);
	Graphics->SaveSettings();
	Settings->SavePreferences(Slider_Master->GetValue(), Slider_Music->GetValue(), Slider_Effects->GetValue(),
		Slider_Sensitivity->GetValue(), Check_CameraShake->IsChecked());
	Text_Status->SetText(LOCTEXT("Saved", "설정을 적용하고 저장했습니다."));
}

void UHBSettingsWidget::CloseSettings()
{
	if (bVideoPending) RevertVideo();
	RemoveFromParent();
	if (ReturnFocusWidget.IsValid()) ReturnFocusWidget->SetKeyboardFocus();
}

FReply UHBSettingsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		if (bVideoPending) RevertVideo();
		else CloseSettings();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

void UHBSettingsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	const TCHAR* Rows[] = {TEXT("Row_WindowMode"), TEXT("Row_Resolution"), TEXT("Row_Quality"), TEXT("Row_FrameRate"),
		TEXT("Row_VSync"), TEXT("Row_DLSS"), TEXT("Row_Master"), TEXT("Row_Music"), TEXT("Row_Effects"),
		TEXT("Row_Sensitivity"), TEXT("Row_CameraShake")};
	const int32 PageIndex = SettingsPages->GetActiveWidgetIndex();
	const int32 FirstRow = PageIndex == 0 ? 0 : (PageIndex == 1 ? 6 : 9);
	const int32 LastRow = PageIndex == 0 ? 6 : (PageIndex == 1 ? 9 : 11);
	for (int32 RowIndex = FirstRow; RowIndex < LastRow; ++RowIndex)
	{
		UWidget* Row = GetWidgetFromName(Rows[RowIndex]);
		if (Row && Row->IsHovered() && LastDescribedWidget.Get() != Row)
		{
			Text_Description->SetText(Row->GetToolTipText());
			if (UScrollBox* DescriptionScroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ScrollBox_Description")))) DescriptionScroll->ScrollToStart();
			LastDescribedWidget = Row;
		}
	}
	if (bVideoPending)
	{
		const int32 Remaining = FMath::Max(0, FMath::CeilToInt(VideoDeadline - FPlatformTime::Seconds()));
		Text_VideoCountdown->SetText(FText::Format(LOCTEXT("KeepVideoQuestion", "이 화면 설정을 유지할까요?\n{0}초 후 이전 설정으로 돌아갑니다."), Remaining));
		if (Remaining <= 0) RevertVideo();
	}
}

#undef LOCTEXT_NAMESPACE
