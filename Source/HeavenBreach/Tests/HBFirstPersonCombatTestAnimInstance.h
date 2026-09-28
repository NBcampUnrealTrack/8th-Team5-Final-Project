#pragma once

#include "CoreMinimal.h"
#include "Player/Animations/HBPlayerAnimInst.h"
#include "HBFirstPersonCombatTestAnimInstance.generated.h"

class UAnimSequence;
class UAnimSequenceBase;

// Drives only the isolated first-person combat lab's cosmetic pose correction.
UCLASS(Transient, Blueprintable)
class HEAVENBREACH_API UHBFirstPersonCombatTestAnimInstance : public UHBPlayerAnimInst
{
	GENERATED_BODY()

public:
#pragma region TemporaryFirstPersonCombatPose
	// TODO: Replace with the reviewed production first-person animation layer after the pose audit.
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// Editor utility restricted to animation assets in this combat lab.
	UFUNCTION(BlueprintCallable, Category="Test|CombatPose")
	static bool ReplaceTestMontageSequence(UAnimMontage* Montage, UAnimSequence* Source, UAnimSequence* Replacement);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test|CombatPose")
	TObjectPtr<UAnimMontage> GuardMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test|CombatPose")
	TObjectPtr<UAnimSequenceBase> NeutralViewAnimation;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	TObjectPtr<UAnimSequenceBase> CurrentViewAnimation;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	float ViewAnimationTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	float ViewAnimationWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	FTransform ViewRotation = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	FVector ViewCrouchOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	FTransform WorldRightHand = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test|CombatPose")
	FVector NeutralRightShoulder = FVector(-11.f, 5.f, 159.583f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test|CombatPose")
	FVector NeutralLeftShoulder = FVector(11.f, 7.f, 172.083f);

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	FVector RightShoulderPosition = FVector(-11.f, 5.f, 151.583f);

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	FVector LeftShoulderPosition = FVector(11.f, 7.f, 164.083f);

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	float GuardClearanceAlpha = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Test|CombatPose")
	FVector CombatOffhandClearance = FVector(34.f, 12.f, -32.f);

	UPROPERTY(BlueprintReadOnly, Transient, Category="Test|CombatPose")
	FVector GuardViewOffset = FVector(32.f, 12.f, -22.f);
#pragma endregion
};
