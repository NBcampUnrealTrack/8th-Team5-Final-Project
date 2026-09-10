#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "HB_SkillData.h" // ESkillType 및 기존 데이터 참조용
#include "HB_AugmentData.generated.h"

UENUM(BlueprintType)
enum class EAugmentType : uint8
{
    ActiveUpgrade   UMETA(DisplayName = "Active Upgrade"), // 장착된 액티브 스킬 강화
    NewPassive      UMETA(DisplayName = "New Passive")     // 새로운 패시브 스킬 추가
};

USTRUCT(BlueprintType)
struct FAugmentCardData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
    FName AugmentId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
    EAugmentType AugmentType;

    // 이 증강이 강화할 기존 액티브 스킬 ID, 혹은 새로 지급할 패시브 스킬 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
    FName TargetSkillId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
    class UTexture2D* Icon;
};