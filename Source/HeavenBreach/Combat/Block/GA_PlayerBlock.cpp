#include "GA_PlayerBlock.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Tag/Event/HB_EventTag.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Player/HBPlayerCharacter.h"
#include "EquipmentComponent.h"

UGA_PlayerBlock::UGA_PlayerBlock()
{
	SetAssetTags(FGameplayTagContainer(StateTag::TAG_State_Blocking));
	
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	ActivationOwnedTags.AddTag(StateTag::TAG_State_Blocking);
	
	ActivationBlockedTags.AddTag(AbilityTag::TAG_State_Dead);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Attacking);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_BoxInteracting);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_InventoryBusy);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Blocking);
}

void UGA_PlayerBlock::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 블록 몽타주 필수
	if (!AM_Block)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 장착된 방패가 파손 상태면 막기 자체를 시작하지 않는다(마나도 소모 안 함).
	// 방패가 없는 맨손 막기는 이 체크를 건너뛰고 계속 가능해야 한다.
	if (const AHBPlayerCharacter* Blocker = ActorInfo ? Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
	{
		if (const UEquipmentComponent* Equipment = Blocker->GetEquipment())
		{
			if (Equipment->IsSlotOccupied(EEquipSlotType::SecondaryEquip)
				&& Equipment->IsEquippedItemBroken(EEquipSlotType::SecondaryEquip))
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
				return;
			}
		}
	}

	// 어빌리티 사용 판정
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}


	bReleaseBlock = false;

	//if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	//{
	//	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	//	{
	//		CachedMaxWalkSpeed = Movement->MaxWalkSpeed;
	//		Movement->MaxWalkSpeed = CachedMaxWalkSpeed * BlockMoveSpeedMultiplier;
	//	}
	//}

	// 해제 신호를 기다린다. 해제 신호는 이벤트 태그 TAG_Event_Block_Release가 들어와야한다.
	UAbilityTask_WaitGameplayEvent* WaitRelease = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag::TAG_Event_Block_Release, nullptr, false, false);
	WaitRelease->EventReceived.AddDynamic(this, &UGA_PlayerBlock::OnBlockReleaseReceive);
	WaitRelease->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, AM_Block, 1.f);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UGA_PlayerBlock::OnBlockMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UGA_PlayerBlock::OnBlockMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UGA_PlayerBlock::OnBlockMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UGA_PlayerBlock::OnBlockMontageCancelled);
	MontageTask->ReadyForActivation();

	// 이동 감속. 다른 버프와 곱해지도록 GE 로 건다.
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		RemoveBlockSlow(ActorInfo);
		UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		if (ASC && BlockSlowEffect)
		{
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(BlockSlowEffect, 1.f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				SlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}
	//if (AM_Block)
	//{
	//	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
	//		this, NAME_None, AM_Block, 1.f, NAME_None, true);
	//
	//	MontageTask->OnCompleted.AddDynamic(this, &UGA_PlayerBlock::OnBlockMontageCompleted);
	//	MontageTask->OnBlendOut.AddDynamic(this, &UGA_PlayerBlock::OnBlockMontageCompleted);
	//	MontageTask->OnCancelled.AddDynamic(this, &UGA_PlayerBlock::OnBlockMontageCancelled);
	//	MontageTask->OnInterrupted.AddDynamic(this, &UGA_PlayerBlock::OnBlockMontageCancelled);
	//	MontageTask->ReadyForActivation();
	//}
	UE_LOG(LogTemp, Warning, TEXT("[Block] Activate | Server=%d"),
		(ActorInfo && ActorInfo->IsNetAuthority()) ? 1 : 0);
}

void UGA_PlayerBlock::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	//ApplyCooldown(Handle,ActorInfo,ActivationInfo);
	
	//if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	//{
	//	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	//	{
	//		if (CachedMaxWalkSpeed > 0.f)
	//		{
	//			Movement->MaxWalkSpeed = CachedMaxWalkSpeed;
	//		}
	//	}
	//}

	RemoveBlockSlow(ActorInfo);

	UE_LOG(LogTemp, Warning, TEXT("[Block] End | Server=%d Cancelled=%d"),
		(ActorInfo && ActorInfo->IsNetAuthority()) ? 1 : 0, bWasCancelled ? 1 : 0);
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

//void UGA_PlayerBlock::CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
//	const FGameplayAbilityActivationInfo ActivationInfo)
//{
//	ApplyCost(Handle,ActorInfo,ActivationInfo);
//}

void UGA_PlayerBlock::OnBlockReleaseReceive(FGameplayEventData Payload)
{
	// 신호가 두번 들어오는것 방지
	if (bReleaseBlock)
	{
		return;
	}

	bReleaseBlock = true;

	//ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	//USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	//UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	//
	//// 종료섹션으로 넘어가서 마지막 애니메이션 출력
	//if (Anim && AM_Block && !ReleaseSectionName.IsNone())
	//{
	//	Anim->Montage_JumpToSection(ReleaseSectionName, AM_Block);
	//	return;
	//}

	// GAS자체에서 제공하는 몽타주 기능
	// 위의 주석처리된 코드와 차이점은 GAS를 통해가서 클라에서 부르면 서버로 RPC 까지 보낸다.
	if (AM_Block && !ReleaseSectionName.IsNone())
	{
		MontageJumpToSection(ReleaseSectionName);
		return;
	}

	// 종료 섹션 없으면 즉시 종료
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_PlayerBlock::OnBlockMontageCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,false);
}

void UGA_PlayerBlock::OnBlockMontageCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
}

void UGA_PlayerBlock::RemoveBlockSlow(const FGameplayAbilityActorInfo* ActorInfo)
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
