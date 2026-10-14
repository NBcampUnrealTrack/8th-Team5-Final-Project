// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/GA_MagicElementalOrb.h"

#include "AI/EnemyCharacterBase.h"
#include "Camera/CameraComponent.h"
#include "Combat/Ability/Magic/HBMageOrb.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Engine/World.h"
#include "Player/HBPlayerCharacter.h"

UGA_MagicElementalOrb::UGA_MagicElementalOrb()
{
	Cooldown = 6.f;
	ManaCost = 20.f;
	bUseAttackSpeed = false;
	RequiredWeaponTags.AddTag(ItemTag::TAG_Weapon_Staff);

	CostGameplayEffectClass = UGE_SkillManaCost::StaticClass();
	CooldownGameplayEffectClass = UGE_MagicOrbCooldown::StaticClass();

	bIsCasting = true;
	ReleaseTime = 0.4f;
	CastHandFxIndex = 1;
	CastHandScale = 0.2f;

	OrbClass = AHBMageOrb::StaticClass();

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Power = Augments.AddDefaulted_GetRef();
	Power.AugmentTag = MagicTag::TAG_Augment_Magic_ElementalOrb_Power;
	Power.DisplayName = NSLOCTEXT("MagicElementalOrb", "PowerName", "원소 구체: 위력");
	Power.Description = NSLOCTEXT("MagicElementalOrb", "PowerDesc", "원소 구체 피해가 늘어납니다. (1단계 15%, 2단계 30%, 3단계 50%)");
	Power.Params.Add(SkillParamTag::TAG_SkillParam_Damage, 1.15f);
	Power.TierMagnitudes = { 1.15f, 1.30f, 1.50f };

	FHBSkillAugment& Rapid = Augments.AddDefaulted_GetRef();
	Rapid.AugmentTag = MagicTag::TAG_Augment_Magic_ElementalOrb_Rapid;
	Rapid.DisplayName = NSLOCTEXT("MagicElementalOrb", "RapidName", "원소 구체: 속사");
	Rapid.Description = NSLOCTEXT("MagicElementalOrb", "RapidDesc", "원소 구체 재사용 대기시간이 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Rapid.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Rapid.TierMagnitudes = { 0.85f, 0.75f, 0.65f };

	FHBSkillAugment& Wide = Augments.AddDefaulted_GetRef();
	Wide.AugmentTag = MagicTag::TAG_Augment_Magic_ElementalOrb_Wide;
	Wide.DisplayName = NSLOCTEXT("MagicElementalOrb", "WideName", "원소 구체: 폭발 범위");
	Wide.Description = NSLOCTEXT("MagicElementalOrb", "WideDesc", "원소 구체 폭발 범위가 넓어집니다. (기본 2m 에서 1단계 2.5m, 2단계 3m, 3단계 3.5m)");
	Wide.Params.Add(SkillParamTag::TAG_SkillParam_Radius, 250.f);
	Wide.TierMagnitudes = { 250.f, 300.f, 350.f };
}

void UGA_MagicElementalOrb::OnCastReleased(const FGameplayAbilityActorInfo* Info)
{
	AHBPlayerCharacter* Player = GetPlayerFrom(Info);
	UWorld* World = GetWorld();
	if (!Player || !World || !OrbClass || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	const UCameraComponent* Camera = Player->GetFirstPersonCamera();
	const FVector CameraLocation = Camera ? Camera->GetComponentLocation() : Player->GetActorLocation();

	FVector AimPoint = Player->ComputeAimPoint(AimTraceDistance);

	// ComputeAimPoint 는 Visibility 채널이다. 적 충돌 프로필("Enemy")은 Visibility 를 무시해서
	// 적을 조준해도 조준선이 적을 지나 뒤 벽·바닥이 조준점이 된다. 그 사이에 적이 있으면 적에 맞은 점을 조준점으로 쓴다(표식과 같다).
	if (Camera)
	{
		FCollisionQueryParams AimParams(SCENE_QUERY_STAT(MagicOrbAim), false, Player);
		TArray<FHitResult> AimHits;
		World->LineTraceMultiByObjectType(AimHits, CameraLocation, AimPoint, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects), AimParams);
		for (const FHitResult& AimHit : AimHits)
		{
			if (Cast<AEnemyCharacterBase>(AimHit.GetActor()))
			{
				AimPoint = AimHit.ImpactPoint;
				break;
			}
		}
	}

	// 시작점은 왼손. 소켓이 없으면 카메라 위치.
	FVector Start = CameraLocation;
	if (const USkeletalMeshComponent* Body = Player->GetMesh())
	{
		if (Body->DoesSocketExist(MuzzleSocketName))
		{
			Start = Body->GetSocketLocation(MuzzleSocketName);
		}
	}

	FVector Dir = (AimPoint - Start).GetSafeNormal();
	if (Dir.IsZero())
	{
		Dir = Camera ? Camera->GetForwardVector() : Player->GetActorForwardVector();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Player;
	SpawnParams.Instigator = Player;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHBMageOrb* Orb = World->SpawnActor<AHBMageOrb>(OrbClass, Start, Dir.Rotation(), SpawnParams);
	if (!Orb)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 원소 구체 스폰 실패"));
		return;
	}

	const float Coef = DamageCoef * GetAugmentParam(SkillParamTag::TAG_SkillParam_Damage, 1.f);
	const float Radius = GetAugmentParam(SkillParamTag::TAG_SkillParam_Radius, ExplosionRadius);
	Orb->Launch(Player, Dir, Speed, Coef, CastId, Radius, EdgeDamageFraction);
	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 원소 구체 발사  계수=%.2f  폭발 반경=%.0f  시전ID=%d"), Coef, Radius, CastId);
}
