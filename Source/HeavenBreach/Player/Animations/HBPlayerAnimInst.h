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

	// 주무기를 안 들었다. ABP 상체 분기(Blend Poses by bool)가 맨손 가드 자세로 바꾸는 데 쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bIsUnarmed = false;

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
};
