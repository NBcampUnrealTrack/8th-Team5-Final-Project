#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HB_HitEffectWidget.generated.h"

class UWidgetAnimation;

/**
 * 피격 시 화면을 덮는 연출용 위젯 (혈흔 스플래터 등).
 *
 * 화면 단위 UI가 아니라 순간 이펙트이므로 Activatable Widget Stack에 push하지 않는다.
 * 스택은 위젯을 올리면 아래 위젯을 Deactivate 시키기 때문에, 여기에 올리면
 * 맞을 때마다 HUD가 통째로 꺼졌다 켜진다.
 * → HUD 레이어(WBP_MainHUD) 안의 자식 위젯으로 배치하고, UHB_HUDLayer가 재생을 호출한다.
 *
 * 사용법: WBP_PlayerBloodSplatter를 이 클래스로 Reparent 한 뒤
 *        WBP_MainHUD 캔버스에 풀스크린 앵커로 배치한다.
 */
UCLASS(Abstract, Blueprintable)
class HEAVENBREACH_API UHB_HitEffectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 피격 연출 재생. 이미 재생 중이면 처음부터 다시 시작한다
	 * (연타로 맞을 때 알파가 어중간하게 남아 있는 상태를 방지)
	 */
	UFUNCTION(BlueprintCallable, Category = "HB|UI|HitEffect")
	void PlayHitEffect();

	/**
	 * 위젯 애니메이션 대신 BP에서 직접 연출하고 싶을 때 사용.
	 *
	 * 주의: Visibility 자동 관리(Collapsed <-> HitTestInvisible)는 PlayerHit 애니메이션의
	 * 재생/종료에 묶여 있다. PlayerHit 없이 이 이벤트만 쓰면 위젯이 Collapsed 인 채라
	 * 아무것도 보이지 않으므로, 그 경우 BP 가 Visibility 를 직접 켜고 꺼야 한다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "HB|UI|HitEffect", meta = (DisplayName = "On Hit Effect"))
	void BP_OnHitEffect();

protected:
	virtual void NativeOnInitialized() override;

	/** 재생이 끝나면 다시 Collapsed 로 되돌린다 */
	UFUNCTION()
	void HandleHitEffectFinished();

	/**
	 * WBP의 위젯 애니메이션과 이름으로 바인딩된다.
	 * 이름이 다르거나 없으면 nullptr이 들어오고, BP_OnHitEffect만 호출된다
	 */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> PlayerHit;
};
