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

	if (DrawMontage)
	{
		// 당기기는 떼기를 기다리는 동안만 재생한다. 끝 콜백은 받지 않는다(떼기가 끝을 정한다).
		UAbilityTask_PlayMontageAndWait* DrawTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("Draw"), DrawMontage);
		DrawTask->ReadyForActivation();
	}

	// 서버·클라가 각자 기다린다. 클라의 떼기는 InputReleased 이벤트로 서버에 복제된다.
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
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

	if (!Shot.bFire)
	{
		PlayFinishAndEnd(CancelMontage);
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		SpawnArrow(Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()), Shot.Speed, Shot.DamageCoef);
	}

	PlayFinishAndEnd(ReleaseMontage);
}

void UGA_PlayerBowAttack::SpawnArrow(AHBPlayerCharacter* Player, const float Speed, const float DamageCoef)
{
	UWorld* World = GetWorld();
	if (!Player || !World || !ArrowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살을 만들 수 없음 (Player=%s ArrowClass=%s)"), *GetNameSafe(Player), *GetNameSafe(ArrowClass));
		return;
	}

	// 서버의 시선 = 폰 눈 위치 + 컨트롤 회전. 데디 서버에선 카메라 컴포넌트 위치를 믿지 않는다.
	FVector EyeLocation;
	FRotator EyeRotation;
	Player->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	const FTransform SpawnTransform(EyeRotation, EyeLocation + EyeRotation.Vector() * SpawnForwardOffset);

	AHBPlayerArrow* Arrow = World->SpawnActorDeferred<AHBPlayerArrow>(ArrowClass, SpawnTransform, Player, Player, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Arrow)
	{
		return;
	}
	Arrow->FinishSpawning(SpawnTransform);
	Arrow->Launch(Player, Speed, DamageCoef);
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

	// 당기기 몽타주가 남아 있으면 끊는다(취소로 끝난 경우).
	if (DrawMontage && ActorInfo)
	{
		if (UAnimInstance* Anim = ActorInfo->GetAnimInstance())
		{
			Anim->Montage_Stop(0.2f, DrawMontage);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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
