// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/HB_HUDWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"


void UHB_HUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IMG_HP_Bar_Fill)
	{
		HPBarDynamicMaterial = IMG_HP_Bar_Fill->GetDynamicMaterial();
	}

	BindPlayerState();
}

void UHB_HUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 현재 표시 퍼센트와 목표 퍼센트가 다를 때만 보간 실행
	if (!FMath::IsNearlyEqual(CurrentPercent, TargetPercent, 0.0001f))
	{
		// 1. FInterpTo: 타격 시 크게 감소한 뒤 점점 부드럽게 감속하며 들어가는 보간 (권장)
		CurrentPercent = FMath::FInterpTo(CurrentPercent, TargetPercent, InDeltaTime, InterpSpeed);

		// ※ 만약 일정한 속도로 일정하게 줄어들게 하고 싶다면 FInterpConstantTo 사용:
		// CurrentPercent = FMath::FInterpConstantTo(CurrentPercent, TargetPercent, InDeltaTime, InterpSpeed);

		// 머티리얼 Parameter 갱신
		if (HPBarDynamicMaterial)
		{
			HPBarDynamicMaterial->SetScalarParameterValue(FName("Percent"), CurrentPercent);
		}
	}
}

void UHB_HUDWidget::BindPlayerState()
{
	AHB_PlayerState* PS = Cast<AHB_PlayerState>(GetOwningPlayerState());
	if (PS)
	{
		PS->OnHPChanged.RemoveAll(this);
		PS->OnHPChanged.AddDynamic(this, &UHB_HUDWidget::UpdateHPBar);

		if (UHB_PlayerAttributeSet* AttribSet = PS->GetPlayerAttributeSet())
		{
			UpdateHPBar(AttribSet->GetPlayer_HP(), AttribSet->GetPlayer_MaxHP());
		}
	}
}

void UHB_HUDWidget::UpdateHPBar(float CurrentHP, float MaxHP)
{
	if (MaxHP <= 0.0f) return;

	TargetPercent = FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f);

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