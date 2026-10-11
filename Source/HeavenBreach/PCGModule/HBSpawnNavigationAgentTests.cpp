#include "PCGModule/HBSpawnNavigationAgents.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBSpawnCompatibleNavigationDataTest,
	"HeavenBreach.PCGModule.Navigation.CompatibleNavData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBSpawnCompatibleNavigationDataTest::RunTest(const FString&)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UNavigationSystemV1* Nav = NewObject<UNavigationSystemV1>(World);
	World->SetNavigationSystem(Nav);
	ARecastNavMesh* Tall = nullptr;
	TArray<ARecastNavMesh*> Meshes;
	for (const FNavDataConfig& Config : Nav->GetSupportedAgents())
	{
		ARecastNavMesh* Mesh = World->SpawnActor<ARecastNavMesh>();
		if (!TestNotNull(TEXT("Test navigation data is created"), Mesh)) continue;
		Mesh->SetConfig(Config);
		Nav->RequestRegistrationDeferred(*Mesh);
		Meshes.Add(Mesh);
		if (Config.Name == TEXT("HB_Tall")) Tall = Mesh;
	}
	Nav->Tick(0.f);
	for (const ARecastNavMesh* Mesh : Meshes)
		TestTrue(TEXT("Project navigation profiles register successfully"), Mesh->IsRegistered());
	UClass* BossClass = LoadClass<ACharacter>(nullptr,
		TEXT("/Game/Monsters/ParagonSevarog/Blueprint/BP_Sevarog.BP_Sevarog_C"));
	FNavAgentProperties BossAgent;
	if (TestNotNull(TEXT("The dungeon boss class is available"), BossClass)
		&& TestTrue(TEXT("The boss uses its actual capsule dimensions"),
			HBSpawnNavigationAgents::GetCharacterAgent(BossClass->GetDefaultObject<ACharacter>(), BossAgent)))
	{
		TestEqual(TEXT("Boss capsule radius"), BossAgent.AgentRadius, 35.f);
		TestEqual(TEXT("Boss capsule height"), BossAgent.AgentHeight, 300.f);
		if (TestNotNull(TEXT("A tall navigation profile is registered"), Tall))
		{
			TestTrue(TEXT("Boss selects the tall mesh, even when the engine picks a shorter mesh"),
				HBSpawnNavigationAgents::FindCompatibleNavData(Nav, BossAgent, FVector::ZeroVector) == Tall);
			Nav->UnregisterNavData(Tall);
			TestNull(TEXT("Boss is rejected when no registered mesh supports its full height"),
				HBSpawnNavigationAgents::FindCompatibleNavData(Nav, BossAgent, FVector::ZeroVector));
		}
	}
	for (const FNavAgentProperties Agent : {FNavAgentProperties(34.f, 176.f),
		FNavAgentProperties(80.f, 180.f), FNavAgentProperties(68.f, 153.f)})
	{
		const ANavigationData* EngineChoice = Nav->GetNavDataForProps(Agent, FVector::ZeroVector);
		TestTrue(TEXT("Existing small and wolf meshes support their agents"),
			HBSpawnNavigationAgents::SupportsAgent(EngineChoice, Agent));
		TestTrue(TEXT("Compatible engine choices remain unchanged"),
			HBSpawnNavigationAgents::FindCompatibleNavData(Nav, Agent, FVector::ZeroVector) == EngineChoice);
	}
	TestNull(TEXT("Missing navigation system fails safely"),
		HBSpawnNavigationAgents::FindCompatibleNavData(nullptr, BossAgent, FVector::ZeroVector));
	TestNull(TEXT("An invalid agent fails safely"),
		HBSpawnNavigationAgents::FindCompatibleNavData(Nav, FNavAgentProperties(), FVector::ZeroVector));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
