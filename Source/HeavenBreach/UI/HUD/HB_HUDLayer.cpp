#include "UI/HUD/HB_HUDLayer.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextBlock.h"
#include "UI/HUD/HB_HitEffectWidget.h"

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

	if (IMG_EquipWeapon)
	{
		DefaultWeaponBrush = IMG_EquipWeapon->GetBrush();
	}
	if (IMG_EquipSubWeapon)
	{
		DefaultSubWeaponBrush = IMG_EquipSubWeapon->GetBrush();
	}

	// 머티리얼이 지정돼 있지 않으면 nullptr 이 돌아온다. 그 경우 갱신만 건너뛴다
	if (IMG_Experience)
	{
		ExpBarDynamicMaterial = IMG_Experience->GetDynamicMaterial();
	}
	if (IMG_Compass)
	{
		CompassDynamicMaterial = IMG_Compass->GetDynamicMaterial();
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

	// 경험치도 같은 방식으로 부드럽게 채운다
	if (!FMath::IsNearlyEqual(ExpCurrentPercent, ExpTargetPercent, 0.0001f))
	{
		ExpCurrentPercent = FMath::FInterpTo(ExpCurrentPercent, ExpTargetPercent, InDeltaTime, InterpSpeed);

		if (ExpBarDynamicMaterial)
		{
			ExpBarDynamicMaterial->SetScalarParameterValue(FName("Percent"), ExpCurrentPercent);
		}
	}

	UpdateCompass();
}

void UHB_HUDLayer::UpdateCompass()
{
	if (CompassDynamicMaterial == nullptr)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}

	// 카메라가 보는 방향 기준. 0~360 으로 정규화한 뒤 0~1 로 바꿔 UV 오프셋에 쓴다.
	// 텍스처 X 타일링이 Wrap 이면 360도 경계가 자동으로 이어진다
	const float Yaw = FRotator::ClampAxis(PC->GetControlRotation().Yaw);
	const float Heading = (bInvertCompassDirection ? -Yaw : Yaw) / 360.0f;

	CompassDynamicMaterial->SetScalarParameterValue(CompassHeadingParam, Heading);
}

void UHB_HUDLayer::UpdateLevel(int32 NewLevel)
{
	if (Txt_Level)
	{
		Txt_Level->SetText(FText::AsNumber(NewLevel));
	}

	BP_OnLevelChanged(NewLevel);
}

void UHB_HUDLayer::UpdateExperience(int32 CurrentExperience, int32 ExperienceToNextLevel, float ExperiencePercent)
{
	ExpTargetPercent = FMath::Clamp(ExperiencePercent, 0.0f, 1.0f);

	// 최초 1회는 보간 없이 즉시 반영 (접속 직후 0 → 현재값 애니메이션 방지)
	if (!bIsExpInitialized)
	{
		ExpCurrentPercent = ExpTargetPercent;
		if (ExpBarDynamicMaterial)
		{
			ExpBarDynamicMaterial->SetScalarParameterValue(FName("Percent"), ExpCurrentPercent);
		}
		bIsExpInitialized = true;
	}

	BP_OnExperienceChanged(CurrentExperience, ExperienceToNextLevel, ExpTargetPercent);
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
	bool bPSBound = false;

	// 1. PlayerState 및 AttributeSet 바인딩
	AHB_PlayerState* PS = Cast<AHB_PlayerState>(GetOwningPlayerState());
	if (PS)
	{
		// 레벨/경험치는 AttributeSet 이 아니라 PlayerState 가 직접 복제한다
		PS->OnPlayerLevelChanged.RemoveAll(this);
		PS->OnPlayerLevelChanged.AddDynamic(this, &UHB_HUDLayer::UpdateLevel);

		PS->OnPlayerExperienceChanged.RemoveAll(this);
		PS->OnPlayerExperienceChanged.AddDynamic(this, &UHB_HUDLayer::UpdateExperience);

		UpdateLevel(PS->GetPlayerLevel());
		UpdateExperience(PS->GetCurrentExperience(), PS->GetExperienceToNextLevel(), PS->GetExperiencePercent());

		if (UHB_PlayerAttributeSet* AttribSet = PS->GetPlayerAttributeSet())
		{
			PS->OnHPChanged.RemoveAll(this);
			PS->OnHPChanged.AddDynamic(this, &UHB_HUDLayer::UpdateHPBar);

			UpdateHPBar(AttribSet->GetPlayer_HP(), AttribSet->GetPlayer_MaxHP());
			bPSBound = true;
		}
	}

	// 2. EquipmentComponent 바인딩 (아직 바인딩되지 않은 경우에만 시도)
	if (!bIsEquipBound)
	{
		if (APlayerController* OwningController = GetOwningPlayer())
		{
			AHB_PlayerState* HBPlayerState = OwningController->GetPlayerState<AHB_PlayerState>();
			if (UEquipmentComponent* EquipComp = HBPlayerState ? HBPlayerState->GetEquipment() : nullptr)
			{
				CachedEquipComp = EquipComp;

				CachedEquipComp->OnEquipmentChanged.RemoveDynamic(this, &UHB_HUDLayer::UpdateEquipmentSlots);
				CachedEquipComp->OnEquipmentChanged.AddDynamic(this, &UHB_HUDLayer::UpdateEquipmentSlots);

				UpdateEquipmentSlots();
				bIsEquipBound = true;
			}
		}
	}

	// PlayerState와 EquipmentComponent 둘 다 준비되어야 재시도 타이머가 종료됩니다.
	return bPSBound && bIsEquipBound;
}

void UHB_HUDLayer::UpdateEquipmentSlots()
{
	if (!CachedEquipComp)
	{
		return;
	}

	auto UpdateSlotImage = [this](EEquipSlotType SlotType, UImage* TargetImage, const FSlateBrush& DefaultBrush)
		{
			if (!TargetImage)
			{
				return;
			}

			FItemInstance ItemInstance;
			if (CachedEquipComp->GetItemInSlot(SlotType, ItemInstance))
			{
				FItemViewData ViewData;
				if (ResolveItemViewData(ItemInstance.ItemRow, ViewData))
				{
					UTexture2D* IconTexture = ViewData.Icon;

					if (!IconTexture && !ViewData.IconPath.IsEmpty())
					{
						IconTexture = LoadItemIcon(ViewData.IconPath);
					}

					if (IconTexture)
					{
						TargetImage->SetBrushFromTexture(IconTexture);
						TargetImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
						return;
					}
				}
			}

			// 빈 슬롯일 경우 WBP에서 설정했던 원래 기본 브러시로 복원하고 완전 불투명하게 유지
			TargetImage->SetBrush(DefaultBrush);
			TargetImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
		};

	UpdateSlotImage(EEquipSlotType::PrimaryWeapon, IMG_EquipWeapon, DefaultWeaponBrush);
	UpdateSlotImage(EEquipSlotType::SecondaryEquip, IMG_EquipSubWeapon, DefaultSubWeaponBrush);
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

void UHB_HUDLayer::PlayHitEffect()
{
	if (WBP_PlayerBloodSplatter)
	{
		WBP_PlayerBloodSplatter->PlayHitEffect();
	}
}
