#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Player/Job/HBSkillDescription.h"
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

	// 치환자를 쓸 수 있다. "공격력 {Attack%} 상승" 처럼 적으면 애셋의 실제 수치가 들어간다.
	// 쓸 수 있는 이름은 HBSkillDescription.cpp 의 BuildRegistry 에 있다.
	// 읽을 때는 이 칸이 아니라 아래 두 함수를 쓸 것.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (MultiLine = "true"))
	FText Description;

	// 평문과 리치를 함께 얻는다. 스킬 카드처럼 색을 쓸 수 있는 곳이 쓴다.
	FHBFormattedText FormatDescription() const;

	/**
	 * 평문만. 태그가 섞이지 않는다.
	 *
	 * 왜 따로 두나: HB_PlayerState 의 신규 패시브 후보가 이것을 쓰는데,
	 * 그쪽 증강 카드는 아직 리치 텍스트가 아니다. 리치를 돌려주면
	 * 화면에 <Value> 가 글자로 찍힌다.
	 *
	 * USTRUCT 이라 UFUNCTION 을 못 붙인다. 블루프린트에서 Description 을 직접
	 * 꺼내면 치환 전 원문이 나온다. BP 에서 쓸 일이 생기면 UHBJobData 에
	 * BlueprintPure 래퍼를 하나 다는 것이 해법이다.
	 */
	FText GetFormattedDescription() const;

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
