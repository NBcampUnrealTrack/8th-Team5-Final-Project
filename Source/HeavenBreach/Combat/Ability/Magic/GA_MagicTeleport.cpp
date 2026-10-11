// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/GA_MagicTeleport.h"

#include "AbilitySystemComponent.h"
#include "Combat/Ability/Magic/HBMagic.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Components/CapsuleComponent.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Core/Tag/Magic/HB_MagicTag.h"
#include "Core/Tag/Skill/HB_SkillParamTag.h"
#include "Engine/World.h"
#include "Player/HBPlayerCharacter.h"

UGA_MagicTeleport::UGA_MagicTeleport()
{
	Cooldown = 8.f;
	ManaCost = 20.f;
	bUseAttackSpeed = false;
	RequiredWeaponTags.AddTag(ItemTag::TAG_Weapon_Staff);

	CostGameplayEffectClass = UGE_SkillManaCost::StaticClass();
	CooldownGameplayEffectClass = UGE_MagicTeleportCooldown::StaticClass();

	// 몽타주를 2배속으로 틀어 0.6 / 2.0 = 0.3초 뒤에 발동한다. 캐스팅 스킬이 아니라 감속·집중 배율이 없다.
	bIsCasting = false;
	ReleaseTime = 0.6f;
	MontagePlayRate = 2.f;
	CastHandFxIndex = 2;
	CastHandScale = 0.16f;

	// 단계 값은 그 단계의 최종값이다(누적 아님). 설명에 숫자를 적었으므로 수치를 바꾸면 같이 고칠 것.
	FHBSkillAugment& Far = Augments.AddDefaulted_GetRef();
	Far.AugmentTag = MagicTag::TAG_Augment_Magic_Teleport_Far;
	Far.DisplayName = NSLOCTEXT("MagicTeleport", "FarName", "순간 이동: 장거리");
	Far.Description = NSLOCTEXT("MagicTeleport", "FarDesc", "순간 이동 거리가 늘어납니다. (1단계 5m, 2단계 6m, 3단계 7m)");
	Far.Params.Add(SkillParamTag::TAG_SkillParam_Distance, 500.f);
	Far.TierMagnitudes = { 500.f, 600.f, 700.f };

	FHBSkillAugment& Rapid = Augments.AddDefaulted_GetRef();
	Rapid.AugmentTag = MagicTag::TAG_Augment_Magic_Teleport_Rapid;
	Rapid.DisplayName = NSLOCTEXT("MagicTeleport", "RapidName", "순간 이동: 재빠른");
	Rapid.Description = NSLOCTEXT("MagicTeleport", "RapidDesc", "순간 이동 재사용 대기시간이 줄어듭니다. (1단계 15%, 2단계 25%, 3단계 35%)");
	Rapid.Params.Add(SkillParamTag::TAG_SkillParam_Cooldown, 0.85f);
	Rapid.TierMagnitudes = { 0.85f, 0.75f, 0.65f };
}

