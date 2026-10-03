// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Attack/GA_PlayerBowAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Combat/Attack/HBBowDraw.h"
#include "Combat/Attack/HBPlayerArrow.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "EquipmentComponent.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/HBWeaponPresentation.h"
#include "Camera/CameraComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Player/HBPlayerController.h"
#include "TimerManager.h"

UGA_PlayerBowAttack::UGA_PlayerBowAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationOwnedTags.AddTag(StateTag::TAG_State_Attacking);

	// 막는 태그는 검 평타(UGA_PlayerAttackRE)와 같다.
	ActivationBlockedTags.AddTag(AbilityTag::TAG_State_Dead);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Attacking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Blocking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_BoxInteracting);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_InventoryBusy);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Staggered);

	// AssetTags 는 BP 에서 Weapon.Bow 로 정한다.
}

UGA_PlayerBowAttack* UGA_PlayerBowAttack::FindActive(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		if (UGA_PlayerBowAttack* Bow = Cast<UGA_PlayerBowAttack>(Spec.GetPrimaryInstance()))
		{
			return Bow;
		}
	}
	return nullptr;
}

void UGA_PlayerBowAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());

	// 파손된 활은 당기지도 않는다.
	if (Player)
	{
		if (const UEquipmentComponent* Equipment = Player->GetEquipment())
		{
			if (Equipment->IsEquippedItemBroken(EEquipSlotType::PrimaryWeapon))
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
				return;
			}
		}
	}

	// 화살이 없으면 당기지 않는다. 클라가 먼저 막고 안내를 띄우고, 서버도 같은 검사를 한다.
	if (Player && !HasArrow(Player))
	{
		if (ActorInfo->IsLocallyControlled())
		{
			if (AHBPlayerController* PC = Player->GetController<AHBPlayerController>())
			{
				PC->ShowHudNotification(NoArrowMessage);
			}
		}
		else if (ActorInfo->IsNetAuthority())
		{
			// 클라가 복제 지연으로 화살이 있다고 예측했다가 서버가 거절한 경우 — 소유 클라에 안내한다.
			if (AHBPlayerController* PC = Player->GetController<AHBPlayerController>())
			{
				PC->ClientShowHudNotification(NoArrowMessage);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 없음 — 당기기 취소  Auth=%d"), ActorInfo->IsNetAuthority() ? 1 : 0);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 기본공격이라 마나를 쓰지 않는다.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	if (ActorInfo->IsNetAuthority() && ASC && AttackSlowEffect)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(AttackSlowEffect, 1.f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			SlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// 몽타주는 활마다 다를 수 있어 외형 표에서 먼저 찾는다.
	const FHBWeaponPresentation* Presentation = Player ? Player->GetWeaponPresentation() : nullptr;
	ActiveDrawMontage = ResolveMontage(Presentation ? Presentation->DrawMontage : nullptr, DrawMontage);
	ActiveReleaseMontage = ResolveMontage(Presentation ? Presentation->ReleaseMontage : nullptr, ReleaseMontage);
	ActiveCancelMontage = ResolveMontage(Presentation ? Presentation->CancelMontage : nullptr, CancelMontage);

	// 활 AnimBP 의 팔·시위 IK 와 장전 화살이 이 값을 본다. 다른 클라는 복제로 받는다.
	if (Player)
	{
		Player->SetBowDrawing(true);
	}

	if (ActiveDrawMontage)
	{
		// 당기기는 떼기를 기다리는 동안만 재생한다. 끝 콜백은 받지 않는다(떼기가 끝을 정한다).
		UAbilityTask_PlayMontageAndWait* DrawTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Draw"), ActiveDrawMontage);
		DrawTask->ReadyForActivation();
	}

	UE_LOG(LogTemp, Warning, TEXT("[Bow] 당기기 시작  Auth=%d  Local=%d  ArrowClass=%s"),
		ActorInfo->IsNetAuthority() ? 1 : 0, ActorInfo->IsLocallyControlled() ? 1 : 0, *GetNameSafe(ArrowClass));

	// 서버·클라가 각자 기다린다. 클라의 떼기는 InputReleased 이벤트로 서버에 복제된다.
	// bTestAlreadyReleased 는 false 여야 한다. 이 프로젝트는 스펙의 InputPressed 를 켜지 않아서,
	// true 면 시작하자마자 "이미 뗐다"로 보고 0초 당김(취소)이 된다.
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	ReleaseTask->OnRelease.AddDynamic(this, &UGA_PlayerBowAttack::OnReleased);
	ReleaseTask->ReadyForActivation();
}

void UGA_PlayerBowAttack::OnReleased(const float TimeHeld)
{
	FHBBowDrawParams Params;
	Params.MinDrawTime = MinDrawTime;
	Params.MaxDrawTime = MaxDrawTime;
	Params.MinArrowSpeed = MinArrowSpeed;
	Params.MaxArrowSpeed = MaxArrowSpeed;
	Params.MinDamageCoef = MinDamageCoef;
	Params.MaxDamageCoef = MaxDamageCoef;

	// TimeHeld 는 각자 잰 값이다. 발사는 서버 값으로만 한다.
	const FHBBowShot Shot = HBBow::ComputeShot(TimeHeld, Params);

	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	UE_LOG(LogTemp, Warning, TEXT("[Bow] 뗌  Auth=%d  TimeHeld=%.2f  Fire=%d  Speed=%.0f  Coef=%.2f"),
		Info && Info->IsNetAuthority() ? 1 : 0, TimeHeld, Shot.bFire ? 1 : 0, Shot.Speed, Shot.DamageCoef);

	// 놓는 순간 당김 자세(IK·장전 화살)를 푼다. 발사 몽타주가 복귀를 맡는다.
	if (AHBPlayerCharacter* Player = Info ? Cast<AHBPlayerCharacter>(Info->AvatarActor.Get()) : nullptr)
	{
		Player->SetBowDrawing(false);
	}

	if (!Shot.bFire)
	{
		PlayFinishAndEnd(ActiveCancelMontage);
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		AHBPlayerCharacter* Shooter = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());

		// 당기는 사이에 화살을 버렸으면 쏘지 않는다. 취소 동작으로 마무리.
		if (!ConsumeArrow(Shooter))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Bow] 발사 직전 화살 없음 — 취소"));
			PlayFinishAndEnd(ActiveCancelMontage);
			return;
		}

		SpawnArrow(Shooter, Shot.Speed, Shot.DamageCoef);
	}

	PlayFinishAndEnd(ActiveReleaseMontage);
}

