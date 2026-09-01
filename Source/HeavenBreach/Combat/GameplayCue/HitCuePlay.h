#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

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
}