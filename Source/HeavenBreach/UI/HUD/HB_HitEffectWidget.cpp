#include "UI/HUD/HB_HitEffectWidget.h"

#include "Animation/WidgetAnimation.h"

void UHB_HitEffectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 평상시에는 아예 숨겨 둔다.
	// Hidden 이 아니라 Collapsed 를 쓰는 이유는 레이아웃 계산에서도 빠지기 때문.
	// 이렇게 하면 WBP 의 Image 기본 알파가 0 이 아니어도 혈흔이 남지 않는다
	SetVisibility(ESlateVisibility::Collapsed);

	// 위젯 애니메이션은 Initialize 단계에서 이미 바인딩돼 있다.
	// 재생마다 바인딩하면 델리게이트가 누적되므로 여기서 한 번만 건다
	if (PlayerHit)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &UHB_HitEffectWidget::HandleHitEffectFinished);
		BindToAnimationFinished(PlayerHit, FinishedEvent);
	}
}

void UHB_HitEffectWidget::PlayHitEffect()
{
	if (PlayerHit)
	{
		// 연타 피격 시 이전 재생이 남아 있으면 알파가 중간값에서 시작한다.
		// 항상 0프레임부터 다시 재생.
		// StopAnimation 은 AnimationFinished 를 브로드캐스트하므로
		// 반드시 Visibility 를 켜기 전에 호출해야 한다 (안 그러면 바로 다시 Collapsed 된다)
		if (IsAnimationPlaying(PlayerHit))
		{
			StopAnimation(PlayerHit);
		}

		// 화면을 덮는 오버레이라 클릭을 먹으면 안 된다
		SetVisibility(ESlateVisibility::HitTestInvisible);

		PlayAnimation(PlayerHit);
	}

	BP_OnHitEffect();
}

void UHB_HitEffectWidget::HandleHitEffectFinished()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
