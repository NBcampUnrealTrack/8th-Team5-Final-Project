#include "PCGModule/HBEncounterComponent.h"
#include "AI/EnemyCharacterBase.h"

UHBEncounterComponent::UHBEncounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHBEncounterComponent::Validate(TArray<FString>& OutErrors) const
{
	OutErrors.Reset();
	if (Groups.IsEmpty()) OutErrors.Add(TEXT("Encounter needs at least one monster group."));

	TSet<FName> GroupIds;
	bool bSpawnsAnything = false;
	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		const FHBEncounterGroup& Group = Groups[GroupIndex];
		const FString Label = FString::Printf(TEXT("Group %d"), GroupIndex);
		if (Group.GroupId.IsNone() || GroupIds.Contains(Group.GroupId))
			OutErrors.Add(Label + TEXT(" needs a nonempty, unique GroupId."));
		GroupIds.Add(Group.GroupId);
		TArray<FString> CountErrors;
		HBSpawnCountSelection::Validate(Group.MinCount, Group.MaxCount, Group.CountSelectionMode, Group.CountWeights, CountErrors);
		for (const FString& Error : CountErrors) OutErrors.Add(Label + TEXT(" ") + Error);
		int32 SelectableMinimum = 0, SelectableMaximum = 0;
		HBSpawnCountSelection::GetSelectableRange(Group.MinCount, Group.MaxCount,
			Group.CountSelectionMode, Group.CountWeights, SelectableMinimum, SelectableMaximum);
		if (!Group.ConditionGroupId.IsNone())
		{
			const int32 ConditionIndex = Groups.IndexOfByPredicate(
				[&Group](const FHBEncounterGroup& Other) { return Other.GroupId == Group.ConditionGroupId; });
			if (Group.ConditionGroupId == Group.GroupId || ConditionIndex == INDEX_NONE)
				OutErrors.Add(Label + TEXT(" ConditionGroupId must name another group in this encounter."));
			else
			{
				const FHBEncounterGroup& Condition = Groups[ConditionIndex];
				const bool bPlacedEarlier = static_cast<uint8>(Condition.Grade) > static_cast<uint8>(Group.Grade)
					|| (Condition.Grade == Group.Grade && ConditionIndex < GroupIndex);
				if (!bPlacedEarlier)
					OutErrors.Add(Label + TEXT(" ConditionGroupId must be placed earlier: use a higher grade, or the same grade listed above."));
			}
			if (Group.CountWeightsIfConditionSpawned.IsEmpty())
				OutErrors.Add(Label + TEXT(" needs CountWeightsIfConditionSpawned when ConditionGroupId is set."));
			else
			{
				TArray<FString> ConditionErrors;
				HBSpawnCountSelection::Validate(Group.MinCount, Group.MaxCount, EHBSpawnCountSelectionMode::Weighted,
					Group.CountWeightsIfConditionSpawned, ConditionErrors);
				for (const FString& Error : ConditionErrors) OutErrors.Add(Label + TEXT(" condition ") + Error);
				int32 ConditionMinimum = 0, ConditionMaximum = 0;
				HBSpawnCountSelection::GetSelectableRange(Group.MinCount, Group.MaxCount, EHBSpawnCountSelectionMode::Weighted,
					Group.CountWeightsIfConditionSpawned, ConditionMinimum, ConditionMaximum);
				SelectableMaximum = FMath::Max(SelectableMaximum, ConditionMaximum);
			}
		}
		if (SelectableMaximum > 0)
		{
			bSpawnsAnything = true;
			if (Group.Monsters.IsEmpty()) OutErrors.Add(Label + TEXT(" needs a monster class when a positive count can be selected."));
		}
		TSet<UClass*> Classes;
		int64 TotalWeight = 0;
		for (int32 OptionIndex = 0; OptionIndex < Group.Monsters.Num(); ++OptionIndex)
		{
			const FHBEncounterMonsterOption& Option = Group.Monsters[OptionIndex];
			if (!Option.EnemyClass || Option.Weight <= 0)
				OutErrors.Add(FString::Printf(TEXT("%s option %d needs an enemy class and positive weight."), *Label, OptionIndex));
			if (!FMath::IsFinite(Option.SpawnScale) || Option.SpawnScale <= 0.f)
				OutErrors.Add(FString::Printf(TEXT("%s option %d SpawnScale must be finite and positive."), *Label, OptionIndex));
			if (Option.EnemyClass && Classes.Contains(Option.EnemyClass.Get()))
				OutErrors.Add(Label + TEXT(" contains the same enemy class more than once."));
			if (Option.EnemyClass) Classes.Add(Option.EnemyClass.Get());
			const AEnemyCharacterBase* Defaults = Option.EnemyClass
				? Option.EnemyClass->GetDefaultObject<AEnemyCharacterBase>() : nullptr;
			if (Defaults && (!Defaults->GetIsReplicated() || !Defaults->IsReplicatingMovement()
				|| Defaults->bOnlyRelevantToOwner))
				OutErrors.Add(FString::Printf(TEXT("%s enemy %s must replicate its actor and movement to other players; owner-only relevancy is unsupported for room spawns."),
					*Label, *Option.EnemyClass->GetPathName()));
			if (Option.Weight > 0) TotalWeight += Option.Weight;
		}
		if (TotalWeight > MAX_int32) OutErrors.Add(Label + TEXT(" total monster weight exceeds int32 range."));
	}
	if (!bSpawnsAnything) OutErrors.Add(TEXT("Encounter has no monsters to place; remove the setup actor for a safe room."));

	auto CheckNonnegative = [&OutErrors](float Value, const TCHAR* Name)
	{
		if (!FMath::IsFinite(Value) || Value < 0.f)
			OutErrors.Add(FString::Printf(TEXT("%s must be finite and nonnegative."), Name));
	};
	CheckNonnegative(MinEntryDistance, TEXT("MinEntryDistance"));
	CheckNonnegative(MinMonsterSpacing, TEXT("MinMonsterSpacing"));
	CheckNonnegative(MaxNavProjectionDistance, TEXT("MaxNavProjectionDistance"));
	CheckNonnegative(MaxNavProjectionHeight, TEXT("MaxNavProjectionHeight"));
	if (!FMath::IsFinite(MinFloorUpDot) || MinFloorUpDot < 0.f || MinFloorUpDot > 1.f)
		OutErrors.Add(TEXT("MinFloorUpDot must be finite and between zero and one."));
	return OutErrors.IsEmpty();
}
