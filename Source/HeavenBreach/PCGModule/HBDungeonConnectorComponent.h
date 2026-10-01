#pragma once
#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "HBDungeonConnectorComponent.generated.h"

UENUM(BlueprintType)
enum class EHBDungeonConnectorRole : uint8
{
	None,
	BossEntrance UMETA(DisplayName="Boss Entrance"),
	BossSide UMETA(DisplayName="Boss Side"),
	DungeonSide UMETA(DisplayName="Dungeon Side")
};

/** Symmetric allow-list. Unknown roles never match. */
inline bool HBConnectorRolesMatch(EHBDungeonConnectorRole A, EHBDungeonConnectorRole B)
{
	using R = EHBDungeonConnectorRole;
	return (A == R::None && (B == R::None || B == R::DungeonSide))
		|| (A == R::DungeonSide && B == R::None)
		|| (A == R::BossEntrance && B == R::BossSide)
		|| (A == R::BossSide && B == R::BossEntrance);
}

/** Floor-center connection marker. +X points outward, +Z up; unit scale. */
UCLASS(ClassGroup=(Dungeon), meta=(BlueprintSpawnableComponent), BlueprintType,
	HideCategories=(Collision, Physics, Lighting, Navigation, Rendering, HLOD, ArrowComponent))
class HEAVENBREACH_API UHBDungeonConnectorComponent : public UArrowComponent
{
	GENERATED_BODY()
public:
	UHBDungeonConnectorComponent();
	/** Optional explicit ID. Empty uses saved actor/component names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon|Connector")
	FName ConnectorId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon|Connector")
	EHBDungeonConnectorRole Role = EHBDungeonConnectorRole::None;
	FName GetEffectiveConnectorId() const;
};
