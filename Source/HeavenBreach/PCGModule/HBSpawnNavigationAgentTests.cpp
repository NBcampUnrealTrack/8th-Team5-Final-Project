#include "PCGModule/HBSpawnNavigationAgents.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSpawnNavigationAgentTest,
	"HeavenBreach.PCGModule.Navigation.CharacterAgent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBSpawnNavigationAgentTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	for (const TCHAR* Path : {
		TEXT("/Game/Monsters/Remnant_Archor/Blueprint/BP_Remnant_Archor.BP_Remnant_Archor_C"),
		TEXT("/Game/Monsters/Remnant_Blade/Blueprint/BP_Remnant_Blade.BP_Remnant_Blade_C"),
		TEXT("/Game/Monsters/WarpedDelver/Blueprint/BP_WarpedDelver.BP_WarpedDelver_C"),
		TEXT("/Game/Monsters/Sabertooth/Blueprints/BP_Enemy_SteamWolf.BP_Enemy_SteamWolf_C"),
		TEXT("/Game/Monsters/HeavenWarden/Blueprint/BP_Enemy_Warden.BP_Enemy_Warden_C"),
		TEXT("/Game/Player/BP_HBPlayerCharacter.BP_HBPlayerCharacter_C") })
	{
		UClass* Class = LoadClass<ACharacter>(nullptr, Path);
		if (!TestNotNull(Path, Class)) continue;
		const ACharacter* Defaults = Class->GetDefaultObject<ACharacter>();
		FNavAgentProperties Agent;
		if (!TestTrue(TEXT("Class defaults resolve an effective navigation agent"),
			HBSpawnNavigationAgents::GetCharacterAgent(Defaults, Agent))) continue;
		TestEqual(TEXT("Navigation radius uses the real class capsule"),
			Agent.AgentRadius, Defaults->GetCapsuleComponent()->GetScaledCapsuleRadius());
		TestEqual(TEXT("Navigation height uses the real class capsule"),
			Agent.AgentHeight, Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f);
		ACharacter* RuntimeCharacter = World->SpawnActor<ACharacter>(Class);
		if (TestNotNull(TEXT("Character instance initializes navigation properties"), RuntimeCharacter))
		{
			const FNavAgentProperties& RuntimeAgent = RuntimeCharacter->GetCharacterMovement()->GetNavAgentPropertiesRef();
			TestEqual(TEXT("Pre-spawn and initialized runtime radius agree"), Agent.AgentRadius, RuntimeAgent.AgentRadius);
			TestEqual(TEXT("Pre-spawn and initialized runtime height agree"), Agent.AgentHeight, RuntimeAgent.AgentHeight);
			RuntimeCharacter->Destroy();
		}
	}
	ACharacter* Explicit = World->SpawnActor<ACharacter>();
	Explicit->GetCharacterMovement()->SetUpdateNavAgentWithOwnersCollisions(false);
	Explicit->GetCharacterMovement()->NavAgentProps.AgentRadius = 80.f;
	Explicit->GetCharacterMovement()->NavAgentProps.AgentHeight = 300.f;
	FNavAgentProperties ExplicitAgent;
	TestTrue(TEXT("Explicit character navigation policy is supported"),
		HBSpawnNavigationAgents::GetCharacterAgent(Explicit, ExplicitAgent));
	TestEqual(TEXT("Collision-size opt-out preserves authored navigation radius"), ExplicitAgent.AgentRadius, 80.f);
	TestEqual(TEXT("Collision-size opt-out preserves authored navigation height"), ExplicitAgent.AgentHeight, 300.f);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
