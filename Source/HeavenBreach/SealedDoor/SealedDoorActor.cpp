// Copyright HeavenBreach Team5. All Rights Reserved.

#include "SealedDoorActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Item/Item.h"
#include "Net/UnrealNetwork.h"
#include "Player/HBPlayerCharacter.h"

namespace
{
	const TCHAR* RoomResultToText(ESealedRoomResult InResult)
	{
		switch (InResult)
		{
		case ESealedRoomResult::MonsterRoom:	return TEXT("몬스터 방");
		case ESealedRoomResult::RewardRoom:		return TEXT("보상방");
		default:								return TEXT("미결정");
		}
	}
}

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
	DOREPLIFETIME(ASealedDoorActor, OpenedServerTime);
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

	if (bDebugSkipKeyCheck)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SealedDoor] %s: bDebugSkipKeyCheck 가 켜져 있어 열쇠 없이 개방합니다."),
			*GetName());
	}
	else
	{
		UInventoryComponent* Inventory = GetInteractorInventory(Interactor);

		FGuid KeyInstanceId;
		if (!FindMatchingKey(Inventory, GradeData->RequiredKeyRowName, KeyInstanceId))
		{
			// 열쇠 없음 / 등급 불일치. 프롬프트가 이미 안내하고 있으므로 별도 알림을 보내지 않는다.
			// 단, 필요한 열쇠 이름조차 표시하지 못하는 상태(DT_Item 행 미존재 등)라면
			// 여기서 한 번 로그를 남겨 설정 누락을 알아챌 수 있게 한다.
			if (GetRequiredKeyDisplayName().IsEmpty())
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[SealedDoor] %s: 필요한 열쇠(%s)의 표시 이름을 찾지 못했습니다. ItemDataTable 설정을 확인하세요."),
					*GetName(), *GradeData->RequiredKeyRowName.ToString());
			}
			return;
		}

		if (!Inventory->RemoveStackCount(KeyInstanceId, 1))
		{
			// 방금 찾은 항목이므로 정상적으로는 도달하지 않는다. 여기 걸리면 인벤토리 불변식이 깨진 것이다.
			UE_LOG(LogTemp, Warning,
				TEXT("[SealedDoor] %s: 열쇠를 찾았지만 소비에 실패했습니다."),
				*GetName());
			return;
		}
	}

	OpenDoor();
}

void ASealedDoorActor::OpenDoor()
{
	if (!HasAuthority())
	{
		return;
	}

	RollRoomResult();
	GenerateRoom();

	// GameState 를 못 가져오면 OpenedServerTime 은 기본값(0)에 남는다 —
	// WasRecentlyOpened() 가 false 를 돌려주므로 원샷 연출이 재생되지 않는 안전한 방향이다.
	if (const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		OpenedServerTime = GameState->GetServerWorldTimeSeconds();
	}

	DoorState = ESealedDoorState::Opened;

	// 서버에서는 RepNotify 가 자동 호출되지 않는다 (ABoxActor 와 동일한 처리)
	OnRep_DoorState();
}

void ASealedDoorActor::DebugForceOpen()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SealedDoor] %s: DebugForceOpen 은 서버에서만 동작합니다."), *GetName());
		return;
	}

	if (DoorState != ESealedDoorState::Sealed)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SealedDoor] %s: 이미 열려 있습니다. 다시 열려면 DebugReseal 을 먼저 실행하세요."),
			*GetName());
		return;
	}

	OpenDoor();
}

void ASealedDoorActor::DebugReseal()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SealedDoor] %s: DebugReseal 은 서버에서만 동작합니다."), *GetName());
		return;
	}

	int32 DestroyedCount = 0;
	for (AActor* Spawned : SpawnedActors)
	{
		if (IsValid(Spawned))
		{
			Spawned->Destroy();
			++DestroyedCount;
		}
	}
	SpawnedActors.Reset();

	bRoomGenerated = false;
	RoomResult = ESealedRoomResult::Undetermined;
	OpenedServerTime = 0.0f;
	DoorState = ESealedDoorState::Sealed;

	ApplySealedVisual();

	UE_LOG(LogTemp, Warning,
		TEXT("[SealedDoor] %s: 디버그 재봉인 완료. 스폰 액터 %d개 제거."),
		*GetName(), DestroyedCount);
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

	const FText KeyDisplayName = GetRequiredKeyDisplayName();
	if (KeyDisplayName.IsEmpty())
	{
		// DT_Item 행이 아직 없는 등 이름을 못 찾은 경우. 매 프레임 호출되는 함수이므로
		// 여기서 로그를 남기지 않는다 — 대신 Interact_Implementation 쪽에서 한 번만 남긴다.
		return NSLOCTEXT("SealedDoor", "NeedKeyPromptFallback", "봉인 열쇠 필요");
	}

	return FText::Format(
		NSLOCTEXT("SealedDoor", "NeedKeyPrompt", "{0} 필요"),
		KeyDisplayName);
}

