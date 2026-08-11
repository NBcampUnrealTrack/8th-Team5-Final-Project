#include "ItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

AItemBase::AItemBase()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(100.f);
}

void AItemBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeFromDataTable();
}

void AItemBase::InitializeFromDataTable()
{
	static const FString ContextString(TEXT("AItemBase::InitializeFromDataTable"));
	FString MeshPathToLoad;

	switch (SourceTable)
	{
	case EItemSourceTable::Weapon:
	{
		if (!WeaponDataTable)
		{
			UE_LOG(LogTemp, Warning, TEXT("AItemBase: WeaponDataTable is null (%s)"), *ItemID.ToString());
			return;
		}
		FWeaponData* Row = WeaponDataTable->FindRow<FWeaponData>(ItemID, ContextString);
		if (!Row)
		{
			UE_LOG(LogTemp, Warning, TEXT("AItemBase: Could not find '%s' in WeaponDataTable"), *ItemID.ToString());
			return;
		}
		DisplayName = Row->WeaponName;
		Grade = Row->Grade;
		EquipSlotType = Row->EquipSlotType;
		SellValue = Row->SellValue;
		GridWidth = Row->GridWidth;
		GridHeight = Row->GridHeight;
		IconPath = Row->IconPath;
		MeshPathToLoad = Row->EquipMeshPath;
		break;
	}
	case EItemSourceTable::Shield:
	{
		if (!ShieldDataTable)
		{
			UE_LOG(LogTemp, Warning, TEXT("AItemBase: ShieldDataTable is null (%s)"), *ItemID.ToString());
			return;
		}
		FShieldData* Row = ShieldDataTable->FindRow<FShieldData>(ItemID, ContextString);
		if (!Row)
		{
			UE_LOG(LogTemp, Warning, TEXT("AItemBase: Could not find '%s' in ShieldDataTable"), *ItemID.ToString());
			return;
		}
		DisplayName = Row->ShieldName;
		Grade = Row->Grade;
		EquipSlotType = Row->EquipSlotType;
		SellValue = Row->SellValue;
		GridWidth = Row->GridWidth;
		GridHeight = Row->GridHeight;
		IconPath = Row->IconPath;
		MeshPathToLoad = Row->EquipMeshPath;
		break;
	}
	case EItemSourceTable::Item:
	default:
	{
		if (!ItemDataTable)
		{
			UE_LOG(LogTemp, Warning, TEXT("AItemBase: ItemDataTable is null (%s)"), *ItemID.ToString());
			return;
		}
		FItemData* Row = ItemDataTable->FindRow<FItemData>(ItemID, ContextString);
		if (!Row)
		{
			UE_LOG(LogTemp, Warning, TEXT("AItemBase: Could not find '%s' in ItemDataTable"), *ItemID.ToString());
			return;
		}
		DisplayName = Row->ItemName;
		Grade = Row->Grade;
		EquipSlotType = Row->EquipSlotType;
		SellValue = Row->SellValue;
		GridWidth = Row->GridWidth;
		GridHeight = Row->GridHeight;
		IconPath = Row->IconPath;
		MeshPathToLoad = Row->WorldMeshPath;
		break;
	}
	}

	if (!MeshPathToLoad.IsEmpty())
	{
		UStaticMesh* LoadedMesh = LoadObject<UStaticMesh>(nullptr, *MeshPathToLoad);
		if (LoadedMesh)
		{
			MeshComponent->SetStaticMesh(LoadedMesh);
		}
		else
		{
			// Mesh paths are still temp placeholders ("/Game/Temp/..."), so a load failure here is expected for now.
			UE_LOG(LogTemp, Warning, TEXT("AItemBase: Failed to load mesh path '%s' (expected if still a temp path)"), *MeshPathToLoad);
		}
	}
}