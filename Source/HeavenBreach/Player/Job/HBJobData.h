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

	// 저장,조회 키. 해당 키는 함부러 바꾸지 말 것. 예시) Warrior.BattleCry
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FName SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

	// DisplayCooldown / ManaCost 를 두지 않는다.
	// 실제 값은 GA 애셋의 Cooldown / ManaCost 이고, 증강이 붙으면 DA 값으로는 맞출 수 없다. 해당 UI는 AbilityClass 의 CDO 에서 읽는다.
};

UCLASS(BlueprintType)
class HEAVENBREACH_API UHBJobData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category="Job|Skills")
	TArray<FHBJobSkillData> GetActiveSkillCatalog() const { return ActiveSkills; }

	UFUNCTION(BlueprintPure, Category="Job|Skills")
	TArray<FHBJobSkillData> GetPassiveSkillCatalog() const { return PassiveSkills; }

	const FHBJobSkillData* FindActiveSkill(FName SkillId) const;
	const FHBJobSkillData* FindPassiveSkill(FName SkillId) const;

	// 스킬의 종류를 모를 때. UI가 SkillId로 조회 할 용도.
	const FHBJobSkillData* FindSkillAnywhere(FName SkillId) const;

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

	// 저장된 로드아웃도 로비 선택도 없을 때 쓰는 기본값. 앞에서부터 액티브는 Q, E 순서다.
	// 여기 적은ID 는 ActiveSkills 나 PassiveSkills 에 같은 SkillId 로 있어야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Job|Skills")
	TArray<FName> InitialSkillIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Skills", meta = (TitleProperty = "SkillId"))
	TArray<FHBJobSkillData> ActiveSkills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Skills", meta = (TitleProperty = "SkillId"))
	TArray<FHBJobSkillData> PassiveSkills;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Job|Skills")
	FHBJobSkillData UltimateSkill;

	// Legacy 배열 삭제됨. 쓸모가 적고 불편해...
};
