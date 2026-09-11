#include "ItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Net/UnrealNetwork.h"

AItemBase::AItemBase()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(100.f);
	// 필드에 미리 배치된 픽업 블루프린트(BP_PickupHelmet 등)의 InteractionSphere와 똑같은
	// 콜리전 프리셋으로 맞춤 — 커스텀 콜리전 채널을 쓰고 있어도 상관없이 모든 채널에 대해
	// 오버랩되는 프리셋이라 확실하게 작동함
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractionSphere->SetGenerateOverlapEvents(true);

	// 필드에 미리 배치된 아이템은 레벨 자체에 저장되어 있어서 서버/클라이언트가 각자 로드하지만,
	// 인벤토리에서 버려서 런타임에 스폰되는 아이템은 서버에서만 생성되므로
	// 반드시 복제(Replicate)를 켜줘야 클라이언트 화면에도 보이고 정상적으로 상호작용 가능함
	bReplicates = true;
	SetReplicateMovement(true);
}

void AItemBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeFromDataTable();
	SpawnTime = GetWorld()->GetTimeSeconds();
}

void AItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AItemBase, ItemID);
	DOREPLIFETIME(AItemBase, SourceTable);
	DOREPLIFETIME(AItemBase, ItemDataTable);
	DOREPLIFETIME(AItemBase, WeaponDataTable);
	DOREPLIFETIME(AItemBase, ShieldDataTable);
	DOREPLIFETIME(AItemBase, StackCount);
}

void AItemBase::OnRep_ItemID()
{
	// 드롭 아이템처럼 런타임에 스폰된 경우, 클라이언트에는 BeginPlay 시점에 ItemID/DataTable 값이
	// 아직 복제되어 도착하기 전이라 메쉬 설정이 실패했을 수 있음. 값이 실제로 도착하는
	// 이 시점에 다시 한번 초기화해줌.
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
		Icon = Row->Icon;
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
		Icon = Row->Icon;
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
		Icon = Row->Icon;
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

			// 실제 메쉬가 아직 없을 때 최소한 눈에는 보이도록 엔진 기본 도형(구)을 임시로 붙여줌
			static UStaticMesh* FallbackMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			if (FallbackMesh)
			{
				MeshComponent->SetStaticMesh(FallbackMesh);
				MeshComponent->SetWorldScale3D(FVector(0.3f));
			}
		}
	}
}

bool AItemBase::TryPickUp(AActor* Picker)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!Picker)
	{
		return false;
	}
	if (GetWorld()->GetTimeSeconds() - SpawnTime < PickupBlockDuration)
	{
		return false;
	}
	UInventoryComponent* Inventory = nullptr;
	if (const APawn* PickerPawn = Cast<APawn>(Picker))
	{
		if (AHB_PlayerState* HBPlayerState = PickerPawn->GetPlayerState<AHB_PlayerState>())
		{
			Inventory = HBPlayerState->GetInventory();
		}
	}
	if (!Inventory)
	{
		return false;
	}
	UDataTable* TargetTable = nullptr;
	switch (SourceTable)
	{
	case EItemSourceTable::Weapon:
		TargetTable = WeaponDataTable;
		break;
	case EItemSourceTable::Shield:
		TargetTable = ShieldDataTable;
		break;
	case EItemSourceTable::Item:
	default:
		TargetTable = ItemDataTable;
		break;
	}
	if (!TargetTable)
	{
		UE_LOG(LogTemp, Error, TEXT("TryPickUp: TargetTable is NULL (SourceTable=%d, ItemID=%s)"), (int32)SourceTable, *ItemID.ToString());
		return false;
	}
	FItemInstance NewItem;
	NewItem.ItemRow.DataTable = TargetTable;
	NewItem.ItemRow.RowName = ItemID;
	NewItem.InstanceId = FGuid::NewGuid();
	NewItem.StackCount = FMath::Max(1, StackCount);
	int32 Remainder = 0;
	const bool bAdded = Inventory->TryAddItem(NewItem, Remainder);
	if (bAdded && Remainder == 0)
	{
		Destroy();
		return true;
	}
	return false;
}

void AItemBase::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	// 더 이상 오버랩만으로 자동 습득하지 않음 - F키 상호작용으로만 습득 가능.
	// InteractionSphere는 여전히 "지금 오버랩 중인 아이템 조회"용으로 쓰임.
}

bool AItemBase::CanInteract_Implementation(AActor* Interactor) const
{
	UE_LOG(LogTemp, Warning, TEXT("[ItemBase] CanInteract called on %s"), *GetName());
	return true;
}

void AItemBase::Interact_Implementation(AActor* Interactor)
{
	// 인터페이스 규약상 Interact()는 서버에서만 호출됨 (InteractionComponent::Server_Interact_Implementation 경유)
	TryPickUp(Interactor);
}

FText AItemBase::GetInteractPrompt_Implementation() const
{
	return FText::FromString(FString::Printf(TEXT("%s"), *DisplayName));
}