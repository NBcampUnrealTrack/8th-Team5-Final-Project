// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/GameplayCue/GCN_HitImpact.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

bool UGCN_HitImpact::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	// 부모가 BurstEffects(파티클,데칼등등 이펙트)를 스폰하고 BP 의 OnBurst 도 호출한다.
	bool bHandled = Super::OnExecute_Implementation(Target, Parameters);

	USoundBase* Sound = DefaultSound;

	// 공격자가 실어 보낸 태그에서 타격 성질을 찾는다.
	for (const TPair<FGameplayTag, TObjectPtr<USoundBase>>& Pair : ImpactSounds)
	{
		if (Pair.Value && Parameters.AggregatedSourceTags.HasTag(Pair.Key))
		{
			Sound = Pair.Value;
			break;
		}
	}

	if (!Sound)
	{
		// 성질별 소리도 기본 소리도 없다. 애셋 설정 누락이다.
		UE_LOG(LogTemp, Warning, TEXT("[Cue] %s 에 재생할 소리가 없다"), *GetNameSafe(this));
		return bHandled;
	}

	// 타격 지점에서 재생한다.
	UWorld* World = Target ? Target->GetWorld() : nullptr;
	UGameplayStatics::PlaySoundAtLocation(World, Sound, Parameters.Location);

	return bHandled;
}
