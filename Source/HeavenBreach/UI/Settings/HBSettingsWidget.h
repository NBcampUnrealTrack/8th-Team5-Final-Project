#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HBSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class USlider;
class UTextBlock;
class UWidgetSwitcher;
class UHBSettingsSubsystem;

/** Designer-owned layout with local, transactional settings behavior. */
UCLASS(Abstract)
class HEAVENBREACH_API UHBSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetReturnFocusWidget(UWidget* Widget) { ReturnFocusWidget = Widget; }
	UFUNCTION(BlueprintCallable, Category="Settings")
	void CloseSettings();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Display;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Audio;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Controls;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Defaults;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Cancel;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Apply;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Close;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> SettingsPages;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> Combo_WindowMode;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> Combo_Resolution;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> Combo_Quality;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> Combo_FrameRate;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> Combo_DLSS;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Check_VSync;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UCheckBox> Check_CameraShake;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_Master;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_Music;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_Effects;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> Slider_Sensitivity;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Master;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Music;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Effects;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Sensitivity;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_VSync;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_CameraShake;
	// 카메라 움직임(머리 흔들림) 0~100%. WBP 에 아직 없을 수 있어 선택 항목이다. 없으면 기존 값을 유지한다.
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USlider> Slider_CameraMotion;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_CameraMotion;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Description;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> VideoConfirmation;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_VideoCountdown;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_KeepVideo;
	UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_RevertVideo;

private:
	UFUNCTION() void ShowDisplay();
	UFUNCTION() void ShowAudio();
	UFUNCTION() void ShowControls();
	UFUNCTION() void ResetCurrentTab();
	UFUNCTION() void ApplyChanges();
	UFUNCTION() void KeepVideo();
	UFUNCTION() void RevertVideo();
	UFUNCTION() void OnSliderChanged(float Value);
	UFUNCTION() void OnToggleChanged(bool bChecked);
	UFUNCTION() void OnWindowModeChanged(FString Value, ESelectInfo::Type SelectionType);
	void SelectTab(int32 Index);
	void LoadValues();
	void RefreshValueLabels();
	void CommitChanges();
	void PopulateResolutions(FIntPoint SelectedResolution);
	UHBSettingsSubsystem* GetSettings() const;

	TArray<FIntPoint> Resolutions;
	TArray<float> FrameRates;
	TArray<int32> DLSSModes;
	int32 InitialQuality = -1;
	bool bLoadingValues = false;
	bool bVideoPending = false;
	double VideoDeadline = 0.0;
	FIntPoint PreviousResolution;
	int32 PreviousWindowMode = 0;
	TWeakObjectPtr<UWidget> LastDescribedWidget;
	TWeakObjectPtr<UWidget> ReturnFocusWidget;
};
