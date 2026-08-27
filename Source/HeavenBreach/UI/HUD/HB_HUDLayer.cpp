#include "UI/HUD/HB_HUDLayer.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "TimerManager.h"
#include "Engine/World.h"

UHB_HUDLayer::UHB_HUDLayer()
{
	// HUD는 스택에 올라가는 즉시 활성화되고, 그 상태로 계속 유지된다
	bAutoActivate = true;

	// 활성 상태에서 게임 입력 모드를 선언 → PlayerController의 SetInputMode가 필요 없어진다.
	// 인벤토리 등이 위에 쌓이면 그쪽 Menu 설정이 이기고, 닫히면 여기로 자동 복귀한다
	InputMode = EHBWidgetInputMode::GameOnly;
}

void UHB_HUDLayer::NativeConstruct()
{
	Super::NativeConstruct();

	if (IMG_HP_Bar_Fill)
	{
		HPBarDynamicMaterial = IMG_HP_Bar_Fill->GetDynamicMaterial();
	}

	BindPlayerState();
}

void UHB_HUDLayer::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}

	Super::NativeDestruct();
}

void UHB_HUDLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 현재 표시 퍼센트가 목표와 다를 때만 보간
	if (!FMath::IsNearlyEqual(CurrentPercent, TargetPercent, 0.0001f))
	{
		CurrentPercent = FMath::FInterpTo(CurrentPercent, TargetPercent, InDeltaTime, InterpSpeed);

		if (HPBarDynamicMaterial)
		{
			HPBarDynamicMaterial->SetScalarParameterValue(FName("Percent"), CurrentPercent);
		}
	}
}

void UHB_HUDLayer::BindPlayerState()
{
	if (TryBindPlayerState())
	{
		return;
	}

	// 클라이언트에서는 PlayerState 복제가 위젯 생성보다 늦을 수 있다.
	// 준비될 때까지 재시도한다 (기존 BP_HBPlayerCharacter의 IsValid + Delay 루프를 대체)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BindRetryTimerHandle, this, &UHB_HUDLayer::HandleBindRetry, BindRetryInterval, true);
	}
}

void UHB_HUDLayer::HandleBindRetry()
{
	if (!TryBindPlayerState())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}
}

bool UHB_HUDLayer::TryBindPlayerState()
{
	AHB_PlayerState* PS = Cast<AHB_PlayerState>(GetOwningPlayerState());
	if (PS == nullptr)
	{
		return false;
	}

	UHB_PlayerAttributeSet* AttribSet = PS->GetPlayerAttributeSet();
	if (AttribSet == nullptr)
	{
		return false;
	}

	PS->OnHPChanged.RemoveAll(this);
	PS->OnHPChanged.AddDynamic(this, &UHB_HUDLayer::UpdateHPBar);

	UpdateHPBar(AttribSet->GetPlayer_HP(), AttribSet->GetPlayer_MaxHP());
	return true;
}

void UHB_HUDLayer::UpdateHPBar(float CurrentHP, float MaxHP)
{
	if (MaxHP <= 0.0f)
	{
		return;
	}

	TargetPercent = FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f);

	// 최초 1회는 보간 없이 즉시 반영 (스폰 직후 0 → 100% 애니메이션 방지)
	if (!bIsInitialized)
	{
		CurrentPercent = TargetPercent;
		if (HPBarDynamicMaterial)
		{
			HPBarDynamicMaterial->SetScalarParameterValue(FName("Percent"), CurrentPercent);
		}
		bIsInitialized = true;
	}
}
