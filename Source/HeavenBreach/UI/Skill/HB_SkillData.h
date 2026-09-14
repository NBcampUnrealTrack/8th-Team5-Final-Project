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

    // 값에 색이 들어간 것. RichTextBlock 이 있는 카드만 쓴다.
    //
    // 둘을 함께 들고 다니는 이유: 어느 쪽이 필요한지는 위젯이 알고,
    // 만드는 쪽(MakeCardData)은 모른다. 태그를 떼는 방식은 원문에
    // 부등호가 들어가면 문장을 망가뜨린다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FText DescriptionRich;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    class UTexture2D* Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    int32 ManaCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float DisplayCooldown;
};