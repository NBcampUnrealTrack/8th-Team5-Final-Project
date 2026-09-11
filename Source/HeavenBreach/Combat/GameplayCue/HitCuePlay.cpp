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
	// 공격자가 어떤 성질의 공격을 했는지 본다.
	// 어빌리티의 ActivationOwnedTags나 발사체가 넘긴 태그에 들어 있다.
	if (SourceTags.HasTag(CueTag::TAG_Impact_Slash))
	{
		return CueTag::TAG_GameplayCue_Hit_Slash;
	}
	if (SourceTags.HasTag(CueTag::TAG_Impact_Blunt))
	{
		return CueTag::TAG_GameplayCue_Hit_Blunt;
	}
	if (SourceTags.HasTag(CueTag::TAG_Impact_Pierce))
	{
		return CueTag::TAG_GameplayCue_Hit_Pierce;
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
