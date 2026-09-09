// Copyright HeavenBreach Team5. All Rights Reserved.

#include "SealedDoorTypes.h"

FName SealedDoorGradeToRowName(ESealedDoorGrade InGrade)
{
	switch (InGrade)
	{
	case ESealedDoorGrade::Common:		return FName(TEXT("Common"));
	case ESealedDoorGrade::Rare:		return FName(TEXT("Rare"));
	case ESealedDoorGrade::Epic:		return FName(TEXT("Epic"));
	case ESealedDoorGrade::Celestial:	return FName(TEXT("Celestial"));
	default:							return FName(TEXT("Common"));
	}
}
