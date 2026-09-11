#include "UI/HUD/HB_HUDLayer.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EquipmentComponent.h"
#include "Inventory/QuickSlotComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/HBPlayerCharacter.h"
#include "Components/TextBlock.h"
#include "UI/HUD/HB_HitEffectWidget.h"
#include "Engine/TextureRenderTarget2D.h"

UHB_HUDLayer::UHB_HUDLayer()
{
	// HUD는 스택에 올라가는 즉시 활성화되고, 그 상태로 계속 유지된다
	bAutoActivate = true;

	// 활성 상태에서 게임 입력 모드를 선언 → PlayerController의 SetInputMode가 필요 없어진다.
	// 인벤토리 등이 위에 쌓이면 그쪽 Menu 설정이 이기고, 닫히면 여기로 자동 복귀한다
	InputMode = EHBWidgetInputMode::GameOnly;
}

void UHB_HUDLayer::ToggleInventory()
{
	if (IsInventoryOpen())
	{
		CloseInventory();
	}
	else
	{
		OpenInventory();
	}
}

void UHB_HUDLayer::OpenInventory()
{
	if (WBP_Inventory == nullptr || IsInventoryOpen())
	{
		return;
	}

	APlayerController* OwningController = GetOwningPlayer();
	AHBPlayerCharacter* PlayerCharacter = OwningController
		? Cast<AHBPlayerCharacter>(OwningController->GetPawn())
		: nullptr;
	if (OwningController == nullptr || !OwningController->IsLocalController() || PlayerCharacter == nullptr)
	{
		return;
	}

	// This widget persists inside the HUD, so reconnect it to the current pawn whenever it opens.
	WBP_Inventory->BindToInventory(PlayerCharacter->GetInventory());
	WBP_Inventory->BindToEquipment(PlayerCharacter->GetEquipment());
	WBP_Inventory->SetLinkedEquipment(PlayerCharacter->GetEquipment());
	WBP_Inventory->SetLinkedContainer(nullptr);
	WBP_Inventory->RefreshGrid();
	WBP_Inventory->SetVisibility(ESlateVisibility::Visible);

	FInputModeUIOnly UIOnlyInputMode;
	UIOnlyInputMode.SetWidgetToFocus(WBP_Inventory->TakeWidget());
	OwningController->SetInputMode(UIOnlyInputMode);
	OwningController->bShowMouseCursor = true;
	WBP_Inventory->SetKeyboardFocus();
}

void UHB_HUDLayer::CloseInventory()
{
	if (WBP_Inventory == nullptr || !IsInventoryOpen())
	{
		return;
	}

	WBP_Inventory->PrepareForHide();
	WBP_Inventory->SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* OwningController = GetOwningPlayer())
	{
		OwningController->SetInputMode(FInputModeGameOnly());
		OwningController->bShowMouseCursor = false;
	}
}

bool UHB_HUDLayer::IsInventoryOpen() const
{
	return WBP_Inventory != nullptr
		&& WBP_Inventory->GetVisibility() != ESlateVisibility::Collapsed;
}

