#include "Player/HBWeaponPresentation.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHBWeaponPresentationFindTest, "HeavenBreach.WeaponPresentation.Find", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHBWeaponPresentationFindTest::RunTest(const FString&)
{
	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FHBWeaponPresentation::StaticStruct();

	FHBWeaponPresentation ByType;
	ByType.StringIKOffset = 1.f;
	Table->AddRow(TEXT("Bow"), ByType);

	FHBWeaponPresentation ByRow;
	ByRow.StringIKOffset = 2.f;
	Table->AddRow(TEXT("BOW_002"), ByRow);

	const FHBWeaponPresentation* Exact = HBWeaponPresentation::Find(Table, TEXT("BOW_002"), TEXT("Bow"));
	TestTrue(TEXT("행 이름이 있으면 그 행"), Exact && Exact->StringIKOffset == 2.f);

	const FHBWeaponPresentation* Fallback = HBWeaponPresentation::Find(Table, TEXT("BOW_001"), TEXT("Bow"));
	TestTrue(TEXT("행 이름이 없으면 무기 종류 행"), Fallback && Fallback->StringIKOffset == 1.f);

	TestNull(TEXT("둘 다 없으면 없음"), HBWeaponPresentation::Find(Table, TEXT("SWD_001"), TEXT("Sword")));
	TestNull(TEXT("표가 없으면 없음"), HBWeaponPresentation::Find(nullptr, TEXT("BOW_002"), TEXT("Bow")));
	TestNull(TEXT("이름이 비면 없음"), HBWeaponPresentation::Find(Table, NAME_None, FString()));

	return true;
}

#endif
