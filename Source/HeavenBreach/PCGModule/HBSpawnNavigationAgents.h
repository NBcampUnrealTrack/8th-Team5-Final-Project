#pragma once

#include "CoreMinimal.h"
#include "AI/Navigation/NavigationTypes.h"

class ACharacter;
class ANavigationData;

namespace HBSpawnNavigationAgents
{
	/** Character CDOs have not received the runtime collision-size update yet. */
	HEAVENBREACH_API bool GetCharacterAgent(const ACharacter* Character, FNavAgentProperties& OutAgent, float SpawnScale = 1.f);
	/** GetNavDataForProps may fall back to a mesh built for a smaller agent. */
	HEAVENBREACH_API bool SupportsAgent(const ANavigationData* NavData, const FNavAgentProperties& Agent);
}
