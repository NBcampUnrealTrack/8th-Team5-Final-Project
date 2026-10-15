// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Attack/HBPlayerArrow.h"
#include "Engine/DataTable.h"

class AHBPlayerCharacter;

/**
 * 활 기본공격·활 스킬이 같이 쓰는 화살 함수.
 * 화살 개수 확인·소모·발사·"화살이 없습니다" 안내를 한곳에 둔다.
 */
namespace HBBowShot
{
	/** 가방에 화살이 1개 이상 있는지. 행이 비어 있으면 항상 true(무한) */
	HEAVENBREACH_API bool HasArrow(const AHBPlayerCharacter* Player, const FDataTableRowHandle& ArrowRow);

	/** 서버 전용. 화살 1개를 쓴다. 행이 비어 있으면 아무것도 안 쓰고 true */
	HEAVENBREACH_API bool ConsumeArrow(AHBPlayerCharacter* Player, const FDataTableRowHandle& ArrowRow);

	/**
	 * 서버 전용. 카메라 → ComputeAimPoint(AimDistance) 방향으로, 카메라 앞 ForwardOffset 에서 화살을 만들어 Launch 한다.
	 * YawOffsetDegrees 는 조준 방향을 좌우(월드 위쪽 축)로 돌린다(트리플 샷 부채꼴). 만들지 못하면 nullptr.
	 */
	HEAVENBREACH_API AHBPlayerArrow* SpawnArrow(AHBPlayerCharacter* Player, TSubclassOf<AHBPlayerArrow> ArrowClass, float Speed, float DamageCoef,
		const FDataTableRowHandle& ArrowRow, float AimDistance, float ForwardOffset, const FHBArrowLaunchOptions& Options = FHBArrowLaunchOptions(),
		float YawOffsetDegrees = 0.f);

	/** Count 발을 가운데 0 을 기준으로 좌우 Spread 간격으로 벌린 좌우 각도들. 3, 6 → {-6, 0, 6} */
	inline TArray<float> SpreadYawOffsets(int32 Count, float Spread)
	{
		TArray<float> Out;
		for (int32 i = 0; i < Count; ++i)
		{
			Out.Add((i - (Count - 1) * 0.5f) * Spread);
		}
		return Out;
	}

	/**
	 * "화살이 없습니다" 안내. 로컬 조종이면 바로 띄우고, 서버가 원격 클라 대신 판정한 경우엔
	 * 소유 클라에 RPC 로 보낸다(클라 예측과 서버 판정이 어긋났을 때).
	 */
	HEAVENBREACH_API void NotifyNoArrow(const AHBPlayerCharacter* Player, const FText& Message);

	/**
	 * 활 스킬의 화살 기본값 보정. 스킬에 화살 행이 비었거나 화살 클래스가 비었거나 맨 AHBPlayerArrow(메시 없음)이면
	 * 플레이어에게 부여된 활 평타 GA(UGA_PlayerBowAttack, BP CDO)의 값을 빌려 채운다.
	 * 스킬에 직접 지정한 값이 있으면 그것이 우선이다. 둘 다 행이 없으면 한 번만 경고를 남긴다(무한 화살로 동작).
	 */
	HEAVENBREACH_API void ResolveArrowDefaults(const AHBPlayerCharacter* Player, TSubclassOf<AHBPlayerArrow>& InOutClass, FDataTableRowHandle& InOutRow);

	/** 장착한 활의 스킬 몽타주 키(FHBWeaponPresentation::BowVariant). 외형 행이 없으면 NAME_None */
	HEAVENBREACH_API FName GetBowVariant(const AHBPlayerCharacter* Player);
}
