#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HBRoomNameWidget.generated.h"

class UTextBlock;

UCLASS()
class HEAVENBREACH_API UHBRoomNameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RoomName")
	void ShowRoomName(const FText& InName);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomName")
	float FadeInSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomName")
	float HoldSeconds = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomName")
	float FadeOutSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomName")
	float TopOffset = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomName")
	int32 FontSize = 40;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	float Elapsed = 0.f;
	bool bPlaying = false;

	void ApplyAlpha(float Alpha);
};
