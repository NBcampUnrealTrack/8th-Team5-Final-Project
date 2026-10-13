#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UAbilitySystemComponent;

/**
 * 타격 사운드/이펙트를 실행한다. 공격측이 부른다.
 * 공격방식 : 큐 태그로 고른다.
 * 피격된 재질 : PhysicalMaterial로 넘기고 애셋의 Allowed Surface Types가 필터
 */
namespace HitCuePlay
{
	// 가해자(Instigator)는 ASC를 가진 액터여야 한다. 피해자(Target) null 이어도 된다.
	void PlayImpact(AActor* Instigator, AActor* Target, const FHitResult& Hit, float Magnitude,const FGameplayTagContainer& ExtraSourceTags = FGameplayTagContainer(),FGameplayTag OverrideCueTag = FGameplayTag());

	// 공격자의 큐태그 확인
	FGameplayTag ResolveCueTag(const FGameplayTagContainer& SourceTags);
	// Hit대상에 재질이 없으면 맞은 컴포넌트에서 꺼낸다.
	UPhysicalMaterial* ResolvePhysMaterial(const FHitResult& Hit);

	// 발사체용. 이 객체가 살아 있는 동안만 발사자 ASC 에 성질 태그(GameplayCue.Hit.*)를 붙여 둔다.
	//
	// 왜 필요한가: 맞는 쪽이 PlayImpact 로 "공격자가 지금 들고 있는 태그"에서 성질을 고르는데,
	// 발사체는 GA 가 끝난 뒤에 맞아서 그 순간 공격자에게 태그가 없다. 그래서 기본 큐로 떨어졌다.
	// 근접 평타가 공격 중 루즈 태그를 붙이는 것과 같은 방식이고, 붙여 두는 범위만 맞는 순간으로 좁혔다.
	// 서버에서만 붙인다. 큐도 서버에서만 쏘므로 복제는 필요 없다.
	//
	// 쓰는 법: ApplyMeleeHit 를 부르는 줄과 같은 블록 안, 그 줄 바로 위에 만든다.
	//
	// 쳐낼 조건: 발사자에게 이미 다른 GameplayCue.Hit.* 가 붙어 있으면 어느 쪽이 이길지 정해져 있지 않다.
	// 그런 조합이 생기면 ApplyMeleeHit 에 성질 태그 인자를 추가하고 이것을 지운다.
	struct FScopedImpactTag
	{
		FScopedImpactTag(AActor* Instigator, FGameplayTag ImpactTag);
		~FScopedImpactTag();

		FScopedImpactTag(const FScopedImpactTag&) = delete;
		FScopedImpactTag& operator=(const FScopedImpactTag&) = delete;

	private:
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FGameplayTag Tag;
	};
}
