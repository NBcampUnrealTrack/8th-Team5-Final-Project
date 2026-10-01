// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "HBWeaponPresentation.generated.h"

class UAnimInstance;
class UAnimMontage;
class USkeletalMesh;
class UStaticMesh;

/**
 * 무기를 들었을 때의 외형·애니. DT_WeaponPresentation 의 행 구조체.
 *
 * 아이템 데이터(DT_Weapon)와 따로 둔다: 아이템 표는 다른 담당이고, 활마다 메시뿐 아니라
 * 몽타주·장전 화살·시위 보정값까지 달라서 한 줄로 묶는 게 낫다.
 * 행 이름은 DT_Weapon 의 행 이름(BOW_001 등). 없으면 WeaponType(Bow 등) 행을 쓴다.
 */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBWeaponPresentation : public FTableRowBase
{
	GENERATED_BODY()

	/** 시위가 움직이는 무기 메시. 비어 있으면 기존 스태틱 무기 표시를 쓴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** 무기 메시의 AnimBP (활: 시위 AnimBP) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TSubclassOf<UAnimInstance> SkeletalMeshAnimClass;

	/** 무기 스켈레탈 메시를 붙일 캐릭터 소켓 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	FName AttachSocket = TEXT("BowSocket");

	/** 이 무기를 들었을 때 캐릭터 AnimBP. 비어 있으면 기본 AnimBP */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> CharacterAnimClass;

	/** 활 GA 가 쓰는 몽타주. 비어 있으면 GA BP 의 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ReleaseMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> CancelMontage;

	/** 장전 중 손에 보이는 화살 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow")
	TSoftObjectPtr<UStaticMesh> HeldArrowMesh;

	/** 활마다 다른 시위 IK 보정값. AnimBP 가 메시 에셋을 비교하던 자리 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bow")
	float StringIKOffset = 0.f;
};

namespace HBWeaponPresentation
{
	/** 행 이름 → 무기 종류 순서로 찾는다. 둘 다 없으면 nullptr */
	inline const FHBWeaponPresentation* Find(const UDataTable* Table, const FName ItemRowName, const FString& WeaponType)
	{
		if (!Table)
		{
			return nullptr;
		}

		static const FString Context(TEXT("HBWeaponPresentation::Find"));
		if (!ItemRowName.IsNone())
		{
			if (const FHBWeaponPresentation* Row = Table->FindRow<FHBWeaponPresentation>(ItemRowName, Context, false))
			{
				return Row;
			}
		}

		if (!WeaponType.IsEmpty())
		{
			return Table->FindRow<FHBWeaponPresentation>(FName(*WeaponType), Context, false);
		}
		return nullptr;
	}
}
