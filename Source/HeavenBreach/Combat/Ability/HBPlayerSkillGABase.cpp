#include "Combat/Ability/HBPlayerSkillGABase.h"

#include "AbilitySystemComponent.h"
#include "Combat/Ability/HBAugmentLibrary.h"
#include "Player/HBPlayerCharacter.h"

UHBPlayerSkillGABase::UHBPlayerSkillGABase()
{
	// 액티브와 패시브 둘 다 상태를 들고 있어야 한다.
	// 액티브는 버프 핸들과 몽타주 진행을, 패시브는 상시 GE 핸들을 기억한다.
	// NonInstanced 로 두면 그 상태가 CDO 에 얹혀 모든 플레이어가 공유한다.
	//
	// NetExecutionPolicy 는 여기서 정하지 않는다.
	// 액티브는 예측(LocalPredicted), 패시브는 서버 전용(ServerOnly)으로 갈린다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UHBPlayerSkillGABase::FindAugment(FGameplayTag AugmentTag, FHBSkillAugment& Out) const
{
	if (!AugmentTag.IsValid())
	{
		return false;
	}

	// 배열이 스킬당 서너 개라 선형 탐색으로 충분하다.
	// 쳐낼 조건: 한 스킬의 증강이 수십 개가 되면 TMap 으로 바꾼다.
	for (const FHBSkillAugment& Augment : Augments)
	{
		if (Augment.AugmentTag == AugmentTag)
		{
			Out = Augment;
			return true;
		}
	}
	return false;
}

bool UHBPlayerSkillGABase::HasAugmentFrom(const FGameplayAbilityActorInfo* ActorInfo, FGameplayTag AugmentTag) const
{
	const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
	if (!Player)
	{
		return false;
	}
	// 보유 판정의 주인은 PlayerState 다. 캐릭터가 그리로 넘긴다.
	return Player->HasAugment(AugmentTag);
}

bool UHBPlayerSkillGABase::HasAugment(FGameplayTag AugmentTag) const
{
	return HasAugmentFrom(GetCurrentActorInfo(), AugmentTag);
}

float UHBPlayerSkillGABase::GetAugmentParamFrom(const FGameplayAbilityActorInfo* ActorInfo, FGameplayTag ParamTag, float Default) const
{
	if (!ParamTag.IsValid())
	{
		return Default;
	}

	// 수치는 이 스킬의 애셋에 적혀 있다. 캐릭터는 보유 여부만 안다.
	// 캐릭터에 물으면 어느 스킬의 증강인지 몰라 값을 정할 수 없다.
	for (const FHBSkillAugment& Augment : Augments)
	{
		if (!HasAugmentFrom(ActorInfo, Augment.AugmentTag))
		{
			continue;   // 아직 안 골랐다
		}
		const AHBPlayerCharacter* Player = GetPlayerFrom(ActorInfo);
		const int32 Tier = Player ? FMath::Max(1, Player->GetAugmentTier(Augment.AugmentTag)) : 1;
		float Value = 0.f;
		if (HBAugment::ResolveParam(Augment, ParamTag, Tier, Value))
		{
			return Value;   // 먼저 찾은 것을 쓴다. 단계값이 있으면 그 단계의 최종값
		}
	}

	return Default;
}

float UHBPlayerSkillGABase::GetAugmentParam(FGameplayTag ParamTag, float Default) const
{
	return GetAugmentParamFrom(GetCurrentActorInfo(), ParamTag, Default);
}

TSubclassOf<UGameplayEffect> UHBPlayerSkillGABase::FindAdditionalCostEffect(const FGameplayAbilityActorInfo* ActorInfo) const
{
	for (const FHBSkillAugment& Augment : Augments)
	{
		if (!Augment.AdditionalCostEffect)
		{
			continue;
		}
		if (!HasAugmentFrom(ActorInfo, Augment.AugmentTag))
		{
			continue;
		}
		return Augment.AdditionalCostEffect;
	}

	// 쳐낼 조건: 추가 코스트를 두 개 이상 동시에 내야 하면 배열을 돌려주도록 바꾼다.
	// 지금은 코스트를 건드리는 증강이 한 스킬에 하나뿐이라는 전제다.
	return nullptr;
}

UAbilitySystemComponent* UHBPlayerSkillGABase::GetASC(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}
	return ActorInfo->AbilitySystemComponent.Get();
}

AHBPlayerCharacter* UHBPlayerSkillGABase::GetPlayerFrom(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	// ASC 는 PlayerState 에 있으므로 OwnerActor 가 아니라 AvatarActor 를 봐야 한다.
	return Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
}

AHBPlayerCharacter* UHBPlayerSkillGABase::GetPlayer() const
{
	// CDO 이거나 아직 부여 전이면 CurrentActorInfo 가 없다. 그때는 nullptr 이다.
	return GetPlayerFrom(GetCurrentActorInfo());
}
