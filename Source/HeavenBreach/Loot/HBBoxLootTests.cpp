#include "Loot/HBBoxLoot.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBoxLootCategoryTest, "HeavenBreach.Loot.Category", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBoxLootCategoryTest::RunTest(const FString&)
{
	// 일반 아이템 표의 ItemType 문자열 → 상자 보상 종류. 대소문자는 가리지 않는다(DT 에 "key" 소문자가 있다).
	TestEqual(TEXT("투구는 장비"), HBBoxLoot::CategoryOfItemType(TEXT("Helmet")), EBoxLootCategory::Equipment);
	TestEqual(TEXT("상의는 장비"), HBBoxLoot::CategoryOfItemType(TEXT("Chest")), EBoxLootCategory::Equipment);
	TestEqual(TEXT("하의는 장비"), HBBoxLoot::CategoryOfItemType(TEXT("Legs")), EBoxLootCategory::Equipment);
	TestEqual(TEXT("장신구는 장비"), HBBoxLoot::CategoryOfItemType(TEXT("Accessory")), EBoxLootCategory::Equipment);
	TestEqual(TEXT("가방은 장비"), HBBoxLoot::CategoryOfItemType(TEXT("Bag")), EBoxLootCategory::Equipment);
	TestEqual(TEXT("횃불은 장비"), HBBoxLoot::CategoryOfItemType(TEXT("Torch")), EBoxLootCategory::Equipment);
	TestEqual(TEXT("열쇠(소문자)"), HBBoxLoot::CategoryOfItemType(TEXT("key")), EBoxLootCategory::Key);
	TestEqual(TEXT("열쇠(대문자)"), HBBoxLoot::CategoryOfItemType(TEXT("Key")), EBoxLootCategory::Key);
	TestEqual(TEXT("소모품"), HBBoxLoot::CategoryOfItemType(TEXT("Consumable")), EBoxLootCategory::Consumable);
	TestEqual(TEXT("재료는 기타"), HBBoxLoot::CategoryOfItemType(TEXT("Material")), EBoxLootCategory::Other);
	TestEqual(TEXT("잡템은 기타"), HBBoxLoot::CategoryOfItemType(TEXT("Junk")), EBoxLootCategory::Other);
	TestEqual(TEXT("설계도는 기타"), HBBoxLoot::CategoryOfItemType(TEXT("Blueprint")), EBoxLootCategory::Other);
	TestEqual(TEXT("모르는 종류는 기타"), HBBoxLoot::CategoryOfItemType(TEXT("Something")), EBoxLootCategory::Other);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBBoxLootWeightsTest, "HeavenBreach.Loot.Weights", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBBoxLootWeightsTest::RunTest(const FString&)
{
	// 기본 비율: 장비 40 / 열쇠 15 / 소모품 20 / 기타 25.
	const TMap<EBoxLootCategory, float> Empty;
	const TMap<EBoxLootCategory, float> Defaults = HBBoxLoot::ResolveCategoryWeights(Empty);
	TestEqual(TEXT("장비 40"), Defaults.FindRef(EBoxLootCategory::Equipment), 40.f);
	TestEqual(TEXT("열쇠 15"), Defaults.FindRef(EBoxLootCategory::Key), 15.f);
	TestEqual(TEXT("소모품 20"), Defaults.FindRef(EBoxLootCategory::Consumable), 20.f);
	TestEqual(TEXT("기타 25"), Defaults.FindRef(EBoxLootCategory::Other), 25.f);

	// 표에 값이 있으면 그 값을 쓴다(기본과 섞지 않는다).
	TMap<EBoxLootCategory, float> Custom;
	Custom.Add(EBoxLootCategory::Key, 100.f);
	const TMap<EBoxLootCategory, float> Resolved = HBBoxLoot::ResolveCategoryWeights(Custom);
	TestEqual(TEXT("직접 지정만"), Resolved.Num(), 1);
	TestEqual(TEXT("열쇠 100"), Resolved.FindRef(EBoxLootCategory::Key), 100.f);

	// 가중치 뽑기: Roll 0~1 을 누적 비율로 나눈다. 0 이하 가중치는 안 뽑힌다.
	const TArray<float> W = { 40.f, 0.f, 20.f, 40.f };
	TestEqual(TEXT("처음"), HBBoxLoot::PickWeightedIndex(W, 0.f), 0);
	TestEqual(TEXT("첫 구간 끝"), HBBoxLoot::PickWeightedIndex(W, 0.39f), 0);
	TestEqual(TEXT("0 가중치 건너뜀"), HBBoxLoot::PickWeightedIndex(W, 0.41f), 2);
	TestEqual(TEXT("마지막"), HBBoxLoot::PickWeightedIndex(W, 0.99f), 3);
	TestEqual(TEXT("1 이상은 마지막 유효"), HBBoxLoot::PickWeightedIndex(W, 1.f), 3);
	TestEqual(TEXT("전부 0 이면 -1"), HBBoxLoot::PickWeightedIndex({ 0.f, 0.f }, 0.5f), INDEX_NONE);
	TestEqual(TEXT("비어 있으면 -1"), HBBoxLoot::PickWeightedIndex({}, 0.5f), INDEX_NONE);
	return true;
}

#endif
