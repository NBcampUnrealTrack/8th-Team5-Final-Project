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
	TObjectPtr<AHBPlayerCharacter> OwnerInst;
};
