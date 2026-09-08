// Copyright HeavenBreach Team5. All Rights Reserved.

#include "SealedDoorActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Item/Item.h"
#include "Net/UnrealNetwork.h"
#include "Player/HBPlayerCharacter.h"

ASealedDoorActor::ASealedDoorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	DoorFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrame"));
	RootComponent = DoorFrame;
	DoorFrame->SetCollisionProfileName(FName("BlockAll"));

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorFrame);
	DoorMesh->SetCollisionProfileName(FName("BlockAll"));

	SealDecal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SealDecal"));
	SealDecal->SetupAttachment(DoorMesh);
	SealDecal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASealedDoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASealedDoorActor, Grade);
	DOREPLIFETIME(ASealedDoorActor, DoorState);
	DOREPLIFETIME(ASealedDoorActor, RoomResult);
}

bool ASealedDoorActor::CanInteract_Implementation(AActor* Interactor) const
{
	// 열린 문은 상호작용 대상이 아니다. 열쇠 유무는 여기서 보지 않는다 —
	// 열쇠가 없어도 프롬프트가 떠야 어떤 열쇠가 필요한지 알 수 있기 때문.
	return Interactor != nullptr && DoorState == ESealedDoorState::Sealed;
}

void ASealedDoorActor::Interact_Implementation(AActor* Interactor)
{
	// UInteractionComponent::Server_Interact 가 거리를 재검증한 뒤 서버에서 호출한다.
	if (!HasAuthority() || Interactor == nullptr)
	{
		return;
	}

	if (DoorState != ESealedDoorState::Sealed)
	{
		return;
	}

	const FSealedDoorGradeData* GradeData = FindGradeData();
	if (GradeData == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SealedDoor] %s: DT_SealedDoorGrade 에서 등급 행을 찾지 못했습니다."),
			*GetName());
		return;
	}

	UInventoryComponent* Inventory = GetInteractorInventory(Interactor);

	FGuid KeyInstanceId;
	if (!FindMatchingKey(Inventory, GradeData->RequiredKeyRowName, KeyInstanceId))
	{
		// 열쇠 없음 / 등급 불일치. 프롬프트가 이미 안내하고 있으므로 별도 알림을 보내지 않는다.
		return;
	}

	if (!Inventory->RemoveStackCount(KeyInstanceId, 1))
	{
		return;
	}

	RollRoomResult();
	GenerateRoom();

	DoorState = ESealedDoorState::Opened;

	// 서버에서는 RepNotify 가 자동 호출되지 않는다 (ABoxActor 와 동일한 처리)
	OnRep_DoorState();
}

FText ASealedDoorActor::GetInteractPrompt_Implementation() const
{
	if (DoorState != ESealedDoorState::Sealed)
	{
		return FText::GetEmpty();
	}

	const FSealedDoorGradeData* GradeData = FindGradeData();
	if (GradeData == nullptr)
	{
		return FText::GetEmpty();
	}

	// 이 함수는 클라이언트에서 호출된다. 로컬 플레이어의 인벤토리로 판정하므로 RPC가 필요 없다.
	UInventoryComponent* Inventory = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			Inventory = GetInteractorInventory(PC->GetPawn());
		}
	}

	FGuid UnusedId;
	if (Inventory != nullptr && FindMatchingKey(Inventory, GradeData->RequiredKeyRowName, UnusedId))
	{
		return FText::Format(
			NSLOCTEXT("SealedDoor", "OpenPrompt", "{0} 개방 [F]"),
			GradeData->DoorDisplayName);
	}

	return FText::Format(
		NSLOCTEXT("SealedDoor", "NeedKeyPrompt", "{0} 필요"),
		GetRequiredKeyDisplayName());
}

void ASealedDoorActor::OnRep_DoorState()
{
	if (DoorState == ESealedDoorState::Opened)
	{
		ApplyOpenedVisual();

		BP_OnSealBroken(Grade);
		BP_OnDoorOpened(Grade);
	}
}

const FSealedDoorGradeData* ASealedDoorActor::FindGradeData() const
{
	if (DoorGradeTable == nullptr)
	{
		return nullptr;
	}

	return DoorGradeTable->FindRow<FSealedDoorGradeData>(
		SealedDoorGradeToRowName(Grade),
		TEXT("ASealedDoorActor::FindGradeData"));
}

UInventoryComponent* ASealedDoorActor::GetInteractorInventory(AActor* Interactor) const
{
	if (const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(Interactor))
	{
		return Character->GetInventory();
	}

	return nullptr;
}

