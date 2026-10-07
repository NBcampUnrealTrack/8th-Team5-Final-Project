#include "PCGModule/HBSpawnNavigationAgents.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationData.h"

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
