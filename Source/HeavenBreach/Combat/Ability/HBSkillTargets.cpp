// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/HBSkillTargets.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

namespace HBSkillTargets
{
	void FindInSphere(const UWorld* World, const FVector& Center, const float Radius, const AActor* Caster, TArray<AActor*>& OutTargets)
	{
		if (!World || Radius < 0.f)
		{
			return;
		}

		// 적 캡슐은 대개 "Enemy" 오브젝트 채널, 플레이어는 Pawn 이라 움직이는 오브젝트 전부를 본다.
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HBSkillTargets), false, Caster);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects), FCollisionShape::MakeSphere(Radius), QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Actor = Overlap.GetActor();
			if (!Actor || Actor == Caster || !Actor->Implements<UDamageableInterface>())
			{
				continue;
			}
			const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
			if (!ASC || ASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
			{
				continue;	// ASC 없는 것·시체는 뺀다
			}
			OutTargets.AddUnique(Actor);
		}
	}
}
