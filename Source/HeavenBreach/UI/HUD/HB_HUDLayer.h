#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_HUDLayer.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UEquipmentComponent;
class UHB_HitEffectWidget;
class UTextBlock;

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

	/**
	 * 레벨 표시 갱신.
	 * TXT_Level 이 있으면 숫자를 넣고, 블루프린트에도 이벤트를 보낸다.
	 */
	UFUNCTION()
	void UpdateLevel(int32 NewLevel);

	/** 경험치 바 갱신. Percent 는 0~1 */
	UFUNCTION()
	void UpdateExperience(int32 CurrentExperience, int32 ExperienceToNextLevel, float ExperiencePercent);

	/**
	 * 피격 연출 재생.
	 * HP 감소를 감지하면 자동으로 호출되므로 보통 직접 부를 일은 없다.
	 * 낙하 피해처럼 HP 델리게이트를 타지 않는 연출이 필요할 때만 BP 에서 호출한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|HitEffect")
	void PlayHitEffect();

	/** 텍스트 위젯 없이 BP 에서 직접 표시하고 싶을 때 사용 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Progression", meta = (DisplayName = "On Level Changed"))
	void BP_OnLevelChanged(int32 NewLevel);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Progression", meta = (DisplayName = "On Experience Changed"))
	void BP_OnExperienceChanged(int32 CurrentExperience, int32 ExperienceToNextLevel, float ExperiencePercent);

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

	/** 컨트롤 회전(카메라 시야 방향)을 나침반 머티리얼의 Heading 파라미터로 보낸다 */
	void UpdateCompass();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_HP_Bar_Fill;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_EquipWeapon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_EquipSubWeapon;

	/** 경험치 바. Percent 스칼라 파라미터를 가진 머티리얼이 지정돼 있어야 채워진다 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_Experience;

	/** 방위 스트립. Heading 스칼라 파라미터로 UV 를 미는 머티리얼이 필요하다 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_Compass;

	/** 레벨 숫자. 없으면 BP_OnLevelChanged 로만 알린다 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Level;

	/**
	 * 피격 혈흔 오버레이. WBP_MainHUD 캔버스에 풀스크린으로 배치하면 바인딩된다.
	 * 배치하지 않아도 HUD 는 정상 동작하고 연출만 생략된다
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UHB_HitEffectWidget> WBP_PlayerBloodSplatter;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HPBarDynamicMaterial;

	float TargetPercent = 1.0f;

	float CurrentPercent = 1.0f;

	UPROPERTY(EditAnywhere, Category = "UI|Animation")
	float InterpSpeed = 5.0f;

	bool bIsInitialized = false;

	/** 직전 프레임의 HP. 이 값보다 낮아지면 피격으로 간주한다 */
	float LastHP = 0.0f;

	FTimerHandle BindRetryTimerHandle;

	/** PlayerState 복제를 기다리는 재시도 간격(초) */
	UPROPERTY(EditAnywhere, Category = "UI|GAS")
	float BindRetryInterval = 0.1f;


	UPROPERTY()
	TObjectPtr<UEquipmentComponent> CachedEquipComp;

	bool bIsEquipBound = false;

	FSlateBrush DefaultWeaponBrush;
	FSlateBrush DefaultSubWeaponBrush;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ExpBarDynamicMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CompassDynamicMaterial;

	float ExpTargetPercent = 0.0f;

	float ExpCurrentPercent = 0.0f;

	bool bIsExpInitialized = false;

	/** 나침반 머티리얼의 스칼라 파라미터 이름 */
	UPROPERTY(EditAnywhere, Category = "UI|Compass")
	FName CompassHeadingParam = TEXT("Heading");

	/** 스트립이 반대로 흐르면 체크. 머티리얼 수정 없이 방향만 뒤집는다 */
	UPROPERTY(EditAnywhere, Category = "UI|Compass")
	bool bInvertCompassDirection = false;
};
