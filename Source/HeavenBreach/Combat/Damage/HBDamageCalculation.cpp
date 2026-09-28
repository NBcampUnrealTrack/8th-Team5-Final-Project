#include "Combat/Damage/HBDamageCalculation.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"

float HBDamage::GetAttackerHeadshotBonus(const AActor* Attacker)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	if (!ASC)
	{
		return 0.f;
	}

	// 적이 때렸으면 플레이어 어트리뷰트셋이 없다. bFound 가 false 로 돌아온다.
	// MeleeTraceComponent 가 적 공격력을 읽을 때 쓰는 것과 같은 방식이다.
	bool bFound = false;
	const float Bonus = ASC->GetGameplayAttributeValue(UHB_PlayerAttributeSet::GetPlayer_HeadshotDamageBonusAttribute(), bFound);

	if (!bFound)
	{
		return 0.f;
	}
	return Bonus;
}
