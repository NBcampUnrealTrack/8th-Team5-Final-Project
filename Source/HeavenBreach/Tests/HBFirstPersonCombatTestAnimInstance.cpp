#include "Tests/HBFirstPersonCombatTestAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tests/HBFirstPersonCombatTestCharacter.h"

#pragma region TemporaryFirstPersonCombatPose
void UHBFirstPersonCombatTestAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// TODO: Move these lab-only anchors into the reviewed production first-person animation layer.
	// Read game-thread state here; the animation graph consumes only copied values.
	const AHBFirstPersonCombatTestCharacter* Player = Cast<AHBFirstPersonCombatTestCharacter>(TryGetPawnOwner());
	const UAnimInstance* WorldAnim = Player && Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr;
	const UAnimMontage* ActiveMontage = WorldAnim ? WorldAnim->GetCurrentActiveMontage() : nullptr;
	const bool bGuarding = GuardMontage && ActiveMontage == GuardMontage;

	// Evaluate the same sequence/time without running notifies on the cosmetic mesh.
	bool bFoundViewSequence = false;
	if (!CurrentViewAnimation)
	{
		CurrentViewAnimation = NeutralViewAnimation;
	}
	if (ActiveMontage)
	{
		const float MontageTime = WorldAnim->Montage_GetPosition(ActiveMontage);
		for (const FSlotAnimationTrack& Slot : ActiveMontage->SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
			{
				float SequenceTime = 0.f;
				if (UAnimSequenceBase* Sequence = Segment.GetAnimationData(MontageTime, SequenceTime))
				{
					CurrentViewAnimation = Sequence;
					ViewAnimationTime = SequenceTime;
					bFoundViewSequence = true;
					break;
				}
			}
			if (bFoundViewSequence) break;
		}
	}
	ViewAnimationWeight = FMath::FInterpConstantTo(ViewAnimationWeight, bFoundViewSequence ? 1.f : 0.f, DeltaSeconds, 8.f);
	const float ViewAimPitch = Player ? FRotator::NormalizeAxis(Player->GetBaseAimRotation().Pitch) : 0.f;

	// The Warrior mesh faces +Y. Rotate around the view, rather than pinning shoulders in mesh space.
	// Use the same anchor for idle, normal attacks and every skill montage to avoid activation pops.
	const FQuat ViewPitch = FRotator(0.f, 0.f, -FMath::Clamp(ViewAimPitch, -90.f, 60.f)).Quaternion();
	ViewRotation = FTransform(ViewPitch);
	if (Player && Player->GetMesh())
	{
		WorldRightHand = Player->GetMesh()->GetSocketTransform(TEXT("hand_r"), RTS_Component);
	}
	const FVector PreviousCrouchOffset = ViewCrouchOffset;
	float TargetCrouchOffset = 0.f;
	if (Player && Player->GetCharacterMovement()->IsCrouching())
	{
		const auto* Defaults = Player->GetClass()->GetDefaultObject<AHBFirstPersonCombatTestCharacter>();
		TargetCrouchOffset = Player->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()
			- Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	}
	ViewCrouchOffset.Z = FMath::FInterpTo(ViewCrouchOffset.Z, TargetCrouchOffset, DeltaSeconds, 12.f);

	FVector ViewPivot(0.f, 18.663123f, 159.775104f);
	if (Player && Player->GetMesh())
	{
		if (const UCameraComponent* Camera = Player->FindComponentByClass<UCameraComponent>())
		{
			ViewPivot = Player->GetMesh()->GetComponentTransform().InverseTransformPosition(Camera->GetComponentLocation()) - PreviousCrouchOffset;
		}
	}
	RightShoulderPosition = ViewPivot + ViewPitch.RotateVector(NeutralRightShoulder - ViewPivot) + ViewCrouchOffset;
	LeftShoulderPosition = ViewPivot + ViewPitch.RotateVector(NeutralLeftShoulder - ViewPivot) + ViewCrouchOffset;
	GuardViewOffset = ViewPitch.RotateVector(CombatOffhandClearance);

	// Apply the reviewed shield centering offset only while guarding.
	GuardClearanceAlpha = FMath::FInterpConstantTo(GuardClearanceAlpha, bGuarding ? 1.f : 0.f, DeltaSeconds, 8.f);
}
bool UHBFirstPersonCombatTestAnimInstance::ReplaceTestMontageSequence(UAnimMontage* Montage, UAnimSequence* Source, UAnimSequence* Replacement)
{
#if WITH_EDITOR
	const FString LabPath(TEXT("/Game/Levels/test_gm/FirstPersonSeparation/"));
	if (!Montage || !Source || !Replacement || !Montage->GetPathName().StartsWith(LabPath) ||
		!Replacement->GetPathName().StartsWith(LabPath) || Source->GetSkeleton() != Replacement->GetSkeleton() ||
		!FMath::IsNearlyEqual(Source->GetPlayLength(), Replacement->GetPlayLength(), 0.001f))
	{
		return false;
	}
	Montage->Modify();
	// Replace only matching segments; ReplaceReferredAnimations removes unmapped segments.
	bool bReplaced = false;
	for (FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
	{
		for (FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
		{
			if (Segment.GetAnimReference() == Source)
			{
				Segment.SetAnimReference(Replacement);
				bReplaced = true;
			}
		}
	}
	if (!bReplaced)
	{
		return false;
	}
	Montage->PostEditChange();
	Montage->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}
#pragma endregion
