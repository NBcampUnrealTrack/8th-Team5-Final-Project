// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HBArrowStuck.generated.h"

/** 화살이 어디에 박혔는지 */
UENUM()
enum class EHBArrowStuckKind : uint8
{
	/** 날아가는 중 */
	None,
	/** 벽·바닥·사물. F 로 회수 가능 */
	World,
	/** 몬스터·플레이어·시체. 회수 불가 */
	Character,
};

/** 박힘 상태 복제용. 종류와 대상을 한 번에 받아야 OnRep 에서 맞은 본인인지 판단할 수 있다 */
USTRUCT()
struct FHBArrowStuckState
{
	GENERATED_BODY()

	UPROPERTY()
	EHBArrowStuckKind Kind = EHBArrowStuckKind::None;

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;
};

namespace HBArrow
{
	/** 맞은 대상으로 박힘 종류를 정한다. Pawn 이면 Character, 그 밖은 World, 붙일 곳이 없으면 None */
	inline EHBArrowStuckKind ResolveStuckKind(const AActor* HitActor, const UPrimitiveComponent* HitComp)
	{
		if (!HitActor || !HitComp)
		{
			return EHBArrowStuckKind::None;
		}
		return HitActor->IsA<APawn>() ? EHBArrowStuckKind::Character : EHBArrowStuckKind::World;
	}
}
