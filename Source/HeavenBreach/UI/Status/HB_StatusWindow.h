// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/HB_ActivatableWidget.h"
#include "HB_StatusWindow.generated.h"

class AHBPlayerController;
class AHBPlayerCharacter;
class UPanelWidget;
class UTextBlock;
class UHB_SkillIconWidget;
class UHB_StatusAugmentEntry;

/**
 * 인게임 스탯창 (K).
 *
 * 보여주는 것: 능력치, 로비에서 고른 Q/E/패시브 스킬 아이콘, 이번 판에 고른 증강.
 * 읽기 전용이다. 값을 바꾸는 입력은 받지 않는다.
 *
 * 열고 닫는 흐름은 AHBPlayerController::ToggleSkillAugment 가 정한다.
 * 고를 증강이 남아 있으면 증강 화면이 먼저 뜨고, 고른 뒤에 이 창이 뜬다.
 *
 * 값 갱신: 어트리뷰트 델리게이트를 일일이 묶지 않고 열려 있는 동안만 틱에서
 * 0.2초마다 다시 읽는다. 창은 가끔 잠깐 열어 보는 화면이라 이쪽이 단순하고,
 * 능력치가 늘어나도 Refresh 한 곳만 고치면 된다.
 */
UCLASS()
class HEAVENBREACH_API UHB_StatusWindow : public UHB_ActivatableWidget
{
	GENERATED_BODY()

public:
	UHB_StatusWindow();

	static UHB_StatusWindow* OpenOn(
		AHBPlayerController* OwningPC,
		TSubclassOf<UHB_StatusWindow> ScreenClass);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ---- 능력치. 표시할 칸만 디자이너에 두면 된다 (없으면 건너뛴다) ----

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HP;

	/** 스태미나(Player_Stamina). 마나는 HUD 의 MP 바에 있다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Stamina;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_AttackPower;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Defense;

	// ---- 스킬 ----

	/** Q/E 자리. 액티브 디자인의 WBP_SkillIcon_Active 를 배치한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHB_SkillIconWidget> SkillIcon_Q;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHB_SkillIconWidget> SkillIcon_E;

	/**
	 * 로비에서 고른 시작 패시브 한 칸. WBP_SkillIcon_Passive 를 배치한다.
	 * 레벨업으로 얻은 패시브는 여기가 아니라 증강 목록에 나온다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHB_SkillIconWidget> SkillIcon_Passive;

	// ---- 증강 ----

	/** 고른 증강 줄이 쌓일 곳 (VerticalBox / ScrollBox 등). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> AugmentContainer;

	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	TSubclassOf<UHB_StatusAugmentEntry> AugmentEntryClass;

	/** 증강 줄 사이 위아래 간격(상하 합). */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	float AugmentEntrySpacing = 8.0f;

	/** 능력치를 다시 읽는 주기(초). */
	UPROPERTY(EditDefaultsOnly, Category = "UI Setup")
	float RefreshInterval = 0.2f;

private:
	void RefreshAll();
	void RefreshStats();
	void RefreshSkills();
	void RefreshAugments();

	AHBPlayerCharacter* GetHBCharacter() const;

	float RefreshElapsed = 0.f;

	// 증강/패시브 목록이 바뀌었는지 보는 값. 같으면 위젯을 다시 만들지 않는다.
	int32 LastAugmentSignature = -1;
};
