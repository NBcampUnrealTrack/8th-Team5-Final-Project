#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_HUDLayer.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UEquipmentComponent;
class UQuickSlotComponent;
class UHB_HitEffectWidget;
class UTextBlock;
class UTextureRenderTarget2D;
class UInventoryWidget;
class UHB_NotificationWidget;
class UBorder;
class UProgressBar;

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

	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void OpenInventory();

	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void CloseInventory();

	/** 게임 코드(컨트롤러)가 잠깐 뜨는 알림을 띄울 때. WBP_Notification 이 없으면 아무것도 안 한다 */
	void ShowNotification(const FText& Header, const FText& Body);

	UFUNCTION(BlueprintPure, Category = "UI|Inventory")
	bool IsInventoryOpen() const;

	/**
	 * PlayerState의 HP 변경 델리게이트에 연결한다.
	 * 클라이언트에서는 위젯 생성 시점에 PlayerState가 아직 복제되지 않았을 수 있어,
	 * 실패하면 성공할 때까지 짧은 간격으로 재시도한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|GAS")
	void BindPlayerState();

	UFUNCTION()
	void UpdateEquipmentSlots();
	UFUNCTION()
	void UpdateQuickSlots();

	/** 퀵슬롯 포션 쿨타임 진행바 갱신. 쿨타임은 아이템 종류(RowName)별로 독립이라 각 슬롯이 들고 있는
	 * 실제 아이템을 조회해서 따로 갱신한다 - 서로 다른 포션이면 한쪽만 쿨타임이 돌 수 있다. 매 틱 호출됨. */
	void UpdateQuickSlotCooldown();

	/**
	 * 레벨 표시 갱신.
	 * TXT_Level 이 있으면 숫자를 넣고, 블루프린트에도 이벤트를 보낸다.
	 */
	UFUNCTION()
	void UpdateLevel(int32 NewLevel);

	/**
	 * 좌하단 주무기/보조무기 표시 갱신. 두 칸을 항상 같이 보여주되, 지금 든 무기
	 * 쪽만 배경/아이콘을 진하게, 키 뱃지를 밝게 강조하고 반대쪽은 흐리게 만든다.
	 * true 면 주무기 강조, false 면 보조무기 강조.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Weapon")
	void SetActiveWeaponSlot(bool bIsMainActive);

	/** 경험치 바 갱신. Percent 는 0~1 */
	UFUNCTION()
	void UpdateExperience(int32 CurrentExperience, int32 ExperienceToNextLevel, float ExperiencePercent);

	/**
	 * 피격 연출 재생.
	 * AHBPlayerController::ClientPlayHitEffect 가 피격 시점에 호출한다.
	 * HP 어트리뷰트로 트리거하지 않는 이유는 그쪽 선언부 주석 참고.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|HitEffect")
	void PlayHitEffect();

	/**
	 * HP 바 갱신.
	 * PlayerState 의 OnHPChanged 델리게이트가 호출하지만, 그 경로는 PlayerState
	 * 복제 주기(1Hz) 때문에 최대 1초 늦다. 피격처럼 즉각적인 반응이 필요한 경우
	 * AHBPlayerController::ClientPlayHitEffect 가 서버 값으로 먼저 호출한다.
	 * 뒤늦게 도착하는 복제본은 같은 값이라 덧칠해도 티가 나지 않는다.
	 */
	UFUNCTION()
	void UpdateHPBar(float CurrentHP, float MaxHP);

	UFUNCTION()
	void UpdateMPBar(float CurrentMP, float MaxMP);

	UFUNCTION(BlueprintCallable, Category = "UI|Minimap")
	void SetMinimapTexture(UTextureRenderTarget2D* InMinimapRenderTarget);

	/** 텍스트 위젯 없이 BP 에서 직접 표시하고 싶을 때 사용 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Progression", meta = (DisplayName = "On Level Changed"))
	void BP_OnLevelChanged(int32 NewLevel);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Progression", meta = (DisplayName = "On Experience Changed"))
	void BP_OnExperienceChanged(int32 CurrentExperience, int32 ExperienceToNextLevel, float ExperiencePercent);

	/**
	 * 고를 수 있는 증강 선택권이 있는지 없는지 "상태가 바뀔 때"만 호출된다(매번이 아님).
	 * 여기서 스킬 증강 아이콘의 깜빡임 애니메이션을 Play Animation(Loop)으로 켜고,
	 * false 로 오면 Stop Animation 으로 끄면 된다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Progression", meta = (DisplayName = "On Pending Upgrade Changed"))
	void BP_OnPendingUpgradeChanged(bool bHasPendingUpgrade);

protected:
	/** 실제 바인딩 시도. PlayerState와 AttributeSet이 준비됐으면 true */
	bool TryBindPlayerState();

	void HandleBindRetry();

	/** PS->OnSkillUpgradesChanged 에 물리는 핸들러. 상태가 실제로 바뀔 때만 BP_OnPendingUpgradeChanged 를 부른다. */
	UFUNCTION()
	void HandleSkillUpgradesChanged();

	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	// HP 감소를 프레임마다 보간해서 부드럽게 줄이기 위해 Tick 사용
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 컨트롤 회전(카메라 시야 방향)을 나침반 머티리얼의 Heading 파라미터로 보낸다 */
	void UpdateCompass();

	/** 서버 기준 남은 시간을 MM:SS로 표시한다. */
	void UpdateRemainingTime();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_HP_Bar_Fill;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_MP_Bar_Fill;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_EquipWeapon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_EquipSubWeapon;
	/** 퀵슬롯 3번(포션). QuickSlotComponent 슬롯 인덱스 2에 대응 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_Potion_1;
	/** 퀵슬롯 4번(포션). QuickSlotComponent 슬롯 인덱스 3에 대응 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_Potion_2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Potion_1_Num;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Potion_2_Num;
	/** 퀵슬롯 3번(포션) 쿨타임 진행바. WBP에서 아이콘 위에 겹쳐 놓고, 위젯 자체 Style에서 Fill Type을
	 * Radial(원형)로 설정하면 원형 쿨타임으로, Linear로 두면 바 형태로 보인다 — 코드는 Percent만 채운다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Potion_1_Cooldown;
	/** 퀵슬롯 4번(포션) 쿨타임 진행바. ProgressBar_Potion_1_Cooldown 참고. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Potion_2_Cooldown;
	/** 경험치 바. Percent 스칼라 파라미터를 가진 머티리얼이 지정돼 있어야 채워진다 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_Experience;

	/* 미니맵. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_Minimap;

	/** 방위 스트립. Heading 스칼라 파라미터로 UV 를 미는 머티리얼이 필요하다 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_Compass;

	/** 레벨 숫자. 없으면 BP_OnLevelChanged 로만 알린다 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Level;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Remaining_Time;

	/**
	 * 피격 혈흔 오버레이. WBP_MainHUD 캔버스에 풀스크린으로 배치하면 바인딩된다.
	 * 배치하지 않아도 HUD 는 정상 동작하고 연출만 생략된다
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UHB_HitEffectWidget> WBP_PlayerBloodSplatter;

	/** Player inventory embedded in WBP_MainHUD. Hidden while gameplay is active. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UInventoryWidget> WBP_Inventory;

	/** 레벨업 등 잠깐 떴다 사라지는 알림. 없어도 HUD 는 정상 동작하고 알림만 생략된다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UHB_NotificationWidget> WBP_Notification;

	// ---- 주무기/보조무기 표시 ----

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_MainBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_SubBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MainWeapon_key;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SubWeapon_Key;

	/** 강조된 쪽 배경 이미지 알파. 반대쪽은 InactiveBackgroundAlpha 를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Weapon")
	float ActiveBackgroundAlpha = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Weapon")
	float InactiveBackgroundAlpha = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Weapon")
	float ActiveIconAlpha = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Weapon")
	float InactiveIconAlpha = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Weapon")
	FLinearColor ActiveKeyColor = FLinearColor(0.713542f, 0.713542f, 0.713542f, 0.965217f);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Weapon")
	FLinearColor InactiveKeyColor = FLinearColor(0.307292f, 0.307292f, 0.307292f, 0.965217f);

private:
	// -2: 초기 갱신 전, -1: 타이머 정보 대기 중.
	int32 LastDisplayedRemainingSeconds = -2;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HPBarDynamicMaterial;

	float TargetPercent = 1.0f;

	float CurrentPercent = 1.0f;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MPBarDynamicMaterial;

	float MPTargetPercent = 1.0f;

	float MPCurrentPercent = 1.0f;

	bool bIsMPInitialized = false;

	UPROPERTY(EditAnywhere, Category = "UI|Animation")
	float InterpSpeed = 5.0f;

	bool bIsInitialized = false;

	/**
	 * UpdateLevel 이 "진짜로 레벨이 올랐을 때"와 "HUD 가 막 떠서 현재 레벨을 처음
	 * 동기화할 때"를 구분하기 위한 값. -1 이면 아직 한 번도 안 받아온 상태라는 뜻이라,
	 * 이때는 알림을 띄우지 않는다.
	 */
	int32 LastKnownLevel = -1;

	/** 마지막으로 BP_OnPendingUpgradeChanged 에 넘긴 값. 상태가 진짜 바뀔 때만 다시 부르기 위한 중복 방지용. */
	bool bLastPendingUpgradeState = false;

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
	TObjectPtr<UQuickSlotComponent> CachedQuickSlotComp;
	bool bIsQuickSlotBound = false;
	FSlateBrush DefaultPotion1Brush;
	FSlateBrush DefaultPotion2Brush;
	/** InventoryComponent::OnInventoryChanged 바인딩 여부. QuickSlotComponent::OnQuickSlotChanged 는
	 * 퀵슬롯에 아이템을 등록/해제할 때만 발생하고, 이미 등록된 아이템의 StackCount 만 줄어드는
	 * 경우(포션 사용 등)엔 발생하지 않는다. 그 갱신을 받으려면 인벤토리 변경 델리게이트를 따로 구독해야 한다. */
	bool bIsInventoryBound = false;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ExpBarDynamicMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CompassDynamicMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MinimapDynamicMaterial;

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
