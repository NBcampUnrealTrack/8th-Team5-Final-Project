#include "Combat/Stagger/GA_PlayerStagger.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/State/HB_StateTag.h"

UGA_PlayerStagger::UGA_PlayerStagger()
{
	// 부르는 쪽이 이 태그로 찾는다. 적의 BP_GA_HitReact 와 같은 이름이라
	// ApplyStaggerTo 가 대상이 적인지 플레이어인지 구분하지 않아도 된다.
	SetAssetTags(FGameplayTagContainer(AbilityTag::TAG_Ability_HitReact));

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// ServerOnly 가 아니라 ServerInitiated 다. 발동은 서버만 시작하되
	// 소유 클라에서도 같이 돈다.
	//
	// 왜: GAS 의 몽타주 복제(RepAnimMontageInfo)는 COND_SkipOwner 라
	// 소유 클라에게는 안 간다. LocalPredicted 어빌리티를 전제한 설계로,
	// 소유 클라가 직접 재생하니 복제하면 두 번 물리기 때문이다.
	// ServerOnly 로 두면 소유 클라는 복제도 못 받고 로컬 실행도 없어서
	// 경직 몽타주가 자기 화면에만 안 나온다. 남들은 다 본다.
	//
	// 적은 소유 클라가 없어서 COND_SkipOwner 가 아무도 안 건너뛴다.
	// 그래서 적 경직(BP_GA_HitReact)은 ServerOnly 로도 잘 보인다.
	//
	// 덤으로 ActivationOwnedTags 도 소유 클라에 붙는다. 클라가 자기 경직을
	// 알게 되어 공격 입력이 서버까지 갔다 거절당하지 않고 그 자리에서 막힌다.
	//
	// 보안은 그대로다. 발동을 시작하는 것은 여전히 서버뿐이다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// 이 태그가 곧 경직 상태다. 어빌리티가 사는 동안만 붙는다.
	//
	// 공격(HBPlayerActiveGABase, GA_PlayerAttackRE)과 방어(GA_PlayerBlock)가
	// 이미 이 태그를 ActivationBlockedTags 에 갖고 있어서, 경직 중에는
	// 아무것도 발동하지 않는다. 여기서 따로 막을 필요가 없다.
	ActivationOwnedTags.AddTag(StateTag::TAG_State_Staggered);

	// 적의 경직 GA 와 같은 규칙이다.
	ActivationBlockedTags.AddTag(AbilityTag::TAG_State_Dead);
	ActivationBlockedTags.AddTag(StateTag::TAG_State_SuperArmor);

	// CancelAbilitiesWithTag 를 쓰지 않는다.
	//
	// 그 목록은 AssetTags 를 보는데, 방어 BP 의 AssetTags 는 Block.Shield 처럼
	// 장착별로 갈리고 액티브 스킬은 AssetTags 를 안 쓴다. 태그로 끊으려 하면
	// 새 방어 BP 를 만들 때마다 여기에 줄을 추가해야 한다.
	//
	// 대신 몽타주가 끊는다. 같은 슬롯에 경직 몽타주를 재생하면 진행 중이던
	// 몽타주가 중단되고, 그 어빌리티의 PlayMontageAndWait 이 OnInterrupted 로
	// 스스로 끝난다. 플레이어 어빌리티 넷이 전부 그 델리게이트를 물고 있다.
}

void UGA_PlayerStagger::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!StaggerMontage)
	{
		// 몽타주가 없으면 경직 시간을 정할 방법이 없다. 태그만 붙였다 떼면
		// 한 프레임짜리 경직이 되어 아무 의미가 없다.
		UE_LOG(LogTemp, Warning, TEXT("[Stagger] %s 경직 몽타주 미지정"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, StaggerMontage, PlayRate);
	if (!Task)
	{
		// AnimInstance 가 없으면 null 이 온다. 그냥 붙이면 크래시한다.
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 경직 동안 이동 감속. 서버가 붙이고 어트리뷰트 복제로 클라가 따라온다.
	//
	// 몽타주 태스크를 만든 뒤에 붙인다. 태스크 생성이 실패해 위에서 반환하면
	// 감속만 남는 상황이 생기기 때문이다.
	ApplySlow(ActorInfo);

	// 네 경로 모두 종료로 묶는다. 하나라도 빠지면 경직이 안 풀린다.
	Task->OnCompleted.AddDynamic(this, &UGA_PlayerStagger::OnMontageEnded);
	Task->OnBlendOut.AddDynamic(this, &UGA_PlayerStagger::OnMontageEnded);
	Task->OnCancelled.AddDynamic(this, &UGA_PlayerStagger::OnMontageEnded);
	Task->OnInterrupted.AddDynamic(this, &UGA_PlayerStagger::OnMontageEnded);
	Task->ReadyForActivation();
}

void UGA_PlayerStagger::OnMontageEnded()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_PlayerStagger::ApplySlow(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!StaggerSlowEffect)
	{
		return;   // 감속이 필요 없는 경직. 비워두면 여기서 끝난다
	}
	if (!ActorInfo)
	{
		return;
	}

	// 이동속도는 어트리뷰트라 클라가 자체 적용하면 곧 도착할 서버 값과 싸운다.
	// 공격·방어·액티브 스킬이 전부 같은 규칙이다.
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(StaggerSlowEffect, 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	SlowHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UGA_PlayerStagger::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소나 사망으로 끝나도 반드시 걷는다. 안 떼면 캐릭터가 영구히 느려진다.
	if (SlowHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->RemoveActiveGameplayEffect(SlowHandle);
		}
		SlowHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
