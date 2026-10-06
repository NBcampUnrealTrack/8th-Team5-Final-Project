#include "ItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Equipment/EquipmentComponent.h"
#include "Net/UnrealNetwork.h"

static UInventoryComponent* PickupResolveBagInventory(UEquipmentComponent* Equipment)
{
	if (Equipment != nullptr && Equipment->HasBagEquipped())
	{
		return Equipment->GetBagInventory();
	}
	return nullptr;
}

static bool PickupIsAutoEquipSlot(EEquipSlotType InSlot)
{
	switch (InSlot)
	{
	case EEquipSlotType::PrimaryWeapon:
	case EEquipSlotType::SecondaryEquip:
	case EEquipSlotType::Accessory:
	case EEquipSlotType::Helmet:
	case EEquipSlotType::Chest:
	case EEquipSlotType::Legs:
	case EEquipSlotType::Backpack:
		return true;
	default:
		return false;
	}
}

static bool PickupTryAutoEquip(UInventoryComponent* Inventory, UEquipmentComponent* Equipment, const FItemInstance& InItem, EEquipSlotType InSlot)
{
	if (Inventory == nullptr || Equipment == nullptr || !PickupIsAutoEquipSlot(InSlot) || Equipment->IsSlotOccupied(InSlot))
	{
		return false;
	}
	FItemInstance ToEquip = InItem;
	Inventory->EnsureEngraved(ToEquip);
	return Equipment->EquipDirect(ToEquip, InSlot);
}

AItemBase::AItemBase()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(100.f);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractionSphere->SetGenerateOverlapEvents(true);

	// 콜리전 + 물리 시뮬레이션 담당. 이게 꺼져있으면 드롭된 아이템이 중력을 안 받아서 허공에 박제된다.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
	// 플레이어와 부딪혀도 튕겨나가거나 회전하지 않도록 Pawn 채널만 오버랩으로 낮춘다
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetEnableGravity(true);
	MeshComponent->SetLinearDamping(0.5f);
	MeshComponent->SetAngularDamping(5.0f);
	MeshComponent->BodyInstance.bOverrideMaxAngularVelocity = true;
	MeshComponent->BodyInstance.MaxAngularVelocity = 200.f;

	// 갑옷/투구처럼 실제 에셋이 스켈레탈 메쉬인 장비 표시 전용. 콜리전/물리는 절대 안 가지고
	// MeshComponent(루트)를 그냥 따라다니기만 한다 - 물리 애셋이 따로 요동쳐서 메쉬가 휘는 걸 막기 위함.
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(RootComponent);
	// MeshComponent(콜리전 구체)는 판정용으로 계속 스케일이 바뀌는데, 자식으로 붙어있으면
	// 그 스케일을 그대로 곱해서 상속받아버림 - 실제 보이는 메쉬 크기는 거기 영향 안 받게
	// 절대 스케일로 고정한다.
	SkeletalMeshComponent->SetUsingAbsoluteScale(true);
	SkeletalMeshComponent->SetRelativeScale3D(FVector::OneVector);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshComponent->SetSimulatePhysics(false);
	SkeletalMeshComponent->SetVisibility(false);
	MeshComponent->TransformUpdated.AddUObject(this, &AItemBase::OnMeshTransformUpdated);

	bReplicates = true;
	SetReplicateMovement(true);
}

void AItemBase::BeginPlay()
{
	Super::BeginPlay();

	// 레벨에 미리 배치된 아이템은 에디터에서 Mobility가 Static으로 저장된 경우가 많아서,
	// SimulatePhysics(true)를 줘도 무시되고 배치된 그 자리에 고정된 채로 남는다. 강제로 Movable로 바꿔준다.
	MeshComponent->SetMobility(EComponentMobility::Movable);

	// 메쉬/스케일부터 확정한 다음 물리를 켠다. 반대로 하면(물리가 이미 켜진 채로
	// 콜리전 형태가 바뀌면) 그 프레임에 지오메트리와 겹치면서 강한 밀어내기 힘이
	// 걸려 순간적으로 멀리 튕겨나갈 수 있다.
	if (HasAuthority())
	{
		RollPickupContents();
	}
	InitializeFromDataTable();
	MeshComponent->SetSimulatePhysics(true);

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
	DOREPLIFETIME(AItemBase, bIsFavorite);
	DOREPLIFETIME(AItemBase, CurrentDurability);
	DOREPLIFETIME(AItemBase, Engraving);
	DOREPLIFETIME(AItemBase, BundleItems);
}

