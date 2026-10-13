// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/GA_MagicFrostField.h"

#include "AbilitySystemComponent.h"
#include "AI/EnemyCharacterBase.h"
#include "Camera/CameraComponent.h"
#include "Combat/Ability/Magic/HBFrostField.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Engine/World.h"
#include "Player/HBPlayerCharacter.h"

UGA_MagicFrostField::UGA_MagicFrostField()
{
	Cooldown = 12.f;
	ManaCost = 25.f;
	bUseAttackSpeed = false;
	RequiredWeaponTags.AddTag(ItemTag::TAG_Weapon_Staff);

	CostGameplayEffectClass = UGE_SkillManaCost::StaticClass();
	CooldownGameplayEffectClass = UGE_MagicFrostCooldown::StaticClass();

	bIsCasting = false;
	ReleaseTime = 0.35f;
	CastHandFxIndex = 3;
	CastHandScale = 0.16f;

	FieldClass = AHBFrostField::StaticClass();

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Bitter = Augments.AddDefaulted_GetRef();
	Bitter.AugmentTag = MagicTag::TAG_Augment_Magic_FrostField_Bitter;
	Bitter.DisplayName = NSLOCTEXT("MagicFrostField", "BitterName", "서리 지대: 혹한");
	Bitter.Description = NSLOCTEXT("MagicFrostField", "BitterDesc", "서리 지대의 이동 속도 감소가 커집니다. (1단계 50%, 2단계 55%, 3단계 60%)");
	Bitter.Params.Add(SkillParamTag::TAG_SkillParam_Slow, 0.50f);
	Bitter.TierMagnitudes = { 0.50f, 0.55f, 0.60f };

	FHBSkillAugment& Wide = Augments.AddDefaulted_GetRef();
	Wide.AugmentTag = MagicTag::TAG_Augment_Magic_FrostField_Wide;
	Wide.DisplayName = NSLOCTEXT("MagicFrostField", "WideName", "서리 지대: 확장");
	Wide.Description = NSLOCTEXT("MagicFrostField", "WideDesc", "서리 지대의 반경이 넓어집니다. (1단계 3.5m, 2단계 4m, 3단계 4.5m)");
	Wide.Params.Add(SkillParamTag::TAG_SkillParam_Radius, 350.f);
	Wide.TierMagnitudes = { 350.f, 400.f, 450.f };
}

void UGA_MagicFrostField::OnCastStarted(const FGameplayAbilityActorInfo* Info)
{
	bTargetValid = false;

	AHBPlayerCharacter* Player = GetPlayerFrom(Info);
	UWorld* World = GetWorld();
	if (!Player || !World || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	const UCameraComponent* Camera = Player->GetFirstPersonCamera();
	FVector AimPoint = Player->ComputeAimPoint(AimTraceDistance);

	// ComputeAimPoint 는 Visibility 채널이다. 적 충돌 프로필("Enemy")은 Visibility 를 무시해서
	// 적을 조준해도 조준선이 적을 지나 뒤 벽·바닥이 조준점이 된다. 그 사이에 적이 있으면 적에 맞은 점을 조준점으로 쓴다(표식과 같다).
	if (Camera)
	{
		FCollisionQueryParams AimParams(SCENE_QUERY_STAT(MagicFrostAim), false, Player);
		TArray<FHitResult> AimHits;
		World->LineTraceMultiByObjectType(AimHits, Camera->GetComponentLocation(), AimPoint, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects), AimParams);
		for (const FHitResult& AimHit : AimHits)
		{
			if (Cast<AEnemyCharacterBase>(AimHit.GetActor()))
			{
				AimPoint = AimHit.ImpactPoint;
				break;
			}
		}
	}

	// 조준점 아래 지면. 조준점이 벽면이면 살짝 위에서, 허공이면 아래로 내려 바닥을 찾는다.
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(MagicFrostGround), false, Player);
	FHitResult Ground;
	if (!World->LineTraceSingleByChannel(Ground, AimPoint + FVector(0.f, 0.f, 50.f), AimPoint - FVector(0.f, 0.f, 1000.f), ECC_Visibility, GroundParams)
		|| Ground.bStartPenetrating)
	{
		UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 서리 지대: 조준점 아래 지면 없음  조준점=%s"), *AimPoint.ToString());
		return;
	}

	TargetPoint = Ground.ImpactPoint;
	bTargetValid = true;
}

bool UGA_MagicFrostField::CanRelease(const FGameplayAbilityActorInfo* Info)
{
	return bTargetValid;
}

void UGA_MagicFrostField::OnCastReleased(const FGameplayAbilityActorInfo* Info)
{
	AHBPlayerCharacter* Player = GetPlayerFrom(Info);
	UWorld* World = GetWorld();
	if (!Player || !World || !FieldClass || !bTargetValid || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	const float FinalRadius = GetAugmentParam(SkillParamTag::TAG_SkillParam_Radius, Radius);
	const float FinalSlow = GetAugmentParam(SkillParamTag::TAG_SkillParam_Slow, Slow);

	// Deferred: Init 로 반경·지속 시간을 정한 뒤 FinishSpawning 해야 첫 복제 번들에 실려 클라 BeginPlay 가 올바른 값을 읽는다.
	AHBFrostField* Field = World->SpawnActorDeferred<AHBFrostField>(FieldClass, FTransform(TargetPoint), Player, Player, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Field)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 서리 지대 스폰 실패"));
		return;
	}
	Field->Init(Player, FinalRadius, Duration, FinalSlow);
	Field->FinishSpawning(FTransform(TargetPoint));
	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 서리 지대 생성  위치=%s  반경=%.0f  감속=%.2f  시전ID=%d"), *TargetPoint.ToString(), FinalRadius, FinalSlow, CastId);

	// 지대가 생기는 소리. 마력 폭발 · 순간이동 · 구체 착탄처럼 큐로 낸다.
	// 지대 이펙트는 AHBFrostField 가 클라에서 직접 띄우므로 이 큐(BP_GCN_Magic_FrostField)에는 소리만 둔다.
	UAbilitySystemComponent* SourceASC = GetASC(Info);
	if (!SourceASC)
	{
		return;
	}
	FGameplayCueParameters CueParams;
	CueParams.Location = TargetPoint;
	CueParams.RawMagnitude = FinalRadius;
	CueParams.Instigator = Player;
	CueParams.EffectCauser = Player;
	SourceASC->ExecuteGameplayCue(MagicTag::TAG_GameplayCue_Magic_FrostField, CueParams);
}
