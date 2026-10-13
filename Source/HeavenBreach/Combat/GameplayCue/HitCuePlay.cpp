#include "HitCuePlay.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Core/Tag/Cue/HB_CueTag.h"

void HitCuePlay::PlayImpact(AActor* Instigator, AActor* Target, const FHitResult& Hit, float Magnitude, const FGameplayTagContainer& ExtraSourceTags, FGameplayTag OverrideCueTag)
{
	// 모든 클라에 전달을 위해 발동은 서버에서 한다.
	if (!Instigator || !Instigator->HasAuthority())
	{
		return;
	}

	// cue는 공격자 ASC 로 쏜다. 대상에 ASC 가 없어도 동작해야 하기 때문이다.
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Instigator);
	if (!SourceASC)
	{
		return;
	}

	FGameplayCueParameters CueParams;
	CueParams.Location = Hit.ImpactPoint;
	CueParams.Normal = Hit.ImpactNormal;
	CueParams.Instigator = Instigator;
	CueParams.EffectCauser = Instigator;
	CueParams.SourceObject = Target;
	CueParams.RawMagnitude = Magnitude;

	// 재질을 확정해서 넣는다. Cue_Brust의 Allowed Surface Types이 값을 본다.
	CueParams.PhysicalMaterial = ResolvePhysMaterial(Hit);

	// 공격자 태그를 담는다. 여기서 성질을 읽는다.
	SourceASC->GetOwnedGameplayTags(CueParams.AggregatedSourceTags);
	CueParams.AggregatedSourceTags.AppendTags(ExtraSourceTags);

	// 지정된 큐가 있으면 성질 판정을 건너뛴다. (특정 스킬의 고유 연출. 그 경우 대상의 재질을 판단안함)
	FGameplayTag CueTagToPlay = OverrideCueTag.IsValid()? OverrideCueTag : ResolveCueTag(CueParams.AggregatedSourceTags);

	UE_LOG(LogTemp, Warning, TEXT("[Cue] Tag=%s  Phys=%s"), *CueTagToPlay.ToString(), *GetNameSafe(CueParams.PhysicalMaterial.Get()));

	// 큐를 사용해 이펙트/사운드 발동
	SourceASC->ExecuteGameplayCue(CueTagToPlay, CueParams);
}

FGameplayTag HitCuePlay::ResolveCueTag(const FGameplayTagContainer& SourceTags)
{
	// 공격자가 들고 있는 GameplayCue.Hit.* 를 그대로 쓴다.
	// 어빌리티의 ActivationOwnedTags 나 발사체가 넘긴 태그에 들어 있다.
	//
	// 왜 매핑을 안 하는가: 예전에는 Impact.X 를 달고 여기서 GameplayCue.Hit.X 로
	// 바꿔줬는데, 둘이 1:1 이라 매핑 코드가 전부 중복이었고 타격 종류가 늘 때마다
	// if 도 같이 늘었다. 별칭을 없애니 이 함수가 더 이상 자라지 않는다.
	//
	// 쳐낼 조건: 태그 하나가 여러 큐로 갈라져야 하면 그때 다시 매핑이 필요해진다.
	for (const FGameplayTag& Tag : SourceTags)
	{
		// 부모(GameplayCue.Hit)만 달려 있으면 대응하는 GCN 이 없다. 건너뛴다.
		if (Tag == CueTag::TAG_GameplayCue_Hit)
		{
			continue;
		}
		if (Tag.MatchesTag(CueTag::TAG_GameplayCue_Hit))
		{
			return Tag;
		}
	}

	// 성질을 지정하지 않은 공격이다.
	return CueTag::TAG_GameplayCue_Hit_Default;
}

UPhysicalMaterial* HitCuePlay::ResolvePhysMaterial(const FHitResult& Hit)
{
	// 트레이스로 맞은 경우는 여기에 이미 들어 있다.
	if (UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
	{
		return PhysMat;
	}

	// 발사체처럼 엔진 내부 충돌로 맞은 경우 Hit에 재질이 안 채워진다.
	// bReturnPhysicalMaterial을 켤수 없는 경로라서 맞은 컴포넌트에게 재질이 무엇인지 직접 묻는다.
	if (UPrimitiveComponent* HitComp = Hit.GetComponent())
	{
		if (FBodyInstance* Body = HitComp->GetBodyInstance(Hit.BoneName))
		{
			return Body->GetSimplePhysicalMaterial();
		}
	}

	return nullptr;
}

HitCuePlay::FScopedImpactTag::FScopedImpactTag(AActor* Instigator, FGameplayTag ImpactTag)
{
	// 성질을 지정하지 않은 발사체다. 맞는 쪽이 기본 큐로 처리한다.
	if (!ImpactTag.IsValid())
	{
		return;
	}

	if (!Instigator)
	{
		return;
	}

	// 큐는 서버에서만 쏜다(PlayImpact). 클라에서 붙일 이유가 없다.
	if (!Instigator->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* InstigatorASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Instigator);
	if (!InstigatorASC)
	{
		return;
	}

	InstigatorASC->AddLooseGameplayTag(ImpactTag);
	ASC = InstigatorASC;
	Tag = ImpactTag;
}

HitCuePlay::FScopedImpactTag::~FScopedImpactTag()
{
	// 붙인 적이 없거나 그 사이 발사자가 사라졌다.
	UAbilitySystemComponent* InstigatorASC = ASC.Get();
	if (!InstigatorASC)
	{
		return;
	}

	InstigatorASC->RemoveLooseGameplayTag(Tag);
}
