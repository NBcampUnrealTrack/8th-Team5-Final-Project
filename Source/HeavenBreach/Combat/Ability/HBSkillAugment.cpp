#include "Combat/Ability/HBSkillAugment.h"

bool FHBSkillAugment::FindParam(FGameplayTag ParamTag, float& Out) const
{
	if (!ParamTag.IsValid())
	{
		return false;
	}

	const float* Found = Params.Find(ParamTag);
	if (!Found)
	{
		return false;
	}

	Out = *Found;
	return true;
}

FText FHBSkillAugment::GetFormattedDescription() const
{
	// 치환할 것이 없으면 그대로 돌려준다. 대부분의 설명이 여기서 끝난다.
	// FText::Format 은 포맷 패턴을 파싱하므로 필요 없을 때 부르지 않는다.
	if (Params.IsEmpty())
	{
		return Description;
	}

	const FString Raw = Description.ToString();
	if (!Raw.Contains(TEXT("{")))
	{
		return Description;
	}

	FFormatNamedArguments Args;
	for (const TPair<FGameplayTag, float>& Pair : Params)
	{
		// 치환자 이름은 태그의 마지막 조각이다.
		// Augment.Param.Cooldown 을 설명에서 {Augment.Param.Cooldown} 으로 쓰게 하면
		// 기획자가 매번 전체 경로를 적어야 한다.
		const FString Full = Pair.Key.GetTagName().ToString();

		FString Head;
		FString Key;
		if (!Full.Split(TEXT("."), &Head, &Key, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			Key = Full;   // 점이 없는 태그는 통째로 쓴다
		}

		Args.Add(Key, FText::AsNumber(Pair.Value));
	}

	return FText::Format(Description, Args);
}
