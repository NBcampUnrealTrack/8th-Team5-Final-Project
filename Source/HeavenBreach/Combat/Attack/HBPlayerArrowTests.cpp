#include "Combat/Attack/HBPlayerArrow.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 빈 월드에 Target 클래스 둘을 X=300·600 에 세우고 관통 화살을 X 방향으로 쏜다. 화살이 X=700 을 넘었는가.
	bool PiercesTwoInLine(FAutomationTestBase& Test, UClass* TargetClass)
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
			.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const AActor* First = World->SpawnActor<AActor>(TargetClass, FTransform(FVector(300.f, 0.f, 0.f)), Params);
		const AActor* Second = World->SpawnActor<AActor>(TargetClass, FTransform(FVector(600.f, 0.f, 0.f)), Params);
		Test.TestNotNull(TEXT("앞 대상"), First);
		Test.TestNotNull(TEXT("뒤 대상"), Second);

		AHBPlayerArrow* Arrow = World->SpawnActorDeferred<AHBPlayerArrow>(AHBPlayerArrow::StaticClass(), FTransform(FVector::ZeroVector),
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Arrow->FinishSpawning(FTransform(FVector::ZeroVector));
		FHBArrowLaunchOptions Options;
		Options.MaxPierceCount = 4;
		Arrow->Launch(nullptr, 4000.f, 1.f, FDataTableRowHandle(), Options);

		// 커맨드렛 월드는 액터 틱이 돌지 않아 이동 컴포넌트만 직접 틱한다.
		UProjectileMovementComponent* Movement = Arrow->FindComponentByClass<UProjectileMovementComponent>();
		Test.TestNotNull(TEXT("이동 컴포넌트"), Movement);
		for (int32 Frame = 0; Movement && Frame < 30 && IsValid(Arrow) && Movement->IsActive(); ++Frame)
		{
			Movement->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
		}

		bool bPassed = false;
		if (IsValid(Arrow))
		{
			const FVector Location = Arrow->GetActorLocation();
			Test.AddInfo(FString::Printf(TEXT("%s: 화살 위치 %s  부모 %s"), *GetNameSafe(TargetClass), *Location.ToCompactString(), *GetNameSafe(Arrow->GetAttachParentActor())));
			bPassed = Location.X > 700.f;
		}
		Test.TestTrue(*FString::Printf(TEXT("%s 둘을 뚫고 지나갔다"), *GetNameSafe(TargetClass)), bPassed);

		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return bPassed;
	}
}

// 관통 화살이 일렬로 선 대상 둘을 이동 처리만으로 뚫고 지나가는가. 피해·쏜 사람 없이 충돌 경로만 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBArrowPierceMovementTest,
	"HeavenBreach.Bow.ArrowPierceMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBArrowPierceMovementTest::RunTest(const FString&)
{
	PiercesTwoInLine(*this, ACharacter::StaticClass());

	// 실제 몬스터. 몬스터 BP 의 충돌 설정(캡슐·메시·추가 컴포넌트)에서도 지나가야 한다.
	for (const TCHAR* Path : {
		TEXT("/Game/Monsters/Remnant_Blade/Blueprint/BP_Remnant_Blade.BP_Remnant_Blade_C"),
		TEXT("/Game/Monsters/Remnant_Archor/Blueprint/BP_Remnant_Archor.BP_Remnant_Archor_C"),
		TEXT("/Game/Monsters/SteamWolf/Blueprints/BP_Enemy_SteamWolf.BP_Enemy_SteamWolf_C"),
		TEXT("/Game/Monsters/Monster/Blueprints/BP_TestEnemy.BP_TestEnemy_C") })
	{
		UClass* EnemyClass = LoadClass<AActor>(nullptr, Path);
		if (TestNotNull(*FString::Printf(TEXT("몬스터 클래스 %s"), Path), EnemyClass))
		{
			PiercesTwoInLine(*this, EnemyClass);
		}
	}
	return true;
}

#endif
