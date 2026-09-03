#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HBJobData.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UTexture2D;
class USkeletalMesh;
class UAnimInstance;

/** UI metadata and the GAS class that actually executes one skill. */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBJobSkillData
{
	GENERATED_BODY()

	/** Stable save/load key. Do not rename after release (example: Warrior.BattleCry). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FName SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill", meta=(MultiLine="true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** UI display value. The real cooldown is still enforced by the ability. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill", meta=(ClampMin="0.0"))
	float DisplayCooldown = 0.f;

	/** Mana consumed when this skill is successfully activated. Basic attacks remain free. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill", meta=(ClampMin="0.0"))
	float ManaCost = 0.f;
};

UCLASS(BlueprintType)
class HEAVENBREACH_API UHBJobData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Job|Skills")
	TArray<FHBJobSkillData> GetBasicSkillCatalog() const;

	UFUNCTION(BlueprintPure, Category="Job|Skills")
	TArray<FHBJobSkillData> GetActiveSkillCatalog() const;

	UFUNCTION(BlueprintPure, Category="Job|Skills")
	TArray<FHBJobSkillData> GetPassiveSkillCatalog() const;

	const FHBJobSkillData* FindActiveSkill(FName SkillId) const;
	const FHBJobSkillData* FindPassiveSkill(FName SkillId) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job")
	FName JobId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job", meta=(MultiLine="true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Mesh used by this job. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Appearance")
	TSoftObjectPtr<USkeletalMesh> CharacterMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Appearance")
	TSubclassOf<UAnimInstance> AnimInstanceClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|GAS")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Skills")
	TArray<FHBJobSkillData> BasicSkills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Skills")
	TArray<FHBJobSkillData> ActiveSkills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Skills")
	TArray<FHBJobSkillData> PassiveSkills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Skills")
	FHBJobSkillData UltimateSkill;

	/** Compatibility fields for the existing Warrior asset. Move these entries into the catalogs above. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Legacy", meta=(AdvancedDisplay))
	TArray<TSubclassOf<UGameplayAbility>> BasicAbilities;

	/** 배열 순서가 액티브 스킬 슬롯 순서이며 현재 첫 번째 항목이 Q입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Legacy", meta=(AdvancedDisplay))
	TArray<TSubclassOf<UGameplayAbility>> ActiveAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Legacy", meta=(AdvancedDisplay))
	TArray<TSubclassOf<UGameplayAbility>> PassiveAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Legacy", meta=(AdvancedDisplay))
	TSubclassOf<UGameplayAbility> UltimateAbility;
};