void AItemBase::OnRep_ItemID()
{
	// 드롭 아이템처럼 런타임에 스폰된 경우, 클라이언트에는 BeginPlay 시점에 ItemID/DataTable 값이
	// 아직 복제되어 도착하기 전이라 메쉬 설정이 실패했을 수 있음. 값이 실제로 도착하는
	// 이 시점에 다시 한번 초기화해줌.
	InitializeFromDataTable();
}

static float GetItemCategoryScale(const FString& ItemType)
{
	static const TMap<FString, float> CategoryScales = {
		{TEXT("Junk"), 0.16f},
		{TEXT("Material"), 0.18f},
		{TEXT("Relic"), 0.5f},
		{TEXT("Accessory"), 0.1f},
		{TEXT("Key"), 0.14f},
		{TEXT("Currency"), 0.15f},
		{TEXT("Torch"), 0.5f},
	};
	if (const float* Found = CategoryScales.Find(ItemType))
	{
		return *Found;
	}
	return 1.f;
}

void AItemBase::InitializeFromDataTable()
{
	static const FString ContextString(TEXT("AItemBase::InitializeFromDataTable"));

	FString MeshPathToLoad;
	FString WorldScaleItemType;
	float WorldScaleMultiplier = 1.f;

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
		WorldScaleItemType = Row->ItemType;
		WorldScaleMultiplier = Row->WorldScaleMultiplier;
		break;
	}
	}

	if (!MeshPathToLoad.IsEmpty())
	{
		// 1순위: 스태틱 메쉬로 시도 (잡템/무기/방패 대부분 이 경우)
		if (UStaticMesh* LoadedStaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPathToLoad))
		{
			MeshComponent->SetStaticMesh(LoadedStaticMesh);
			MeshComponent->SetWorldScale3D(FVector(GetItemCategoryScale(WorldScaleItemType) * WorldScaleMultiplier));
			MeshComponent->SetVisibility(true);
			SkeletalMeshComponent->SetSkeletalMesh(nullptr);
			SkeletalMeshComponent->SetVisibility(false);
			return;
		}

		// 2순위: 스켈레탈 메쉬로 시도 (갑옷/투구처럼 본이 있는 장비)
		if (USkeletalMesh* LoadedSkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPathToLoad))
		{
			const FBoxSphereBounds RefPoseBounds = LoadedSkeletalMesh->GetBounds();

			// F키 상호작용 판정/물리 콜리전을 담당하는 프록시. 예전엔 구 형태였는데,
			// 구는 회전 안정성이 없어서 바닥에 닿아도 계속 굴러다니려는 경향이 있고
			// 그 회전을 스켈레탈 메쉬가 그대로 따라가다보니 투구는 몰라도 바지처럼
			// 길쭉한 파츠는 데굴데굴 도는 것처럼 보였다. 박스는 한 면으로 안착해서
			// 모양이 둥글든 길쭉하든 훨씬 자연스럽게 멈춘다.
			static UStaticMesh* CollisionProxyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (CollisionProxyMesh)
			{
				MeshComponent->SetStaticMesh(CollisionProxyMesh);

				// 엔진 기본 Cube는 한 변이 100(반지름 50) 기준이라, 메쉬 바운드의 각 축
				// 반지름을 50으로 나눠서 스케일로 변환. 너무 작거나(조준 빗나감) 커지지
				// (판정이 실제 메쉬보다 과하게 넓어짐) 않게 클램프.
				const FVector ClampedExtent(
					FMath::Clamp(RefPoseBounds.BoxExtent.X, 15.f, 80.f),
					FMath::Clamp(RefPoseBounds.BoxExtent.Y, 15.f, 80.f),
					FMath::Clamp(RefPoseBounds.BoxExtent.Z, 15.f, 80.f));
				MeshComponent->SetWorldScale3D(ClampedExtent / 50.f);
			}
			MeshComponent->SetVisibility(false);

			SkeletalMeshComponent->SetSkeletalMesh(LoadedSkeletalMesh);
			SkeletalMeshComponent->SetPhysicsAsset(nullptr);           // 메쉬에 딸린 기본 피직스 애셋을 런타임에 다시 참조 못 하게 명시적으로 차단
			SkeletalMeshComponent->SetAllBodiesSimulatePhysics(false); // 컴포넌트 플래그가 아니라 개별 바디 단위로도 강제로 끔
			SkeletalMeshComponent->SetVisibility(true);
			SkeletalMeshLocalOffset = -RefPoseBounds.Origin;
			SkeletalMeshComponent->SetRelativeLocation(SkeletalMeshLocalOffset);

			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("AItemBase: Failed to load mesh path '%s' as StaticMesh or SkeletalMesh"), *MeshPathToLoad);
	}

	// 경로가 비어있거나(임시 경로 미지정) 로드 자체가 실패한 경우 -
	// 최소한 눈에는 보이도록 엔진 기본 도형(구)을 임시로 붙여줌
	static UStaticMesh* FallbackMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (FallbackMesh)
	{
		MeshComponent->SetStaticMesh(FallbackMesh);
		MeshComponent->SetVisibility(true);
		MeshComponent->SetWorldScale3D(FVector(0.3f));
	}
	SkeletalMeshComponent->SetVisibility(false);
}

