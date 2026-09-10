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
#include "Combat/Ability/Warrior/GA_WarriorParry.h"
#include "Player/HBPlayerCharacter.h"

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

	// 경직 중에는 방어를 올릴 수 없다. 공격과 같은 규칙으로 맞췄다.
	// 작동방식: 피격 GE 나 노티파이가 State.Staggered 를 붙이면
	// CanActivateAbility 가 이 목록과 비교해 발동을 거부한다.
	// State.HitReact 는 넣지 않는다. 그 태그는 "행동 유지" 용도로 나뉘어 있다.
	// State.Grabbed 도 넣지 않는다. 잡힌 상태에서 막을 수 있어야 한다는 기획이다.
	// 쳐낼 조건: 경직을 방어로 끊는(패링류) 시스템이 생기면 이 줄을 뺀다.
	ActivationBlockedTags.AddTag(StateTag::TAG_State_Staggered);
}

void UGA_PlayerBlock::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// ���� ��Ÿ�� �ʼ�
	if (!AM_Block)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// �����Ƽ ��� ����
	if (!CommitAbility(Handle,ActorInfo,ActivationInfo))
	{
		EndAbility(Handle,ActorInfo,ActivationInfo,true,false);
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

	// ���� ��ȣ�� ��ٸ���. ���� ��ȣ�� �̺�Ʈ �±� TAG_Event_Block_Release�� ���;��Ѵ�.
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

	// �̵� ����. �ٸ� ������ ���������� GE �� �Ǵ�.
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

		// 패링 창. 슬로우와 같은 자리에 두어 "서버에서만 적용"이라는 규칙을 공유한다.
		ApplyParryWindow(ActorInfo);
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
	// ��ȣ�� �ι� �����°� ����
	if (bReleaseBlock)
	{
		return;
	}

	bReleaseBlock = true;

	//ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	//USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	//UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	//
	//// ���Ἵ������ �Ѿ�� ������ �ִϸ��̼� ���
	//if (Anim && AM_Block && !ReleaseSectionName.IsNone())
	//{
	//	Anim->Montage_JumpToSection(ReleaseSectionName, AM_Block);
	//	return;
	//}

	// GAS��ü���� �����ϴ� ��Ÿ�� ���
	// ���� �ּ�ó���� �ڵ�� �������� GAS�� ���ذ��� Ŭ�󿡼� �θ��� ������ RPC ���� ������.
	if (AM_Block && !ReleaseSectionName.IsNone())
	{
		MontageJumpToSection(ReleaseSectionName);
		return;
	}

	// ���� ���� ������ ��� ����
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

void UGA_PlayerBlock::ApplyParryWindow(const FGameplayAbilityActorInfo* ActorInfo)
{
	const AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!Player)
	{
		return;
	}

	// 패링을 안 배웠으면 창 자체를 열지 않는다.
	// 전에는 누구나 방어할 때마다 태그가 붙고 판정에서 걸러냈다. 쓸데없는 GE 였다.
	const UGA_WarriorParry* Parry = Player->GetParryPassive();
	if (!Parry)
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> WindowEffect = Parry->GetWindowEffect();
	if (!WindowEffect)
	{
		return;   // 패시브 애셋에 창 GE 를 안 넣었다
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(WindowEffect, 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(
		AbilityTag::TAG_SetByCaller_Duration, Parry->GetWindowDuration());
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
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
