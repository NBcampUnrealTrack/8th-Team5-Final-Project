#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_HUDLayer.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UEquipmentComponent;

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

	/**
	 * PlayerState의 HP 변경 델리게이트에 연결한다.
	 * 클라이언트에서는 위젯 생성 시점에 PlayerState가 아직 복제되지 않았을 수 있어,
	 * 실패하면 성공할 때까지 짧은 간격으로 재시도한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|GAS")
	void BindPlayerState();

	UFUNCTION()
	void UpdateEquipmentSlots();

protected:
	/** 실제 바인딩 시도. PlayerState와 AttributeSet이 준비됐으면 true */
	bool TryBindPlayerState();

	void HandleBindRetry();

	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	// HP 감소를 프레임마다 보간해서 부드럽게 줄이기 위해 Tick 사용
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void UpdateHPBar(float CurrentHP, float MaxHP);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_HP_Bar_Fill;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_EquipWeapon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_EquipSubWeapon;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HPBarDynamicMaterial;

	float TargetPercent = 1.0f;

	float CurrentPercent = 1.0f;

	UPROPERTY(EditAnywhere, Category = "UI|Animation")
	float InterpSpeed = 5.0f;

	bool bIsInitialized = false;

	FTimerHandle BindRetryTimerHandle;

	/** PlayerState 복제를 기다리는 재시도 간격(초) */
	UPROPERTY(EditAnywhere, Category = "UI|GAS")
	float BindRetryInterval = 0.1f;


	UPROPERTY()
	TObjectPtr<UEquipmentComponent> CachedEquipComp;

	bool bIsEquipBound = false;

	FSlateBrush DefaultWeaponBrush;
	FSlateBrush DefaultSubWeaponBrush;
};