void AItemBase::OnMeshTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	if (SkeletalMeshComponent && MeshComponent)
	{
		const FVector RotatedOffset = MeshComponent->GetComponentRotation().RotateVector(SkeletalMeshLocalOffset);

		SkeletalMeshComponent->SetWorldLocationAndRotation(
			MeshComponent->GetComponentLocation() + RotatedOffset,
			MeshComponent->GetComponentRotation());
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
	UEquipmentComponent* Equipment = nullptr;
	if (const APawn* PickerPawn = Cast<APawn>(Picker))
	{
		if (AHB_PlayerState* HBPlayerState = PickerPawn->GetPlayerState<AHB_PlayerState>())
		{
			Inventory = HBPlayerState->GetInventory();
			Equipment = HBPlayerState->GetEquipment();
		}
	}
	if (!Inventory)
	{
		return false;
	}
	if ((ItemID.IsNone() || bMainItemClaimed) && BundleItems.IsEmpty())
	{
		return false;
	}
	if (!ItemID.IsNone() && !bMainItemClaimed)
	{
		UDataTable* TargetTable = GetDataTableFor(SourceTable);
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
		NewItem.bIsFavorite = bIsFavorite;
		NewItem.CurrentDurability = CurrentDurability;
		// 버릴 때 실어 둔 각인을 되살린다.
		// 레벨에 미리 놓인 아이템은 비어 있어서(bEngraved false) 아래 TryAddItem 입구에서 새로 굴린다.
		NewItem.Engraving = Engraving;
		NewItem.BagContents = BagContents;
		int32 Remainder = 0;
		const bool bAdded = AddWithPickupFlow(Inventory, Equipment, NewItem, EquipSlotType, Remainder);
		if (bAdded && Remainder == 0)
		{
			bMainItemClaimed = true;
		}
		else if (Remainder > 0 && Remainder < NewItem.StackCount)
		{
			StackCount = Remainder;
		}
	}

	int32 BundleIndex = 0;
	while (BundleIndex < BundleItems.Num())
	{
		if (GrantBundleEntry(Inventory, Equipment, BundleItems[BundleIndex]))
		{
			BundleItems.RemoveAt(BundleIndex);
		}
		else
		{
			++BundleIndex;
		}
	}

	if ((ItemID.IsNone() || bMainItemClaimed) && BundleItems.IsEmpty())
	{
		Destroy();
		return true;
	}
	return false;
}

UDataTable* AItemBase::GetDataTableFor(EItemSourceTable InSource) const
{
	switch (InSource)
	{
	case EItemSourceTable::Weapon:
		return WeaponDataTable;
	case EItemSourceTable::Shield:
		return ShieldDataTable;
	case EItemSourceTable::Item:
	default:
		return ItemDataTable;
	}
}

