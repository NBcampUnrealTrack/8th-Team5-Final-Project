// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Attack/HBBowShot.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/Attack/GA_PlayerBowAttack.h"
#include "Engine/World.h"
#include "Inventory/InventoryComponent.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/HBPlayerController.h"
#include "Player/HBWeaponPresentation.h"

namespace HBBowShot
{
	bool HasArrow(const AHBPlayerCharacter* Player, const FDataTableRowHandle& ArrowRow)
	{
		if (ArrowRow.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Bow] ArrowItemRow 가 비어 있어 화살을 쓰지 않음 (BP 에서 DT_Item/Arrow 지정)"));
			return true;
		}

		const UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
		return Inventory && Inventory->GetTotalGold(ArrowRow.RowName) > 0;
	}

	bool ConsumeArrow(AHBPlayerCharacter* Player, const FDataTableRowHandle& ArrowRow)
	{
		if (ArrowRow.IsNull())
		{
			return true;
		}

		// 이름은 Gold 지만 아무 행이나 개수만큼 뺀다(서버 전용).
		UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
		return Inventory && Inventory->ConsumeGold(ArrowRow, 1);
	}

	AHBPlayerArrow* SpawnArrow(AHBPlayerCharacter* Player, const TSubclassOf<AHBPlayerArrow> ArrowClass, const float Speed, const float DamageCoef,
		const FDataTableRowHandle& ArrowRow, const float AimDistance, const float ForwardOffset, const FHBArrowLaunchOptions& Options)
	{
		UWorld* World = Player ? Player->GetWorld() : nullptr;
		if (!Player || !World || !ArrowClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살을 만들 수 없음 (Player=%s ArrowClass=%s)"), *GetNameSafe(Player), *GetNameSafe(ArrowClass));
			return nullptr;
		}

		// 화면 중앙(카메라)에서 조준점으로. 서버도 몸 포즈를 매 틱 갱신하므로(AlwaysTickPoseAndRefreshBones)
		// 머리 뼈에 붙은 카메라 위치를 그대로 쓸 수 있다. 눈 높이(캡슐 기준)를 쓰면 화면과 어긋난다.
		const UCameraComponent* Camera = Player->GetFirstPersonCamera();
		const FVector CameraLocation = Camera ? Camera->GetComponentLocation() : Player->GetPawnViewLocation();
		const FVector AimPoint = Player->ComputeAimPoint(AimDistance);
		const FVector AimDirection = (AimPoint - CameraLocation).GetSafeNormal();
		if (AimDirection.IsNearlyZero())
		{
			return nullptr;
		}
		const FRotator EyeRotation = AimDirection.Rotation();

		const FTransform SpawnTransform(EyeRotation, CameraLocation + AimDirection * ForwardOffset);

		AHBPlayerArrow* Arrow = World->SpawnActorDeferred<AHBPlayerArrow>(ArrowClass, SpawnTransform, Player, Player, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Arrow)
		{
			return nullptr;
		}
		Arrow->FinishSpawning(SpawnTransform);
		Arrow->Launch(Player, Speed, DamageCoef, ArrowRow, Options);

		UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 스폰 %s  위치=%s  방향=%s"),
			*GetNameSafe(Arrow), *SpawnTransform.GetLocation().ToCompactString(), *EyeRotation.ToCompactString());
		return Arrow;
	}

	void NotifyNoArrow(const AHBPlayerCharacter* Player, const FText& Message)
	{
		AHBPlayerController* PC = Player ? Player->GetController<AHBPlayerController>() : nullptr;
		if (!PC)
		{
			return;
		}

		if (Player->IsLocallyControlled())
		{
			PC->ShowHudNotification(Message);
		}
		else if (Player->HasAuthority())
		{
			// 클라가 복제 지연으로 화살이 있다고 예측했다가 서버가 거절한 경우 — 소유 클라에 안내한다.
			PC->ClientShowHudNotification(Message);
		}
	}

	void ResolveArrowDefaults(const AHBPlayerCharacter* Player, TSubclassOf<AHBPlayerArrow>& InOutClass, FDataTableRowHandle& InOutRow)
	{
		bool bNeedRow = InOutRow.IsNull();
		bool bNeedClass = !InOutClass || InOutClass.Get() == AHBPlayerArrow::StaticClass();
		if (!bNeedRow && !bNeedClass)
		{
			return;
		}

		// 활 평타 GA 에서 빌린다. Spec.Ability 는 BP CDO 라 BP 에서 지정한 값이 들어 있다(ServerGiveArrows 와 같은 방식).
		const UAbilitySystemComponent* ASC = Player ? Player->GetAbilitySystemComponent() : nullptr;
		if (ASC)
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				const UGA_PlayerBowAttack* Bow = Cast<UGA_PlayerBowAttack>(Spec.Ability);
				if (!Bow)
				{
					continue;
				}
				if (bNeedRow && !Bow->GetArrowItemRow().IsNull())
				{
					InOutRow = Bow->GetArrowItemRow();
					bNeedRow = false;
				}
				if (bNeedClass && Bow->GetArrowClass())
				{
					InOutClass = Bow->GetArrowClass();
					bNeedClass = false;
				}
				if (!bNeedRow && !bNeedClass)
				{
					break;
				}
			}
		}

		if (InOutRow.IsNull())
		{
			// 스킬에도 활 평타에도 행이 없다. 무한 화살로 동작한다. 매 발동마다 찍으면 시끄러워서 한 번만.
			static bool bLoggedMissingRow = false;
			if (!bLoggedMissingRow)
			{
				bLoggedMissingRow = true;
				UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 화살 행이 스킬에도 활 평타 GA 에도 없음 — 화살을 쓰지 않음(무한). BP 에서 ArrowItemRow 지정 필요"));
			}
		}
	}

	FName GetBowVariant(const AHBPlayerCharacter* Player)
	{
		const FHBWeaponPresentation* Presentation = Player ? Player->GetWeaponPresentation() : nullptr;
		return Presentation ? Presentation->BowVariant : NAME_None;
	}
}
