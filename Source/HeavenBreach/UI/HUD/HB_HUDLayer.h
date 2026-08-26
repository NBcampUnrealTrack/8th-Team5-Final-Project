#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_HUDLayer.generated.h"

class UImage;
class UMaterialInstanceDynamic;

/**
 * 인게임 HUD 루트. UI.Layer.Game 스택에 항상 활성 상태로 올라간다.
 *
 * 기존 UHB_HUDWidget의 HP바 로직을 그대로 옮겨온 것이며,
 * 차이는 UUserWidget → UHB_ActivatableWidget으로 바뀌면서
 * 입력 모드(GameOnly)를 스스로 선언하게 된 점이다.
 */
UCLASS()
class HEAVENBREACH_API UHB_HUDLayer : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UHB_HUDLayer();

	UFUNCTION(BlueprintCallable, Category = "UI|GAS")
	void BindPlayerState();

protected:
	virtual void NativeConstruct() override;

	// HP 감소를 프레임마다 보간해서 부드럽게 줄이기 위해 Tick 사용
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void UpdateHPBar(float CurrentHP, float MaxHP);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_HP_Bar_Fill;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HPBarDynamicMaterial;

	float TargetPercent = 1.0f;

	float CurrentPercent = 1.0f;

	UPROPERTY(EditAnywhere, Category = "UI|Animation")
	float InterpSpeed = 5.0f;

	bool bIsInitialized = false;
};