void UHB_HUDLayer::NativeConstruct()
{
	Super::NativeConstruct();

	if (WBP_Inventory)
	{
		WBP_Inventory->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (IMG_HP_Bar_Fill)
	{
		HPBarDynamicMaterial = IMG_HP_Bar_Fill->GetDynamicMaterial();
	}

	if (IMG_MP_Bar_Fill)
	{
		MPBarDynamicMaterial = IMG_MP_Bar_Fill->GetDynamicMaterial();
	}

	if (IMG_EquipWeapon)
	{
		DefaultWeaponBrush = IMG_EquipWeapon->GetBrush();
	}
	if (IMG_EquipSubWeapon)
	{
		DefaultSubWeaponBrush = IMG_EquipSubWeapon->GetBrush();
	}
	if (IMG_Potion_1)
	{
		DefaultPotion1Brush = IMG_Potion_1->GetBrush();
	}
	if (IMG_Potion_2)
	{
		DefaultPotion2Brush = IMG_Potion_2->GetBrush();
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

	if (IMG_Minimap)
	{
		MinimapDynamicMaterial = IMG_Minimap->GetDynamicMaterial();
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

	if (!FMath::IsNearlyEqual(MPCurrentPercent, MPTargetPercent, 0.0001f))
	{
		MPCurrentPercent = FMath::FInterpTo(MPCurrentPercent, MPTargetPercent, InDeltaTime, InterpSpeed);

		if (MPBarDynamicMaterial)
		{
			MPBarDynamicMaterial->SetScalarParameterValue(FName("Percent"), MPCurrentPercent);
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

			PS->OnMPChanged.RemoveAll(this);
			PS->OnMPChanged.AddDynamic(this, &UHB_HUDLayer::UpdateMPBar);

			// 델리게이트는 "바뀔 때"만 오므로 접속 직후 현재 값을 한 번 직접 넣는다
			UpdateHPBar(AttribSet->GetPlayer_HP(), AttribSet->GetPlayer_MaxHP());
			UpdateMPBar(AttribSet->GetPlayer_Mana(), AttribSet->GetPlayer_MaxMana());
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

	// 3. QuickSlotComponent 바인딩 (아직 바인딩되지 않은 경우에만 시도)
	if (!bIsQuickSlotBound)
	{
		if (APlayerController* OwningController = GetOwningPlayer())
		{
			AHB_PlayerState* HBPlayerState = OwningController->GetPlayerState<AHB_PlayerState>();
			if (UQuickSlotComponent* QuickSlotComp = HBPlayerState ? HBPlayerState->GetQuickSlot() : nullptr)
			{
				CachedQuickSlotComp = QuickSlotComp;
				CachedQuickSlotComp->OnQuickSlotChanged.RemoveAll(this);
				CachedQuickSlotComp->OnQuickSlotChanged.AddUObject(this, &UHB_HUDLayer::UpdateQuickSlots);
				UpdateQuickSlots();
				bIsQuickSlotBound = true;
			}
		}
	}
	// PlayerState, EquipmentComponent, QuickSlotComponent 모두 준비되어야 재시도 타이머가 종료됩니다.
	return bPSBound && bIsEquipBound && bIsQuickSlotBound;
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
void UHB_HUDLayer::UpdateQuickSlots()
{
	if (!CachedQuickSlotComp)
	{
		return;
	}
	APlayerController* OwningController = GetOwningPlayer();
	AHB_PlayerState* HBPlayerState = OwningController ? OwningController->GetPlayerState<AHB_PlayerState>() : nullptr;
	UInventoryComponent* Inventory = HBPlayerState ? HBPlayerState->GetInventory() : nullptr;
	auto UpdateSlotImage = [this, Inventory](int32 SlotIndex, UImage* TargetImage, UTextBlock* CountText, const FSlateBrush& DefaultBrush)
		{
			if (!TargetImage)
			{
				return;
			}
			const FGuid InstanceId = CachedQuickSlotComp->GetSlotInstanceId(SlotIndex);
			if (Inventory && InstanceId.IsValid())
			{
				const int32 EntryIndex = Inventory->FindEntryIndexById(InstanceId);
				if (Inventory->GetEntries().IsValidIndex(EntryIndex))
				{
					const FItemInstance& SlotItem = Inventory->GetEntries()[EntryIndex].Item;
					FItemViewData ViewData;
					if (ResolveItemViewData(SlotItem.ItemRow, ViewData))
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
							if (CountText)
							{
								if (SlotItem.StackCount > 1)
								{
									CountText->SetText(FText::AsNumber(SlotItem.StackCount));
									CountText->SetVisibility(ESlateVisibility::HitTestInvisible);
								}
								else
								{
									CountText->SetVisibility(ESlateVisibility::Collapsed);
								}
							}
							return;
						}
					}
				}
			}
			// 슬롯이 비었거나 인벤토리에서 아이템을 찾지 못하면 기본 브러시로 복원
			TargetImage->SetBrush(DefaultBrush);
			TargetImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
			if (CountText)
			{
				CountText->SetVisibility(ESlateVisibility::Collapsed);
			}
		};
	// Potion_1 = 퀵슬롯 3번 = QuickSlotComponent 인덱스 2, Potion_2 = 퀵슬롯 4번 = 인덱스 3
	UpdateSlotImage(2, IMG_Potion_1, Txt_Potion_1_Num, DefaultPotion1Brush);
	UpdateSlotImage(3, IMG_Potion_2, Txt_Potion_2_Num, DefaultPotion2Brush);
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

void UHB_HUDLayer::UpdateMPBar(float CurrentMP, float MaxMP)
{
	if (MaxMP <= 0.0f)
	{
		return;
	}

	MPTargetPercent = FMath::Clamp(CurrentMP / MaxMP, 0.0f, 1.0f);

	if (!bIsMPInitialized)
	{
		MPCurrentPercent = MPTargetPercent;
		if (MPBarDynamicMaterial)
		{
			MPBarDynamicMaterial->SetScalarParameterValue(FName("Percent"), MPCurrentPercent);
		}
		bIsMPInitialized = true;
	}
}

void UHB_HUDLayer::SetMinimapTexture(UTextureRenderTarget2D* InMinimapRenderTarget)
{
	if (MinimapDynamicMaterial && InMinimapRenderTarget)
	{
		MinimapDynamicMaterial->SetTextureParameterValue(FName("MinimapTexture"), InMinimapRenderTarget);
	}
}

void UHB_HUDLayer::PlayHitEffect()
{
	if (WBP_PlayerBloodSplatter)
	{
		WBP_PlayerBloodSplatter->PlayHitEffect();
	}
}
