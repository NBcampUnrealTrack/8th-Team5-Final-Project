#pragma once

#include "CoreMinimal.h"
#include "AnimNotifyState_TimedNiagaraEffect.h"
#include "HBFirstPersonWeaponTrailNotify.generated.h"

#pragma region TemporaryFirstPersonWeaponTrail
// TODO: Replace this test-only routing when dual-view equipment enters the production character.
UCLASS(meta=(DisplayName="Test Dual View Warrior Weapon Trail"))
class HEAVENBREACH_API UHBFirstPersonWeaponTrailNotify : public UAnimNotifyState_TimedNiagaraEffect
{
	GENERATED_BODY()

public:
	UHBFirstPersonWeaponTrailNotify(const FObjectInitializer& ObjectInitializer);
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual UFXSystemComponent* SpawnEffect(USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation) const override;
};
#pragma endregion