void ASealedDoorActor::OnRep_DoorState()
{
	if (DoorState != ESealedDoorState::Opened)
	{
		// 정상 게임플레이에서는 Opened 에서 Sealed 로 돌아가지 않는다. DebugReseal 전용 경로다.
		ApplySealedVisual();
		return;
	}

	{
		// 상태 훅: 열려 있는 동안 항상 호출된다. 뒤늦게 relevant 해진 클라이언트도
		// 반드시 열린 시각적 상태를 봐야 하므로 조건 없이 호출한다.
		ApplyOpenedVisual();
		BP_OnDoorOpened(Grade);

		// 원샷 훅: "방금" 열렸을 때만 호출한다. 그렇지 않으면 넷 컬링에서 벗어나 있던
		// 클라이언트나 늦게 접속한 플레이어가, 실제로는 한참 전에 열린 문에 대해
		// 봉인 파괴 연출과(PvP 위치 신호인) 개방 사운드를 재생하게 된다.
		if (WasRecentlyOpened())
		{
			BP_OnSealBroken(Grade);
		}
	}
}

bool ASealedDoorActor::WasRecentlyOpened() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		return false;
	}

	// 클라이언트의 서버 시각 추정에는 약간의 오차가 있을 수 있으므로 절댓값으로 비교한다.
	const float Age = GameState->GetServerWorldTimeSeconds() - OpenedServerTime;
	return FMath::Abs(Age) <= RecentlyOpenedWindowSeconds;
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

void ASealedDoorActor::ApplySealedVisual()
{
	if (DoorMesh)
	{
		DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		DoorMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}

	if (SealDecal)
	{
		SealDecal->SetVisibility(true);
	}
}

void ASealedDoorActor::RollRoomResult()
{
	if (DebugForcedResult != ESealedRoomResult::Undetermined)
	{
		RoomResult = DebugForcedResult;
		UE_LOG(LogTemp, Warning,
			TEXT("[SealedDoor] %s: DebugForcedResult 로 방 결과를 강제합니다 (%s)."),
			*GetName(), RoomResultToText(RoomResult));
		return;
	}

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

	const FString RowName = SealedDoorGradeToRowName(Grade).ToString();

	UE_LOG(LogTemp, Log,
		TEXT("[SealedDoor] %s: 방 결과 = %s, 등급 행 '%s', 사용 가능 스폰 포인트 %d개"),
		*GetName(), RoomResultToText(RoomResult), *RowName, AvailablePoints.Num());

	if (RoomResult == ESealedRoomResult::MonsterRoom)
	{
		if (GradeData->MonsterRoomSpawns.Num() == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SealedDoor] %s: 방 결과가 몬스터 방인데 '%s' 행의 MonsterRoomSpawns 가 비어 있습니다."),
				*GetName(), *RowName);
		}

		SpawnEntries(GradeData->MonsterRoomSpawns, AvailablePoints);
	}
	else
	{
		if (GradeData->RewardRoomSpawns.Num() == 0 && GradeData->GuardSpawns.Num() == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SealedDoor] %s: 방 결과가 보상방인데 '%s' 행의 RewardRoomSpawns 와 GuardSpawns 가 모두 비어 있습니다."),
				*GetName(), *RowName);
		}

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
			UE_LOG(LogTemp, Warning,
				TEXT("[SealedDoor] %s: 스폰 목록에 ActorClass 가 비어 있는 항목이 있어 건너뜁니다."),
				*GetName());
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

		if (Count <= 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SealedDoor] %s: '%s' 의 스폰 개수가 0 입니다 (MinCount %d / MaxCount %d)."),
				*GetName(), *Entry.ActorClass.ToString(), Entry.MinCount, Entry.MaxCount);
			continue;
		}

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

			AActor* Spawned = World->SpawnActor<AActor>(
				SpawnClass,
				Point->GetActorLocation(),
				Point->GetActorRotation(),
				SpawnParams);

			if (Spawned == nullptr)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[SealedDoor] %s: %s 스폰에 실패했습니다."),
					*GetName(), *SpawnClass->GetName());
				continue;
			}

			SpawnedActors.Add(Spawned);

			UE_LOG(LogTemp, Log,
				TEXT("[SealedDoor] %s: %s 를 %s 위치에 스폰했습니다."),
				*GetName(), *Spawned->GetName(), *Point->GetName());
		}
	}
}