bool ASealedDoorActor::FindMatchingKey(UInventoryComponent* Inventory, FName RequiredKeyRowName, FGuid& OutInstanceId) const
{
	if (Inventory == nullptr || RequiredKeyRowName.IsNone())
	{
		return false;
	}

	for (const FInventoryEntry& Entry : Inventory->GetEntries())
	{
		// 등급 일치는 행 이름 정확 일치로 판정한다.
		// 상위 등급 열쇠가 하위 문을 대신 열지 않아야 하므로 등급 비교로 완화하지 않는다.
		if (Entry.Item.ItemRow.RowName == RequiredKeyRowName && Entry.Item.StackCount > 0)
		{
			OutInstanceId = Entry.Item.InstanceId;
			return true;
		}
	}

	return false;
}

FText ASealedDoorActor::GetRequiredKeyDisplayName() const
{
	const FSealedDoorGradeData* GradeData = FindGradeData();
	if (GradeData == nullptr || ItemDataTable == nullptr)
	{
		return FText::GetEmpty();
	}

	if (const FItemData* Row = ItemDataTable->FindRow<FItemData>(
		GradeData->RequiredKeyRowName,
		TEXT("ASealedDoorActor::GetRequiredKeyDisplayName")))
	{
		return FText::FromString(Row->ItemName);
	}

	return FText::GetEmpty();
}

void ASealedDoorActor::ApplyOpenedVisual()
{
	if (DoorMesh)
	{
		DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DoorMesh->SetRelativeRotation(FRotator(0.0f, -100.0f, 0.0f));
	}

	if (SealDecal)
	{
		SealDecal->SetVisibility(false);
	}
}

void ASealedDoorActor::RollRoomResult()
{
	const FSealedDoorGradeData* GradeData = FindGradeData();
	if (GradeData == nullptr)
	{
		RoomResult = ESealedRoomResult::RewardRoom;
		return;
	}

	// 개방 시점에 추첨한다. 미리 정해두면 클라이언트가 문을 열기 전에 알 수 있고,
	// 몬스터를 미리 스폰하면 열지도 않은 방의 AI 가 계속 돈다.
	RoomResult = (FMath::FRand() < GradeData->MonsterRoomChance)
		? ESealedRoomResult::MonsterRoom
		: ESealedRoomResult::RewardRoom;
}

void ASealedDoorActor::GenerateRoom()
{
	// 기획서 21장: 세션 내 재생성 없음
	if (bRoomGenerated)
	{
		return;
	}
	bRoomGenerated = true;

	const FSealedDoorGradeData* GradeData = FindGradeData();
	if (GradeData == nullptr)
	{
		return;
	}

	TArray<AActor*> AvailablePoints;
	for (const TObjectPtr<AActor>& Point : SpawnPoints)
	{
		if (Point != nullptr)
		{
			AvailablePoints.Add(Point);
		}
	}

	if (AvailablePoints.Num() == 0)
	{
		// 배치 실수가 문을 잠가버리지 않도록, 경고만 남기고 문은 정상 개방한다.
		UE_LOG(LogTemp, Warning,
			TEXT("[SealedDoor] %s: SpawnPoints 가 비어 있어 내용물을 배치하지 않습니다."),
			*GetName());
		return;
	}

	for (int32 i = AvailablePoints.Num() - 1; i > 0; --i)
	{
		AvailablePoints.Swap(i, FMath::RandRange(0, i));
	}

	if (RoomResult == ESealedRoomResult::MonsterRoom)
	{
		SpawnEntries(GradeData->MonsterRoomSpawns, AvailablePoints);
	}
	else
	{
		SpawnEntries(GradeData->RewardRoomSpawns, AvailablePoints);
		SpawnEntries(GradeData->GuardSpawns, AvailablePoints);
	}
}

void ASealedDoorActor::SpawnEntries(const TArray<FSealedRoomSpawnEntry>& InEntries, TArray<AActor*>& InOutAvailablePoints)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (const FSealedRoomSpawnEntry& Entry : InEntries)
	{
		if (Entry.ActorClass.IsNull())
		{
			continue;
		}

		UClass* SpawnClass = Entry.ActorClass.LoadSynchronous();
		if (SpawnClass == nullptr)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SealedDoor] %s: 스폰 클래스 로드에 실패했습니다. (%s)"),
				*GetName(), *Entry.ActorClass.ToString());
			continue;
		}

		const int32 MinCount = FMath::Min(Entry.MinCount, Entry.MaxCount);
		const int32 MaxCount = FMath::Max(Entry.MinCount, Entry.MaxCount);
		const int32 Count = FMath::RandRange(MinCount, MaxCount);

		for (int32 i = 0; i < Count; ++i)
		{
			if (InOutAvailablePoints.Num() == 0)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[SealedDoor] %s: 스폰 포인트가 부족해 남은 스폰을 건너뜁니다."),
					*GetName());
				return;
			}

			const AActor* Point = InOutAvailablePoints.Pop();

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			World->SpawnActor<AActor>(
				SpawnClass,
				Point->GetActorLocation(),
				Point->GetActorRotation(),
				SpawnParams);
		}
	}
}
