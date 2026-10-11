#include "PCGModule/HBSpawnNavigationAgents.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

bool HBSpawnNavigationAgents::GetCharacterAgent(const ACharacter* Character, FNavAgentProperties& OutAgent, float SpawnScale)
{
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Movement || !Capsule || !FMath::IsFinite(SpawnScale) || SpawnScale <= 0.f) return false;
	OutAgent = Movement->GetNavAgentPropertiesRef();
	if (Movement->ShouldUpdateNavAgentWithOwnersCollision())
	{
		OutAgent.AgentRadius = Capsule->GetScaledCapsuleRadius() * SpawnScale;
		OutAgent.AgentHeight = Capsule->GetScaledCapsuleHalfHeight() * 2.f * SpawnScale;
	}
	return OutAgent.IsValid() && FMath::IsFinite(OutAgent.AgentRadius) && FMath::IsFinite(OutAgent.AgentHeight);
}

bool HBSpawnNavigationAgents::SupportsAgent(const ANavigationData* NavData, const FNavAgentProperties& Agent)
{
	if (!NavData || !Agent.IsValid()) return false;
	const FNavDataConfig& Config = NavData->GetConfig();
	return Config.AgentRadius + KINDA_SMALL_NUMBER >= Agent.AgentRadius
		&& Config.AgentHeight + KINDA_SMALL_NUMBER >= Agent.AgentHeight;
}

const ANavigationData* HBSpawnNavigationAgents::FindCompatibleNavData(const UNavigationSystemV1* Nav,
	const FNavAgentProperties& Agent, const FVector& AgentLocation)
{
	if (!Nav || !Agent.IsValid() || !FMath::IsFinite(Agent.AgentRadius)
		|| !FMath::IsFinite(Agent.AgentHeight) || AgentLocation.ContainsNaN()) return nullptr;
	const ANavigationData* Preferred = Nav->GetNavDataForProps(Agent, AgentLocation);
	if (IsValid(Preferred) && SupportsAgent(Preferred, Agent)
		&& Preferred->GetConfig().IsNavDataMatching(Agent)) return Preferred;

	const ANavigationData* Best = nullptr;
	for (const ANavigationData* Candidate : Nav->NavDataSet)
	{
		if (!IsValid(Candidate) || !SupportsAgent(Candidate, Agent)
			|| !Candidate->GetConfig().IsNavDataMatching(Agent)) continue;
		const FNavDataConfig& Config = Candidate->GetConfig();
		if (!Best || Config.AgentRadius < Best->GetConfig().AgentRadius
			|| (Config.AgentRadius == Best->GetConfig().AgentRadius
				&& Config.AgentHeight < Best->GetConfig().AgentHeight)) Best = Candidate;
	}
	return Best;
}
