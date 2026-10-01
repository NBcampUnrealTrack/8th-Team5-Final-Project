#include "PCGModule/HBChestPlacementComponent.h"
#include "Loot/BoxActor.h"

UHBChestPlacementComponent::UHBChestPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHBChestPlacementComponent::Validate(TArray<FString>& OutErrors) const
{
	OutErrors.Reset();
	if (Groups.IsEmpty()) OutErrors.Add(TEXT("Chest placement needs at least one group."));

	TSet<FName> GroupIds;
	bool bPlacesAnyChest = false;
	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		const FHBChestPlacementGroup& Group = Groups[GroupIndex];
		const FString Label = FString::Printf(TEXT("Chest group %d"), GroupIndex);
		if (Group.GroupId.IsNone() || GroupIds.Contains(Group.GroupId))
			OutErrors.Add(Label + TEXT(" needs a nonempty, unique GroupId."));
		GroupIds.Add(Group.GroupId);
		if (Group.MinCount < 0 || Group.MaxCount < Group.MinCount)
			OutErrors.Add(Label + TEXT(" count range must satisfy 0 <= MinCount <= MaxCount."));
		if (Group.MaxCount > 0)
		{
			bPlacesAnyChest = true;
			if (Group.Chests.IsEmpty()) OutErrors.Add(Label + TEXT(" needs a chest class when MaxCount is positive."));
		}
		TSet<UClass*> Classes;
		int64 TotalWeight = 0;
		for (int32 OptionIndex = 0; OptionIndex < Group.Chests.Num(); ++OptionIndex)
		{
			const FHBChestClassOption& Option = Group.Chests[OptionIndex];
			if (!Option.ChestClass || Option.Weight <= 0)
				OutErrors.Add(FString::Printf(TEXT("%s option %d needs a box class and positive weight."), *Label, OptionIndex));
			if (Option.ChestClass && Classes.Contains(Option.ChestClass.Get()))
				OutErrors.Add(Label + TEXT(" contains the same box class more than once."));
			if (Option.ChestClass) Classes.Add(Option.ChestClass.Get());
			const ABoxActor* Defaults = Option.ChestClass ? Option.ChestClass->GetDefaultObject<ABoxActor>() : nullptr;
			if (Defaults && (!Defaults->GetIsReplicated() || Defaults->bOnlyRelevantToOwner))
				OutErrors.Add(FString::Printf(TEXT("%s box %s must replicate to other players; owner-only relevancy is unsupported for room spawns."),
					*Label, *Option.ChestClass->GetPathName()));
			if (Option.Weight > 0) TotalWeight += Option.Weight;
		}
		if (TotalWeight > MAX_int32) OutErrors.Add(Label + TEXT(" total chest weight exceeds int32 range."));
	}
	if (!bPlacesAnyChest)
		OutErrors.Add(TEXT("Chest placement has no chests to place; remove the setup actor for an empty room."));

	auto CheckNonnegative = [&OutErrors](float Value, const TCHAR* Name)
	{
		if (!FMath::IsFinite(Value) || Value < 0.f)
			OutErrors.Add(FString::Printf(TEXT("%s must be finite and nonnegative."), Name));
	};
	CheckNonnegative(MinEntryDistance, TEXT("MinEntryDistance"));
	CheckNonnegative(MinChestSpacing, TEXT("MinChestSpacing"));
	CheckNonnegative(MaxNavProjectionDistance, TEXT("MaxNavProjectionDistance"));
	CheckNonnegative(MaxNavProjectionHeight, TEXT("MaxNavProjectionHeight"));
	if (!FMath::IsFinite(MinFloorUpDot) || MinFloorUpDot < 0.f || MinFloorUpDot > 1.f)
		OutErrors.Add(TEXT("MinFloorUpDot must be finite and between zero and one."));
	return OutErrors.IsEmpty();
}
