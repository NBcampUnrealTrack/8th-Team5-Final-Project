#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HB_MapLevelEntry.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FHBMapEntryClicked, int32);

UCLASS(Abstract, Blueprintable)
class HEAVENBREACH_API UHB_MapLevelEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InIndex, FName InCode, const FText& InName, FHBMapEntryClicked InHandler);
	void SetState(bool bInGenerated, bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Map")
	FName GetLevelCode() const { return Code; }

protected:
	virtual void NativeOnInitialized() override;

	
	UFUNCTION(BlueprintNativeEvent, Category = "Map")
	void OnStateChanged(bool bGenerated, bool bSelected);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Entry;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Dot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Code;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor DotOnColor = FLinearColor(1.f, 0.55f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor DotOffColor = FLinearColor(0.034f, 0.026f, 0.017f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor NameOnColor = FLinearColor(0.84f, 0.76f, 0.61f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor NameOffColor = FLinearColor(0.19f, 0.15f, 0.09f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor SelectedTint = FLinearColor(1.f, 0.75f, 0.35f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Style")
	FLinearColor NormalTint = FLinearColor(0.35f, 0.3f, 0.25f, 1.f);

private:
	UFUNCTION()
	void HandleClicked();

	int32 Index = INDEX_NONE;
	FName Code;
	FHBMapEntryClicked Handler;
};
