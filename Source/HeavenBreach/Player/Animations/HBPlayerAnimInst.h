#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HBPlayerAnimInst.generated.h"

class AHBPlayerCharacter;

UCLASS()
class HEAVENBREACH_API UHBPlayerAnimInst : public UAnimInstance
{
	GENERATED_BODY()
	
protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
	UPROPERTY(BlueprintReadOnly)
	float MoveSpeed;
	
	UPROPERTY(BlueprintReadOnly)
	bool bIsFalling;
	
	UPROPERTY(BlueprintReadOnly)
	float FallSpeed;
	
	UPROPERTY(BlueprintReadOnly)
	float Direction;
	
	UPROPERTY(BlueprintReadOnly)
	bool bIsCrouching;

#pragma region AimOffset
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Aim Offset")
	float AimPitch = 0.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Aim Offset", meta = (ClampMin = "0.0"))
	float AimPitchInterpSpeed = 10.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Aim Offset", meta = (ClampMin = "-90.0", ClampMax = "0.0"))
	float AimPitchMin = -90.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Aim Offset", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float AimPitchMax = 60.0f;
#pragma endregion
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AHBPlayerCharacter> OwnerInst;

	// ----- 활 (활 AnimBP 가 캐스트 없이 읽는다) -----
	// 활을 당기는 중인가. AHBPlayerCharacter::IsBowDrawing.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Bow")
	bool bBowDrawing = false;

	// 손에 든 스켈레탈 무기(활). 테스트 AnimBP 의 BowVisual 자리. 활이 아니면 메시가 비어 있다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Bow")
	TObjectPtr<USkeletalMeshComponent> WeaponSkeletalMesh;

	// 활마다 다른 시위 IK 보정값. 외형 표(DT_WeaponPresentation)에서 온다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Bow")
	float StringIKOffset = 0.f;
};