const FItemPickupEntry* AItemBase::PickWeighted(const TArray<FItemPickupEntry>& InEntries)
{
	float TotalWeight = 0.f;
	for (const FItemPickupEntry& Entry : InEntries)
	{
		if (!Entry.ItemID.IsNone() && Entry.Weight > 0.f)
		{
			TotalWeight += Entry.Weight;
		}
	}
	if (TotalWeight <= 0.f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	const FItemPickupEntry* LastValid = nullptr;
	for (const FItemPickupEntry& Entry : InEntries)
	{
		if (Entry.ItemID.IsNone() || Entry.Weight <= 0.f)
		{
			continue;
		}
		LastValid = &Entry;
		if (Roll < Entry.Weight)
		{
			return &Entry;
		}
		Roll -= Entry.Weight;
	}
	return LastValid;
}

int32 AItemBase::RollCount(const FItemPickupEntry& InEntry)
{
	const int32 Min = FMath::Max(1, InEntry.MinCount);
	const int32 Max = FMath::Max(Min, InEntry.MaxCount);
	return FMath::RandRange(Min, Max);
}

void AItemBase::RollPickupContents()
{
	for (FItemPickupEntry& Entry : BundleItems)
	{
		const int32 Count = RollCount(Entry);
		Entry.MinCount = Count;
		Entry.MaxCount = Count;
	}

	if (RandomCandidates.IsEmpty())
	{
		return;
	}

	if (bRandomizeItemID)
	{
		if (const FItemPickupEntry* Picked = PickWeighted(RandomCandidates))
		{
			ItemID = Picked->ItemID;
			SourceTable = Picked->SourceTable;
			StackCount = RollCount(*Picked);

			FDataTableRowHandle PickedRow;
			PickedRow.DataTable = GetDataTableFor(SourceTable);
			PickedRow.RowName = ItemID;
			FItemViewData ViewData;
			if (ResolveItemViewData(PickedRow, ViewData) && !ViewData.IsStackable())
			{
				StackCount = 1;
			}
		}
	}

	for (int32 Roll = 0; Roll < RandomBundleRolls; ++Roll)
	{
		if (const FItemPickupEntry* Picked = PickWeighted(RandomCandidates))
		{
			FItemPickupEntry& Added = BundleItems.Add_GetRef(*Picked);
			const int32 Count = RollCount(*Picked);
			Added.MinCount = Count;
			Added.MaxCount = Count;
		}
	}

	RandomCandidates.Reset();
}

bool AItemBase::AddWithPickupFlow(UInventoryComponent* Inventory, UEquipmentComponent* Equipment, const FItemInstance& InItem, EEquipSlotType InSlot, int32& OutRemainder)
{
	Inventory->TryAddItem(InItem, OutRemainder);
	if (OutRemainder == 0)
	{
		return true;
	}
	if (OutRemainder == InItem.StackCount && PickupTryAutoEquip(Inventory, Equipment, InItem, InSlot))
	{
		OutRemainder = 0;
		return true;
	}
	UInventoryComponent* BagInventory = PickupResolveBagInventory(Equipment);
	if (BagInventory != nullptr && BagInventory != Inventory && BagInventory->AcceptsItem(InItem))
	{
		FItemInstance Rest = InItem;
		if (OutRemainder < InItem.StackCount)
		{
			Rest.InstanceId = FGuid::NewGuid();
		}
		Rest.StackCount = OutRemainder;
		BagInventory->TryAddItem(Rest, OutRemainder);
	}
	return OutRemainder == 0;
}

bool AItemBase::GrantBundleEntry(UInventoryComponent* Inventory, UEquipmentComponent* Equipment, FItemPickupEntry& InOutEntry)
{
	UDataTable* TargetTable = GetDataTableFor(InOutEntry.SourceTable);
	if (!TargetTable || InOutEntry.ItemID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("TryPickUp: invalid bundle entry '%s'"), *InOutEntry.ItemID.ToString());
		return true;
	}

	FItemInstance Template;
	Template.ItemRow.DataTable = TargetTable;
	Template.ItemRow.RowName = InOutEntry.ItemID;

	FItemViewData ViewData;
	if (!ResolveItemViewData(Template.ItemRow, ViewData))
	{
		UE_LOG(LogTemp, Warning, TEXT("TryPickUp: could not resolve bundle entry '%s'"), *InOutEntry.ItemID.ToString());
		return true;
	}

	int32 Remaining = FMath::Max(1, InOutEntry.MinCount);
	const int32 BatchSize = ViewData.IsStackable() ? Remaining : 1;
	while (Remaining > 0)
	{
		FItemInstance NewItem = Template;
		NewItem.InstanceId = FGuid::NewGuid();
		NewItem.StackCount = FMath::Min(BatchSize, Remaining);
		int32 Remainder = 0;
		AddWithPickupFlow(Inventory, Equipment, NewItem, ViewData.EquipSlotType, Remainder);
		Remaining -= NewItem.StackCount - Remainder;
		if (Remainder > 0)
		{
			break;
		}
	}

	InOutEntry.MinCount = Remaining;
	InOutEntry.MaxCount = Remaining;
	return Remaining <= 0;
}

void AItemBase::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	// 더 이상 오버랩만으로 자동 습득하지 않음 - F키 상호작용으로만 습득 가능.
}

bool AItemBase::CanInteract_Implementation(AActor* Interactor) const
{
	return true;
}

void AItemBase::Interact_Implementation(AActor* Interactor)
{
	// 인터페이스 규약상 Interact()는 서버에서만 호출됨 (InteractionComponent::Server_Interact_Implementation 경유)
	TryPickUp(Interactor);
}

FText AItemBase::GetInteractPrompt_Implementation() const
{
	if (BundleItems.Num() > 0)
	{
		return FText::FromString(FString::Printf(TEXT("%s 외 %d종"), *DisplayName, BundleItems.Num()));
	}
	return FText::FromString(FString::Printf(TEXT("%s"), *DisplayName));
}