#include "PCGModule/HBRoomEncounterProfile.h"
#include "AI/EnemyCharacterBase.h"

namespace
{
	void ValidatePool(const TCHAR* Label, int32 Minimum, int32 Maximum,
		const TArray<FHBEnemySpawnOption>& Options, TArray<FString>& Errors)
	{
		if (Minimum < 0 || Maximum < Minimum)
		{
			Errors.Add(FString::Printf(TEXT("%s count range must satisfy 0 <= Min <= Max."), Label));
		}
		if (Maximum > 0 && Options.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s needs at least one enemy class when Max is positive."), Label));
		}
		TSet<UClass*> Classes;
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			const FHBEnemySpawnOption& Option = Options[Index];
			if (!Option.EnemyClass || Option.Weight <= 0)
			{
				Errors.Add(FString::Printf(TEXT("%s option %d needs an enemy class and a positive weight."), Label, Index));
			}
			if (Option.EnemyClass && Classes.Contains(Option.EnemyClass.Get()))
			{
				Errors.Add(FString::Printf(TEXT("%s contains the same enemy class more than once."), Label));
			}
			if (Option.EnemyClass) Classes.Add(Option.EnemyClass.Get());
		}
	}
}

bool UHBRoomEncounterProfile::Validate(TArray<FString>& OutErrors) const
{
	OutErrors.Reset();
	ValidatePool(TEXT("Common"), MinCommonCount, MaxCommonCount, CommonEnemies, OutErrors);
	ValidatePool(TEXT("Elite"), MinEliteCount, MaxEliteCount, EliteEnemies, OutErrors);
	if (BossClass && BossPointId.IsNone())
	{
		OutErrors.Add(TEXT("BossClass needs a dedicated BossPointId."));
	}
	if (!BossClass && !BossPointId.IsNone())
	{
		OutErrors.Add(TEXT("BossPointId needs a BossClass."));
	}
	TSet<UClass*> AssignedClasses;
	for (const FHBEnemySpawnOption& Option : CommonEnemies)
	{
		if (Option.EnemyClass) AssignedClasses.Add(Option.EnemyClass.Get());
	}
	for (const FHBEnemySpawnOption& Option : EliteEnemies)
	{
		if (Option.EnemyClass && AssignedClasses.Contains(Option.EnemyClass.Get()))
		{
			OutErrors.Add(TEXT("An enemy class cannot be both common and elite."));
		}
	}
	if (BossClass && (AssignedClasses.Contains(BossClass.Get()) || EliteEnemies.ContainsByPredicate(
		[this](const FHBEnemySpawnOption& Option) { return Option.EnemyClass.Get() == BossClass.Get(); })))
	{
		OutErrors.Add(TEXT("A boss class cannot also be common or elite."));
	}
	if (MaxCommonCount == 0 && MaxEliteCount == 0 && !BossClass)
	{
		OutErrors.Add(TEXT("Encounter profile has no monsters to place; remove the profile for a safe room."));
	}
	return OutErrors.IsEmpty();
}
