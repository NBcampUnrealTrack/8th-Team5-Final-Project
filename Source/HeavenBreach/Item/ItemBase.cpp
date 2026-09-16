#include "ItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
		// 1순위: 스태틱 메쉬로 시도 (잡템/무기/방패 대부분 이 경우)
		if (UStaticMesh* LoadedStaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPathToLoad))
		{
			MeshComponent->SetStaticMesh(LoadedStaticMesh);
			MeshComponent->SetWorldScale3D(FVector(1.f));
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
	return FText::FromString(FString::Printf(TEXT("%s"), *DisplayName));
}