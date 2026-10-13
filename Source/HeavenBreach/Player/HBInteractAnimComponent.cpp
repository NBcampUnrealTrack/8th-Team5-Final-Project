#include "Player/HBInteractAnimComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "GameFramework/Character.h"
#include "Player/HBPlayerCharacter.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	// 루프 애니메이션을 사실상 무한히 돌린다. 끌 때 멈추므로 실제로 다 돌 일은 없다.
	constexpr int32 InteractLoopCount = 1000;
}

UHBInteractAnimComponent::UHBInteractAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// 기문 님이 만든 공통 상호작용 애니메이션. 정식 폴더에 있다.
	StartAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Characters/Warrior/Animations/UpperBody/Interaction/AN_HB_CommonInteract_Start.AN_HB_CommonInteract_Start")));
	LoopAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Characters/Warrior/Animations/UpperBody/Interaction/AN_HB_CommonInteract_Loop.AN_HB_CommonInteract_Loop")));
	EndAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Characters/Warrior/Animations/UpperBody/Interaction/AN_HB_CommonInteract_End.AN_HB_CommonInteract_End")));
}

void UHBInteractAnimComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 본인은 SetInteracting 에서 이미 반영했다. 서버 값이 늦게 돌아와 빠른 켜고-끄기를 되돌리지 않게 뺀다.
	DOREPLIFETIME_CONDITION(UHBInteractAnimComponent, bInteracting, COND_SkipOwner);
}

void UHBInteractAnimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bAppliedInteracting)
	{
		SetBoxInteractingTag(false);
		SetWeaponHidden(false);
		bAppliedInteracting = false;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShowWeaponTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UHBInteractAnimComponent::SetInteracting(bool bNewInteracting)
{
	if (bInteracting == bNewInteracting)
	{
		return;
	}

	// 본인 화면은 서버 왕복을 기다리지 않고 바로 반영한다.
	bInteracting = bNewInteracting;
	ApplyInteracting();

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		Server_SetInteracting(bNewInteracting);
	}
}

void UHBInteractAnimComponent::Server_SetInteracting_Implementation(bool bNewInteracting)
{
	bInteracting = bNewInteracting;
	ApplyInteracting();
}

void UHBInteractAnimComponent::OnRep_Interacting()
{
	ApplyInteracting();
}

void UHBInteractAnimComponent::ApplyInteracting()
{
	if (bAppliedInteracting == bInteracting)
	{
		return;
	}
	bAppliedInteracting = bInteracting;

	SetBoxInteractingTag(bInteracting);

	// 무기는 애니메이션과 별개로 숨긴다. 애님 인스턴스가 없어도(데디 서버 등) 상태는 맞춘다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShowWeaponTimerHandle);
	}
	if (bInteracting)
	{
		SetWeaponHidden(true);
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		if (!bInteracting)
		{
			SetWeaponHidden(false);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoopTimerHandle);
	}

	if (bInteracting)
	{
		UAnimSequenceBase* Start = StartAnim.LoadSynchronous();
		if (!Start)
		{
			PlayLoop();
			return;
		}

		PlayingMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Start, SlotName, BlendTime, BlendTime, 1.f, 1);

		// Start 가 빠지기 시작할 때 Loop 로 넘긴다. 블렌드가 겹쳐 끊김 없이 이어진다.
		const float Delay = FMath::Max(0.01f, Start->GetPlayLength() - BlendTime);
		GetWorld()->GetTimerManager().SetTimer(LoopTimerHandle, this, &UHBInteractAnimComponent::PlayLoop, Delay, false);
		return;
	}

	// 끌 때는 End 가 지금 동작을 덮으며 들어온다. End 가 없으면 그냥 멈춘다.
	if (UAnimSequenceBase* End = EndAnim.LoadSynchronous())
	{
		PlayingMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(End, SlotName, BlendTime, BlendTime, 1.f, 1);

		// 손이 무기 자세로 돌아오는 End 가 빠지기 시작할 때 보인다. 중간에 나타나면 빈손에서 튀어나온다.
		const float Delay = FMath::Max(0.01f, End->GetPlayLength() - BlendTime);
		GetWorld()->GetTimerManager().SetTimer(ShowWeaponTimerHandle, this, &UHBInteractAnimComponent::ShowWeapon, Delay, false);
		return;
	}

	if (PlayingMontage)
	{
		AnimInstance->Montage_Stop(BlendTime, PlayingMontage);
		PlayingMontage = nullptr;
	}
	SetWeaponHidden(false);
}

void UHBInteractAnimComponent::ShowWeapon()
{
	// 타이머 사이 다시 켰으면 ApplyInteracting 이 타이머를 지웠다. 그래도 한 번 더 확인한다.
	if (!bAppliedInteracting)
	{
		SetWeaponHidden(false);
	}
}

void UHBInteractAnimComponent::SetWeaponHidden(bool bHidden)
{
	if (AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(GetOwner()))
	{
		Player->SetHandEquipmentHidden(bHidden);
	}
}

void UHBInteractAnimComponent::PlayLoop()
{
	if (!bAppliedInteracting)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	UAnimSequenceBase* Loop = LoopAnim.LoadSynchronous();
	if (!AnimInstance || !Loop)
	{
		return;
	}

	PlayingMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Loop, SlotName, BlendTime, BlendTime, 1.f, InteractLoopCount);
}

void UHBInteractAnimComponent::SetBoxInteractingTag(bool bAdd)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (!ASC)
	{
		return;
	}

	// 루즈 태그는 복제되지 않는다. 각 머신이 bInteracting 을 보고 스스로 붙인다.
	// 본인 클라에도 있어야 평타·스킬의 로컬 예측 발동이 막힌다.
	if (bAdd)
	{
		ASC->AddLooseGameplayTag(StateTag::TAG_State_BoxInteracting);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(StateTag::TAG_State_BoxInteracting);
	}
}
