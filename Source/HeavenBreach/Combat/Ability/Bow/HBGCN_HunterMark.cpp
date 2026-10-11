// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Bow/HBGCN_HunterMark.h"

#include "Components/CapsuleComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	// 기문 님 테스트 폴더의 에셋. 정식 폴더로 옮기면 BP 자식에서 바꾸면 된다.
	const TCHAR* TestGlyphMaterialPath = TEXT("/Game/Levels/test_gm/BowRetarget/VFX/Skills/HunterMark/M_Test_HunterMarkBillboard.M_Test_HunterMarkBillboard");
	const TCHAR* TestPalmBurstPath = TEXT("/Game/Levels/test_gm/BowRetarget/VFX/Skills/HunterMark/NS_Test_HunterMarkPalmBurst.NS_Test_HunterMarkPalmBurst");
	const TCHAR* TestPulseClassPath = TEXT("/Game/Levels/test_gm/BowRetarget/Skills/HunterMark/BP_Test_HunterMarkPulse.BP_Test_HunterMarkPulse_C");
}

// ---------------- 머리 위 표식 ----------------

AHBGCN_HunterMarkGlyph::AHBGCN_HunterMarkGlyph()
{
	// GameplayCueTag 는 BP 자식에서 지정한다. 부모(C++)에 같은 태그가 있으면 엔진이 에셋 검색용
	// GameplayCueName 을 비워서 큐를 못 찾는다(UAbilitySystemGlobals::DeriveGameplayCueTagFromClass).
	bAutoDestroyOnRemove = true;
	bAutoAttachToOwner = false;
	// 같은 적에 여러 플레이어가 표식을 걸어도 표식은 하나만 보이게 한다.
	bUniqueInstancePerInstigator = false;

	Glyph = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("Glyph"));
	SetRootComponent(Glyph);
	Glyph->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glyph->SetCastShadow(false);

	GlyphMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TestGlyphMaterialPath));
}

bool AHBGCN_HunterMarkGlyph::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (!MyTarget || !Glyph)
	{
		return false;
	}

	if (UMaterialInterface* Material = GlyphMaterial.LoadSynchronous())
	{
		TArray<FMaterialSpriteElement> Elements;
		FMaterialSpriteElement& Element = Elements.AddDefaulted_GetRef();
		Element.Material = Material;
		Element.BaseSizeX = GlyphSize;
		Element.BaseSizeY = GlyphSize;
		Glyph->SetElements(Elements);
	}

	// 캡슐 꼭대기 위. 캐릭터가 아니면 액터 위치 기준으로 같은 높이.
	float HalfHeight = 90.f;
	if (const ACharacter* Character = Cast<ACharacter>(MyTarget))
	{
		HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	AttachToActor(MyTarget, FAttachmentTransformRules::KeepRelativeTransform);
	SetActorRelativeLocation(FVector(0.f, 0.f, HalfHeight + HeightAboveHead));
	Glyph->SetVisibility(true);
	return true;
}

// ---------------- 적용 순간 ----------------

UHBGCN_HunterMarkCast::UHBGCN_HunterMarkCast()
{
	// GameplayCueTag 는 BP 자식에서 지정한다(위 AHBGCN_HunterMarkGlyph 와 같은 이유).
	PalmBurst = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TestPalmBurstPath));
	PulseClass = TSoftClassPtr<AActor>(FSoftObjectPath(TestPulseClassPath));
}

bool UHBGCN_HunterMarkCast::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	// 손 입자: 오른손에서 조준 방향으로. 손목 회전을 따르면 뒤로 튄다(인계 3.3).
	if (UNiagaraSystem* Burst = PalmBurst.LoadSynchronous())
	{
		const ACharacter* Character = Cast<ACharacter>(MyTarget);
		USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		const FVector HandLocation = (Mesh && Mesh->DoesSocketExist(PalmSocket)) ? Mesh->GetSocketLocation(PalmSocket) : MyTarget->GetActorLocation();
		const APawn* Pawn = Cast<APawn>(MyTarget);
		const FRotator AimRotation = Pawn ? Pawn->GetBaseAimRotation() : MyTarget->GetActorRotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Burst, HandLocation, AimRotation, FVector(PalmBurstScale));
	}

	// 바닥 펄스: 표식 범위 중심. 반경이 없으면 건너뛴다.
	const float Radius = Parameters.RawMagnitude;
	if (Radius > KINDA_SMALL_NUMBER)
	{
		if (UClass* Pulse = PulseClass.LoadSynchronous())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const float Scale = Radius * 2.f / PulseMeshDiameter;
			const FTransform Transform(FRotator::ZeroRotator, Parameters.Location, FVector(Scale, Scale, 1.f));
			World->SpawnActor<AActor>(Pulse, Transform, SpawnParams);
		}
	}
	return true;
}
