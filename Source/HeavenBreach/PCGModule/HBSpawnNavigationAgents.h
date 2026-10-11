#pragma once

#include "CoreMinimal.h"
#include "AI/Navigation/NavigationTypes.h"

class ACharacter;
class ANavigationData;
class UNavigationSystemV1;

namespace HBSpawnNavigationAgents
{
	/** Character CDOs have not received the runtime collision-size update yet. */
	HEAVENBREACH_API bool GetCharacterAgent(const ACharacter* Character, FNavAgentProperties& OutAgent, float SpawnScale = 1.f);
	/** GetNavDataForProps may fall back to a mesh built for a smaller agent. */
	HEAVENBREACH_API bool SupportsAgent(const ANavigationData* NavData, const FNavAgentProperties& Agent);
	/** Keep a compatible engine choice; otherwise find registered data that fits both radius and height. */
	HEAVENBREACH_API const ANavigationData* FindCompatibleNavData(const UNavigationSystemV1* Nav,
		const FNavAgentProperties& Agent, const FVector& AgentLocation);
}
