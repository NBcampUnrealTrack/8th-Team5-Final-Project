#include "Tests/HBFirstPersonWeaponTrailNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Tests/HBFirstPersonCombatTestCharacter.h"

#pragma region TemporaryFirstPersonWeaponTrail
// TODO: Move view routing to the production equipment VFX layer after the combat lab is approved.
namespace
{
	USkeletalMeshComponent* FindFirstPersonTrailMesh(const AHBFirstPersonCombatTestCharacter* Character)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Character);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetFName() == TEXT("TestFirstPersonMesh"))
			{
				return Mesh;
			}
		}
		return nullptr;
	}
}

UHBFirstPersonWeaponTrailNotify::UHBFirstPersonWeaponTrailNotify(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Remote autonomous poses on a listen server must dispatch this with the montage hit window.
	bIsNativeBranchingPoint = true;
}

void UHBFirstPersonWeaponTrailNotify::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	const auto* Character = MeshComp ? Cast<AHBFirstPersonCombatTestCharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character || Character->GetMesh() != MeshComp || Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (Character->IsLocallyControlled())
	{
		if (USkeletalMeshComponent* FirstPersonMesh = FindFirstPersonTrailMesh(Character))
		{
			Super::NotifyBegin(FirstPersonMesh, Animation, TotalDuration, EventReference);
		}
	}
}

void UHBFirstPersonWeaponTrailNotify::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	const auto* Character = MeshComp ? Cast<AHBFirstPersonCombatTestCharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character || Character->GetMesh() != MeshComp)
	{
		return;
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
	// Also clean up if local possession changed while the notify was active.
	if (USkeletalMeshComponent* FirstPersonMesh = FindFirstPersonTrailMesh(Character))
	{
		Super::NotifyEnd(FirstPersonMesh, Animation, EventReference);
	}
}

UFXSystemComponent* UHBFirstPersonWeaponTrailNotify::SpawnEffect(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation) const
{
	UFXSystemComponent* Effect = Super::SpawnEffect(MeshComp, Animation);
	if (Effect)
	{
		const bool bFirstPerson = MeshComp->GetFName() == TEXT("TestFirstPersonMesh");
		Effect->SetOnlyOwnerSee(bFirstPerson);
		Effect->SetOwnerNoSee(!bFirstPerson);
		Effect->FirstPersonPrimitiveType = bFirstPerson
			? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::WorldSpaceRepresentation;
		Effect->MarkRenderStateDirty();
		Effect->AddTickPrerequisiteComponent(MeshComp);
	}
	return Effect;
}
#pragma endregion
