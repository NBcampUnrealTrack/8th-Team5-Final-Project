#include "Combat/Attack/HBArrowStuck.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBArrowResolveStuckKindTest, "HeavenBreach.Arrow.ResolveStuckKind", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBArrowResolveStuckKindTest::RunTest(const FString&)
{
	const ACharacter* Character = GetDefault<ACharacter>();
	const AStaticMeshActor* Wall = GetDefault<AStaticMeshActor>();

	TestEqual(TEXT("맞은 액터가 없으면 박히지 않음"),
		HBArrow::ResolveStuckKind(nullptr, Wall->GetStaticMeshComponent()), EHBArrowStuckKind::None);
	TestEqual(TEXT("맞은 컴포넌트가 없으면 박히지 않음"),
		HBArrow::ResolveStuckKind(Wall, nullptr), EHBArrowStuckKind::None);
	TestEqual(TEXT("Pawn(몬스터·플레이어·시체)은 Character"),
		HBArrow::ResolveStuckKind(Character, Character->GetMesh()), EHBArrowStuckKind::Character);
	TestEqual(TEXT("그 밖(벽·사물)은 World"),
		HBArrow::ResolveStuckKind(Wall, Wall->GetStaticMeshComponent()), EHBArrowStuckKind::World);

	return true;
}

#endif