bool UGA_MagicTeleport::CanRelease(const FGameplayAbilityActorInfo* Info)
{
	AHBPlayerCharacter* Player = GetPlayerFrom(Info);
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = Player ? Player->GetCapsuleComponent() : nullptr;
	if (!Player || !World || !Capsule)
	{
		return false;
	}

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MagicTeleport), false, Player);

	// 방향은 조준 Yaw 의 수평 성분. 위아래를 봐도 지면 위 수평으로만 이동한다.
	const FVector Dir = FRotator(0.f, Player->GetBaseAimRotation().Yaw, 0.f).Vector();
	const float Dist = GetAugmentParam(SkillParamTag::TAG_SkillParam_Distance, Distance);
	const FVector Start = Player->GetActorLocation();
	const float CurrentFeetZ = Start.Z - HalfHeight;

	// 왜 올려서 sweep 하나: 서 있는 캡슐(바닥 위 약 2cm)을 그대로 수평으로 밀면 오르막·계단 첫 칸에 바로 걸려
	// 몇 cm 만 가고 끝난다(MinMove 미달로 실패). 그래서 위로 MaxHeightDelta 만큼 올린 높이에서 수평 sweep 한다.
	// 그 높이면 허용 높이차(1m) 안의 경사·계단은 캡슐 아래로 지나가고, 그보다 높은 벽만 막는다.
	// 1) 위로 sweep. 천장이 낮으면 닿은 거리까지만 올린다(SweepSkin 만큼 떼어 다음 sweep 이 천장에 겹친 채 시작하지 않게).
	constexpr float SweepSkin = 1.f;
	FHitResult Hit;
	float RaiseHeight = MaxHeightDelta;
	if (World->SweepSingleByChannel(Hit, Start, Start + FVector(0.f, 0.f, MaxHeightDelta), FQuat::Identity, ECC_Pawn, Shape, Params))
	{
		if (Hit.bStartPenetrating)
		{
			return false; // 시작부터 겹쳐 있으면 안전하게 포기
		}
		RaiseHeight = FMath::Max(Hit.Distance - SweepSkin, 0.f);
	}
	const FVector RaisedStart = Start + FVector(0.f, 0.f, RaiseHeight);

	// 2) 올린 높이에서 수평 sweep. 막히면 막힌 지점에서 WallMargin 만큼 물러난다.
	FVector Dest = RaisedStart + Dir * Dist;
	if (World->SweepSingleByChannel(Hit, RaisedStart, Dest, FQuat::Identity, ECC_Pawn, Shape, Params))
	{
		if (Hit.bStartPenetrating)
		{
			return false;
		}
		Dest = RaisedStart + Dir * FMath::Max(Hit.Distance - WallMargin, 0.f);
	}

	// 3) 지면 찾기. 도착 캡슐 중심(sweep 이 비어 있다고 확인한 자리)에서 곧장 아래로,
	//    현재 발밑보다 MaxHeightDelta 아래 + 여유 50cm 까지. 아무것도 없으면 낭떠러지.
	const float TraceDepth = RaiseHeight + HalfHeight + MaxHeightDelta + 50.f;
	FHitResult Ground;
	if (!World->LineTraceSingleByChannel(Ground, Dest, Dest - FVector(0.f, 0.f, TraceDepth), ECC_Visibility, Params)
		|| Ground.bStartPenetrating)
	{
		return false;
	}

	// 법선·높이차(오르막·내리막 모두)·이동 거리 검사. 이동 거리는 수평만 본다(올려서 sweep 한 높이는 이동으로 치지 않는다).
	if (!HBMagic::IsTeleportDestinationValid(Ground.ImpactNormal.Z, Ground.ImpactPoint.Z - CurrentFeetZ, FVector::Dist2D(Start, Dest), MinGroundNormalZ, MaxHeightDelta, MinMove))
	{
		return false;
	}

	// 4) 캡슐 놓을 높이 = 지면 + HalfHeight + 띄움.
	//    경사(법선 Z = cos θ)에서는 아래 반구가 중심 바로 아래 지면점이 아니라 비탈 위쪽 면에 먼저 닿는다.
	//    반구 중심이 비탈면에서 Radius 만큼 떨어지려면 그 지면점보다 Radius / cos θ 위여야 하므로
	//    평지 기준보다 Radius × (1/cos θ − 1) 더 올리고, 여유 GroundLift(2cm)를 더한다(HBMagic::TeleportSlopeLift).
	//    예) 반지름 34cm, 30° 경사 → 34 × (1/0.866 − 1) ≈ 5.3cm + 2cm.
	const float Lift = HBMagic::TeleportSlopeLift(Radius, Ground.ImpactNormal.Z, GroundLift);
	const FVector Final = FVector(Dest.X, Dest.Y, Ground.ImpactPoint.Z + HalfHeight + Lift);

	// 5) 도착 자리에 벽·물체가 끼어 있지 않은가
	if (World->OverlapBlockingTestByChannel(Final, FQuat::Identity, ECC_Pawn, Shape, Params))
	{
		return false;
	}

	PendingDestination = Final;
	return true;
}

void UGA_MagicTeleport::OnCastReleased(const FGameplayAbilityActorInfo* Info)
{
	AHBPlayerCharacter* Player = GetPlayerFrom(Info);
	const UCapsuleComponent* Capsule = Player ? Player->GetCapsuleComponent() : nullptr;
	if (!Player || !Capsule || !Info || !Info->IsNetAuthority())
	{
		return;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector OldFeet = Player->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

	if (!Player->TeleportTo(PendingDestination, Player->GetActorRotation()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MagicSkill] 순간 이동 실패(TeleportTo)  목적지=%s"), *PendingDestination.ToString());
		return;
	}

	const FVector NewFeet = Player->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	if (UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent())
	{
		// 같은 큐를 두 번: AbilityLevel 0 = 출발, 1 = 도착
		FGameplayCueParameters Departure;
		Departure.Location = OldFeet;
		Departure.Instigator = Player;
		Departure.AbilityLevel = 0;
		ASC->ExecuteGameplayCue(MagicTag::TAG_GameplayCue_Magic_Teleport, Departure);

		FGameplayCueParameters Arrival;
		Arrival.Location = NewFeet;
		Arrival.Instigator = Player;
		Arrival.AbilityLevel = 1;
		ASC->ExecuteGameplayCue(MagicTag::TAG_GameplayCue_Magic_Teleport, Arrival);
	}
	UE_LOG(LogTemp, Log, TEXT("[MagicSkill] 순간 이동  %s -> %s"), *OldFeet.ToString(), *NewFeet.ToString());
}
