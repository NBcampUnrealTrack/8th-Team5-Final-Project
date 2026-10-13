#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_MapWindow.generated.h"

class UButton;
class UImage;
class UPanelWidget;
class UTextBlock;
class UTexture2D;
class UWidget;
class UHB_MapLevelEntry;

UCLASS(Abstract, Blueprintable)
class HEAVENBREACH_API UHB_MapWindow : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UHB_MapWindow();

	UFUNCTION(BlueprintPure, Category = "Map")
	int32 GetSelectedTier() const { return SelectedTier; }

	UFUNCTION(BlueprintPure, Category = "Map")
	FName GetSelectedLevel() const;

	UFUNCTION(BlueprintPure, Category = "Map")
	int32 GetSelectedLayer() const { return SelectedLayer; }

	UFUNCTION(BlueprintPure, Category = "Map")
	bool IsIsoShown() const { return bIsoShown; }

	UFUNCTION(BlueprintCallable, Category = "Map")
	void SelectTier(int32 Tier);

	UFUNCTION(BlueprintCallable, Category = "Map")
	void SelectLevel(FName LevelCode);

	UFUNCTION(BlueprintCallable, Category = "Map")
	void SelectLayer(int32 LayerIndex);

	UFUNCTION(BlueprintCallable, Category = "Map")
	void SetIsoShown(bool bShow);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	
	UFUNCTION(BlueprintImplementableEvent, Category = "Map")
	void OnMapRefreshed();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSubclassOf<UHB_MapLevelEntry> LevelEntryClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor SelectedTint = FLinearColor(1.f, 0.75f, 0.35f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor NormalTint = FLinearColor(0.35f, 0.3f, 0.25f, 1.f);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Tier1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Tier2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> List_Levels;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LevelName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_LevelEnglish;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Layer1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Layer2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Layer3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Plan;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_IsoHidden;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_IsoShown;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_ShowIso;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_HideIso;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Iso;

private:
	struct FLevelInfo
	{
		FName Code;
		int32 Tier = 1;
		FText KoreanName;
		int32 LayerCount = 1;
	};

	void BuildEntries();
	void CollectRunState();
	void Refresh();
	UTexture2D* LoadMapTexture(const FString& Folder, const FString& AssetName);
	void TintButton(UButton* Button, bool bSelected, bool bVisible = true);
	void OnEntryClicked(int32 Index);

	UFUNCTION()
	void HandleTier1();

	UFUNCTION()
	void HandleTier2();

	UFUNCTION()
	void HandleLayer1();

	UFUNCTION()
	void HandleLayer2();

	UFUNCTION()
	void HandleLayer3();

	UFUNCTION()
	void HandleShowIso();

	UFUNCTION()
	void HandleHideIso();

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> TextureCache;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHB_MapLevelEntry>> Entries;

	TArray<FLevelInfo> Levels;
	TSet<FName> GeneratedLevels;
	int32 SelectedTier = 1;
	int32 SelectedLevel = 0;
	int32 SelectedLayer = 0;
	bool bIsoShown = false;
};
