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
