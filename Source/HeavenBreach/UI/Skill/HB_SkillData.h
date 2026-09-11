#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "HB_SkillData.generated.h"

UENUM(BlueprintType)
enum class ESkillType : uint8
{
    Active      UMETA(DisplayName = "Active"),
    Passive     UMETA(DisplayName = "Passive")
};


USTRUCT(BlueprintType)
struct FSkillCardData : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FName SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    ESkillType SkillType; // Active 또는 Passive 구분

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    class UTexture2D* Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    int32 ManaCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float DisplayCooldown;
};