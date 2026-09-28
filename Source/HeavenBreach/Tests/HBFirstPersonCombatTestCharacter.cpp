#include "Tests/HBFirstPersonCombatTestCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryTypes.h"

#pragma region TemporaryFirstPersonCombatTest
// TODO: Replace this isolated adapter with reviewed production integration after the visual/trace audit.
AHBFirstPersonCombatTestCharacter::AHBFirstPersonCombatTestCharacter()
{
	bUseModularEquipment = false;
	bEnableEquipmentVisualTest = false;
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	TestFirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TestFirstPersonMesh"));
	TestFirstPersonMesh->SetupAttachment(GetMesh());
	TestFirstPersonMesh->SetOnlyOwnerSee(true);
	TestFirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	TestFirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestFirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	FirstPersonCamera->SetupAttachment(TestFirstPersonMesh, TEXT("head"));
	FirstPersonCamera->SetAbsolute(false, false, false);
	FirstPersonCamera->SetRelativeLocation(FVector(-2.8, 18.0, 0.0));
	FirstPersonCamera->SetRelativeRotation(FRotator(0.0, 90.0, -90.0));
	FirstPersonCamera->bEnableFirstPersonFieldOfView = true;
	FirstPersonCamera->bEnableFirstPersonScale = true;
	FirstPersonCamera->FirstPersonFieldOfView = 90.0f;
	FirstPersonCamera->FirstPersonScale = 0.6f;

	TestFirstPersonSword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TestFirstPersonSword"));
	TestFirstPersonSword->SetupAttachment(TestFirstPersonMesh, WeaponSocketName);
	TestFirstPersonShield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TestFirstPersonShield"));
	TestFirstPersonShield->SetupAttachment(TestFirstPersonMesh, ShieldSocketName);
	for (UStaticMeshComponent* Component : {TestFirstPersonSword.Get(), TestFirstPersonShield.Get()})
	{
		Component->SetOnlyOwnerSee(true);
		Component->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		// Never add the Weapon tag here: the authoritative trace must select the world weapon.
	}
	for (UStaticMeshComponent* Component : {WeaponMeshComp.Get(), ShieldMeshComp.Get()})
	{
		Component->SetOwnerNoSee(true);
		Component->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	}
}

void AHBFirstPersonCombatTestCharacter::BeginPlay()
{
	Super::BeginPlay();
	TestFirstPersonMesh->AddTickPrerequisiteComponent(GetMesh());
	for (int32 MaterialIndex = 1; MaterialIndex <= 6; ++MaterialIndex)
	{
		TestFirstPersonMesh->ShowMaterialSection(MaterialIndex, INDEX_NONE, false, 0);
	}
	BindTestEquipment();
	EquipTestLoadout();
}

void AHBFirstPersonCombatTestCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindTestEquipment();
	if (HasActorBegunPlay())
	{
		EquipTestLoadout();
	}
}

void AHBFirstPersonCombatTestCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindTestEquipment();
}

void AHBFirstPersonCombatTestCharacter::BindTestEquipment()
{
	if (UEquipmentComponent* Equipment = GetEquipment())
	{
		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::SyncTestEquipment);
		SyncTestEquipment();
	}
}

void AHBFirstPersonCombatTestCharacter::SyncTestEquipment()
{
	RefreshEquippedVisuals();
	TestFirstPersonSword->SetStaticMesh(WeaponMeshComp->GetStaticMesh());
	TestFirstPersonShield->SetStaticMesh(ShieldMeshComp->GetStaticMesh());
}

void AHBFirstPersonCombatTestCharacter::EquipTestLoadout()
{
	UInventoryComponent* Inventory = GetInventory();
	UEquipmentComponent* Equipment = GetEquipment();
	if (!HasAuthority() || bTestLoadoutGranted || !Inventory || !Equipment)
	{
		return;
	}
	const auto EquipRow = [Inventory, Equipment](const FDataTableRowHandle& Row, EEquipSlotType Slot)
	{
		if (Equipment->IsSlotOccupied(Slot)) return true;
		FItemViewData View;
		if (!ResolveItemViewData(Row, View)) return false;
		FItemInstance Item;
		Item.ItemRow = Row;
		Item.InstanceId = FGuid::NewGuid();
		Item.StackCount = 1;
		Item.CurrentDurability = GetMaxDurabilityForGrade(View.Grade);
		int32 Remainder = 0;
		return Inventory->TryAddItem(Item, Remainder) && Remainder == 0
			&& Equipment->EquipFromInventory(Inventory, Item.InstanceId, Slot);
	};
	const bool bSword = EquipRow(TestSwordRow, EEquipSlotType::PrimaryWeapon);
	const bool bShield = EquipRow(TestShieldRow, EEquipSlotType::SecondaryEquip);
	bTestLoadoutGranted = bSword && bShield;
	SyncTestEquipment();
	UE_LOG(LogTemp, Display, TEXT("[FirstPersonCombatTest] %s real equipment: sword=%d shield=%d"), *GetName(), bSword, bShield);
}
#pragma endregion
