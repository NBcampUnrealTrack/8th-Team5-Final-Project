#include "UI/HUD/HB_HUDLayer.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EquipmentComponent.h"
#include "GameFramework/Pawn.h"

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
	bool bPSBound = false;

	// 1. PlayerState 및 AttributeSet 바인딩
	AHB_PlayerState* PS = Cast<AHB_PlayerState>(GetOwningPlayerState());
	if (PS)
	{
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
		if (APawn* Pawn = GetOwningPlayerPawn())
		{
			if (UEquipmentComponent* EquipComp = Pawn->FindComponentByClass<UEquipmentComponent>())
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
