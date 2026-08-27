#include "GA_PlayerAttack.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "GameFramework/Character.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/HBPlayerController.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

UGA_PlayerAttack::UGA_PlayerAttack()
{
	SetAssetTags(FGameplayTagContainer(StateTag::TAG_State_Attacking));
	
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	ActivationOwnedTags.AddTag(StateTag::TAG_State_Attacking);
	
	ActivationBlockedTags.AddTag(AbilityTag::TAG_State_Dead);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Attacking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Blocking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_BoxInteracting);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_InventoryBusy);
}

void UGA_PlayerAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!AM_Attack)
	{
		EndAbility(Handle,ActorInfo,ActivationInfo,true,false);
		return;
	}
	if (!CommitAbility(Handle,ActorInfo,ActivationInfo))
	{
		EndAbility(Handle,ActorInfo,ActivationInfo,true,false);
		return;
	}
	
	AlreadyHitActors.Reset();
	
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const float AttackRate = ASC
		? FMath::Max(0.1f, ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_AttackSpeedAttribute()))
		: 1.f;
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, AM_Attack, AttackRate);
	MontageTask->OnCompleted.AddDynamic(this,&UGA_PlayerAttack::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this,&UGA_PlayerAttack::OnMontageCompleted);
	MontageTask->OnCancelled.AddDynamic(this,&UGA_PlayerAttack::OnMontageCancelled);
	MontageTask->OnInterrupted.AddDynamic(this,&UGA_PlayerAttack::OnMontageCancelled);
	MontageTask->ReadyForActivation();
	
	UAbilityTask_WaitGameplayEvent* HitCheckEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,EventTag::TAG_Event_MeleeHitCheck,nullptr,false,false);
	HitCheckEvent->EventReceived.AddDynamic(this,&UGA_PlayerAttack::OnHitCheckEvent);
	HitCheckEvent->ReadyForActivation();
}

void UGA_PlayerAttack::OnMontageCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,false);
}

void UGA_PlayerAttack::OnMontageCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,false);
}

void UGA_PlayerAttack::OnHitCheckEvent(FGameplayEventData Payload)
{
	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		if (ActorInfo->IsNetAuthority())
		{
			PerformHitTrace(ActorInfo);
		}
	}
}

void UGA_PlayerAttack::PerformHitTrace(const FGameplayAbilityActorInfo* ActorInfo)
{
	ACharacter* AttackerCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!AttackerCharacter)
	{
		return;
	}
	
	AHBPlayerCharacter* AttackPlayer = Cast<AHBPlayerCharacter>(AttackerCharacter);
	const float AttackRange = AttackPlayer ?  AttackPlayer->GetAttackRange() : 150.f;
	
	const FVector Start = AttackerCharacter->GetPawnViewLocation();
	const FVector End = Start + AttackerCharacter->GetBaseAimRotation().Vector() * AttackRange;
	
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AttackerCharacter);
	
	TArray<FHitResult> Hits;
	AttackerCharacter->GetWorld()->SweepMultiByObjectType(
		Hits,Start,End,FQuat::Identity,ObjectParams,FCollisionShape::MakeSphere(TracerRadius),QueryParams);
	
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || AlreadyHitActors.Contains(HitActor))
		{
			continue;
		}
		AlreadyHitActors.Add(HitActor);
		
		if (HitActor->Implements<UDamageableInterface>() && IDamageableInterface::Execute_CanBeDamaged(HitActor,AttackerCharacter))
		{
			ResolveCreatureHit(HitActor,Hit,AttackerCharacter);
		}
		else
		{
			ResolveWallHit(Hit);
		}
	}
}

void UGA_PlayerAttack::ResolveWallHit(const FHitResult& Hit)
{
	if (APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo()))
	{
		if (AHBPlayerController* PC = Cast<AHBPlayerController>(AvatarPawn->GetController()))
		{
			PC->ClientPlayCameraShake(ObjectHitCameraShake);
		}
	}
}

void UGA_PlayerAttack::ResolveCreatureHit(AActor* Target, const FHitResult& Hit, AActor* AttackerActor)
{
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	
	AHBPlayerCharacter* AttackerPlayer = Cast<AHBPlayerCharacter>(AttackerActor);
	float Damage = AttackerPlayer ? AttackerPlayer->GetAttackPower() : 10.f;
	if (AttackerPlayer)
	{
		Damage = AttackerPlayer->ModifyOutgoingDamage(Target, Damage);
	}
	
	if (AHBPlayerCharacter* TargetPlayer = Cast<AHBPlayerCharacter>(Target))
	{
		if (TargetASC && TargetASC->HasMatchingGameplayTag(StateTag::TAG_State_Blocking))
		{
			const FVector ToAttacker = (AttackerActor->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal();
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Target->GetActorForwardVector(), ToAttacker)));
			
			if (AngleDeg <= TargetPlayer->GetBlockAngle() * 0.5f)
			{
				Damage *= (1.f - TargetPlayer->GetBlockDamageReduction());
			}
		}
	}
	
	IDamageableInterface::Execute_ApplyMeleeHit(Target,AttackerActor,Damage,Hit);

	if (AttackerPlayer)
	{
		const bool bTargetKilled = TargetASC
			&& TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
		AttackerPlayer->NotifyDamageDealt(Target, Damage, bTargetKilled);
	}
	
	if (APawn* AttackerPawn = Cast<APawn>(AttackerActor))
	{
		if (AHBPlayerController* PC = Cast<AHBPlayerController>(AttackerPawn->GetController()))
		{
			PC->ClientPlayCameraShake(CreatureHitCameraShake);
		}
	}
}
