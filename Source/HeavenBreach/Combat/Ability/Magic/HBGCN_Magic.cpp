// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/HBGCN_Magic.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	// 기본 시전 이펙트(테스트 폴더 에셋). 정식 폴더로 옮기면 BP 자식에서 바꾸면 된다.
	const TCHAR* CastSystemPaths[] = {
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ElementalOrb/NS_Mage_ElementalOrb_Cast.NS_Mage_ElementalOrb_Cast"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/Teleport/NS_Mage_Teleport_Cast.NS_Mage_Teleport_Cast"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/FrostField/NS_Mage_FrostField_Cast.NS_Mage_FrostField_Cast"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ManaExplosion/NS_Mage_ManaExplosion_Cast.NS_Mage_ManaExplosion_Cast"),
	};
}

// ---------------- 시전 손 이펙트 ----------------

AHBGCN_MagicCastHand::AHBGCN_MagicCastHand()
{
	// GameplayCueTag 는 BP 자식에서 지정한다. 부모(C++)에 같은 태그가 있으면 엔진이 에셋 검색용
	// GameplayCueName 을 비워서 큐를 못 찾는다(UAbilitySystemGlobals::DeriveGameplayCueTagFromClass).
	bAutoDestroyOnRemove = true;
	bAutoAttachToOwner = false;
	// 시전자마다 자기 손 이펙트를 가진다.
	bUniqueInstancePerInstigator = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CastSystemPaths); ++Index)
	{
		CastSystems.Add(Index + 1, TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(CastSystemPaths[Index])));
	}
}

bool AHBGCN_MagicCastHand::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	const ACharacter* Character = Cast<ACharacter>(MyTarget);
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return false;
	}

	const TSoftObjectPtr<UNiagaraSystem>* Found = CastSystems.Find(Parameters.AbilityLevel);
	UNiagaraSystem* System = Found ? Found->LoadSynchronous() : nullptr;
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 손 큐: 이펙트 종류 %d 에 해당하는 나이아가라가 없다"), Parameters.AbilityLevel);
		return false;
	}

	// 이전 이펙트가 남아 있으면 정리한다(큐 재추가).
	if (ActiveFx)
	{
		ActiveFx->DestroyComponent();
		ActiveFx = nullptr;
	}

	// Location 은 월드 좌표가 아니라 소켓 기준 오프셋이다.
	AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);
	SetActorRelativeLocation(Parameters.Location);

	const float Scale = Parameters.RawMagnitude > KINDA_SMALL_NUMBER ? Parameters.RawMagnitude : 1.f;
	ActiveFx = UNiagaraFunctionLibrary::SpawnSystemAttached(System, GetRootComponent(), NAME_None,
		FVector::ZeroVector, FRotator::ZeroRotator, FVector(Scale), EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None);
	return true;
}

bool AHBGCN_MagicCastHand::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (ActiveFx)
	{
		ActiveFx->Deactivate();
		ActiveFx->DestroyComponent();
		ActiveFx = nullptr;
	}
	return true;
}

// ---------------- 일회성 연출 ----------------

bool UHBGCN_MagicBurst::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	UWorld* World = MyTarget ? MyTarget->GetWorld() : GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// 순간이동: AbilityLevel 1 = 도착. Alt 가 없으면 같은 클래스를 쓴다.
	const bool bUseAlt = Parameters.AbilityLevel == 1 && !ActorClassAlt.IsNull();
	UClass* SpawnClass = (bUseAlt ? ActorClassAlt : ActorClass).LoadSynchronous();
	if (!SpawnClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 버스트 큐 %s: 스폰할 액터 클래스가 비어 있다"), *GetName());
		return false;
	}

	float ScaleXY = DefaultScale;
	float ScaleZ = DefaultScale;
	if (ScaleDivisor > 0.f && Parameters.RawMagnitude > 0.f)
	{
		ScaleXY = Parameters.RawMagnitude / ScaleDivisor;
		ScaleZ = 1.f;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Transform(FRotator::ZeroRotator, Parameters.Location, FVector(ScaleXY, ScaleXY, ScaleZ));
	AActor* Spawned = World->SpawnActor<AActor>(SpawnClass, Transform, SpawnParams);
	if (Spawned && LifeSpan > 0.f)
	{
		Spawned->SetLifeSpan(LifeSpan);
	}
	return true;
}
