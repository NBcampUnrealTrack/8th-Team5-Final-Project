// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/Job/HBStartingLoadout.h"

// TSubclassOf<UGameplayAbility> 를 비교하려면 완전한 타입이 필요하다.
// HBJobData.h 는 전방 선언만 하므로 여기서 직접 끌어온다.
#include "Abilities/GameplayAbility.h"
#include "Player/Job/HBJobData.h"

bool ValidateStartingLoadout(const FHBStartingLoadout& InLoadout, FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!InLoadout.Job)
	{
		OutError = FText::FromString(TEXT("직업을 먼저 선택해 주세요."));
		return false;
	}

	// 슬롯은 비어 있어도 된다. 채워져 있을 때만 그 직업의 목록에 있는지 본다.
	const FHBJobSkillData* SkillQ = nullptr;
	if (!InLoadout.SkillQId.IsNone())
	{
		SkillQ = InLoadout.Job->FindActiveSkill(InLoadout.SkillQId);
		if (!SkillQ)
		{
			OutError = FText::FromString(TEXT("Q 슬롯의 스킬을 이 직업에서 찾을 수 없습니다."));
			return false;
		}
		if (!SkillQ->AbilityClass)
		{
			OutError = FText::FromString(TEXT("Q 슬롯 스킬에 어빌리티가 설정되어 있지 않습니다."));
			return false;
		}
	}

	const FHBJobSkillData* SkillE = nullptr;
	if (!InLoadout.SkillEId.IsNone())
	{
		SkillE = InLoadout.Job->FindActiveSkill(InLoadout.SkillEId);
		if (!SkillE)
		{
			OutError = FText::FromString(TEXT("E 슬롯의 스킬을 이 직업에서 찾을 수 없습니다."));
			return false;
		}
		if (!SkillE->AbilityClass)
		{
			OutError = FText::FromString(TEXT("E 슬롯 스킬에 어빌리티가 설정되어 있지 않습니다."));
			return false;
		}
	}

	// 둘 다 채워졌을 때만 겹침을 본다. 하나가 비어 있으면 겹칠 일이 없다.
	if (SkillQ && SkillE)
	{
		if (InLoadout.SkillQId == InLoadout.SkillEId)
		{
			OutError = FText::FromString(TEXT("Q 와 E 에 같은 스킬을 넣을 수 없습니다."));
			return false;
		}
		if (SkillQ->AbilityClass == SkillE->AbilityClass)
		{
			OutError = FText::FromString(TEXT("Q 와 E 의 스킬이 같은 어빌리티를 가리킵니다."));
			return false;
		}
	}

	if (!InLoadout.PassiveSkillId.IsNone())
	{
		const FHBJobSkillData* Passive = InLoadout.Job->FindPassiveSkill(InLoadout.PassiveSkillId);
		if (!Passive)
		{
			OutError = FText::FromString(TEXT("패시브를 이 직업에서 찾을 수 없습니다."));
			return false;
		}
		if (!Passive->AbilityClass)
		{
			OutError = FText::FromString(TEXT("패시브에 어빌리티가 설정되어 있지 않습니다."));
			return false;
		}
	}

	return true;
}

bool MakeDefaultStartingLoadout(UHBJobData* InJob, FHBStartingLoadout& OutLoadout, FText& OutError)
{
	OutError = FText::GetEmpty();

	if (!InJob)
	{
		OutError = FText::FromString(TEXT("기본 구성을 만들 직업 데이터가 없습니다."));
		return false;
	}

	// 실패했을 때 호출자의 값이 반쯤 채워진 채로 오염되지 않도록 따로 모은다.
	FHBStartingLoadout Candidate;
	Candidate.Job = InJob;

	for (const FName SkillId : InJob->InitialSkillIds)
	{
		if (SkillId.IsNone())
		{
			continue;
		}

		if (InJob->FindActiveSkill(SkillId))
		{
			if (Candidate.SkillQId.IsNone())
			{
				Candidate.SkillQId = SkillId;
				continue;
			}
			if (Candidate.SkillEId.IsNone())
			{
				Candidate.SkillEId = SkillId;
			}
			// 셋째부터는 담을 슬롯이 없다. 조용히 버린다.
			continue;
		}

		if (InJob->FindPassiveSkill(SkillId))
		{
			if (Candidate.PassiveSkillId.IsNone())
			{
				Candidate.PassiveSkillId = SkillId;
			}
			continue;
		}

		// 오타를 조용히 넘기지 않는다. 다만 나머지 스킬까지 죽이지는 않는다.
		UE_LOG(LogTemp, Error,
			TEXT("[SkillLoadout] 기본 구성의 '%s' 를 %s 의 스킬 목록에서 찾지 못해 건너뛴다."),
			*SkillId.ToString(), *InJob->JobId.ToString());
	}

	if (!ValidateStartingLoadout(Candidate, OutError))
	{
		return false;
	}

	OutLoadout = Candidate;
	return true;
}