void UGA_PlayerBowAttack::SpawnArrow(AHBPlayerCharacter* Player, const float Speed, const float DamageCoef)
{
	UWorld* World = GetWorld();
	if (!Player || !World || !ArrowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살을 만들 수 없음 (Player=%s ArrowClass=%s)"), *GetNameSafe(Player), *GetNameSafe(ArrowClass));
		return;
	}

	// 화면 중앙(카메라)에서 조준점으로. 서버도 몸 포즈를 매 틱 갱신하므로(AlwaysTickPoseAndRefreshBones)
	// 머리 뼈에 붙은 카메라 위치를 그대로 쓸 수 있다. 눈 높이(캡슐 기준)를 쓰면 화면과 어긋난다.
	const UCameraComponent* Camera = Player->GetFirstPersonCamera();
	const FVector CameraLocation = Camera ? Camera->GetComponentLocation() : Player->GetPawnViewLocation();
	const FVector AimPoint = Player->ComputeAimPoint(AimTraceDistance);
	const FVector AimDirection = (AimPoint - CameraLocation).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		return;
	}
	const FRotator EyeRotation = AimDirection.Rotation();

	const FTransform SpawnTransform(EyeRotation, CameraLocation + AimDirection * SpawnForwardOffset);

	AHBPlayerArrow* Arrow = World->SpawnActorDeferred<AHBPlayerArrow>(ArrowClass, SpawnTransform, Player, Player, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Arrow)
	{
		return;
	}
	Arrow->FinishSpawning(SpawnTransform);
	Arrow->Launch(Player, Speed, DamageCoef, ArrowItemRow);

	UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 스폰 %s  위치=%s  방향=%s"),
		*GetNameSafe(Arrow), *SpawnTransform.GetLocation().ToCompactString(), *EyeRotation.ToCompactString());
}

void UGA_PlayerBowAttack::PlayFinishAndEnd(UAnimMontage* Montage)
{
	if (Montage)
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Finish"), Montage);
		Task->OnCompleted.AddDynamic(this, &UGA_PlayerBowAttack::OnFinishMontageEnded);
		Task->OnBlendOut.AddDynamic(this, &UGA_PlayerBowAttack::OnFinishMontageEnded);
		Task->OnCancelled.AddDynamic(this, &UGA_PlayerBowAttack::OnFinishMontageCancelled);
		Task->OnInterrupted.AddDynamic(this, &UGA_PlayerBowAttack::OnFinishMontageCancelled);
		Task->ReadyForActivation();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RecoveryTimerHandle, FTimerDelegate::CreateUObject(this, &UGA_PlayerBowAttack::EndSelf, false), FMath::Max(ReleaseRecoveryTime, KINDA_SMALL_NUMBER), false);
		return;
	}

	EndSelf(false);
}

void UGA_PlayerBowAttack::OnFinishMontageEnded()
{
	EndSelf(false);
}

void UGA_PlayerBowAttack::OnFinishMontageCancelled()
{
	EndSelf(true);
}

void UGA_PlayerBowAttack::EndSelf(const bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bWasCancelled);
	}
}

void UGA_PlayerBowAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	RemoveAttackSlow(ActorInfo);

	// 끊겨서 끝난 경우(경직·사망 등)에도 당김 자세를 푼다.
	if (AHBPlayerCharacter* Player = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		Player->SetBowDrawing(false);
	}

	// 당기기 몽타주가 남아 있으면 끊는다(취소로 끝난 경우).
	if (ActiveDrawMontage && ActorInfo)
	{
		if (UAnimInstance* Anim = ActorInfo->GetAnimInstance())
		{
			Anim->Montage_Stop(0.2f, ActiveDrawMontage);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UGA_PlayerBowAttack::ResolveMontage(UAnimMontage* FromPresentation, UAnimMontage* Fallback) const
{
	return FromPresentation ? FromPresentation : Fallback;
}

void UGA_PlayerBowAttack::RemoveAttackSlow(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!SlowHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->RemoveActiveGameplayEffect(SlowHandle);
	}
	SlowHandle.Invalidate();
}

bool UGA_PlayerBowAttack::HasArrow(const AHBPlayerCharacter* Player) const
{
	if (ArrowItemRow.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bow] ArrowItemRow 가 비어 있어 화살을 쓰지 않음 (BP 에서 DT_Item/Arrow 지정)"));
		return true;
	}

	const UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	return Inventory && Inventory->GetTotalGold(ArrowItemRow.RowName) > 0;
}

bool UGA_PlayerBowAttack::ConsumeArrow(AHBPlayerCharacter* Player) const
{
	if (ArrowItemRow.IsNull())
	{
		return true;
	}

	// 이름은 Gold 지만 아무 행이나 개수만큼 뺀다(서버 전용).
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	return Inventory && Inventory->ConsumeGold(ArrowItemRow, 1);
}
