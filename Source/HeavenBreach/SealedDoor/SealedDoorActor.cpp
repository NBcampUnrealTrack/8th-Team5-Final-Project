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
	DOREPLIFETIME(ASealedDoorActor, SequenceStartServerTime);
}

bool ASealedDoorActor::CanInteract_Implementation(AActor* Interactor) const
{
	// 열린 문은 상호작용 대상이 아니다. 열쇠 유무는 여기서 보지 않는다 —
	// 열쇠가 없어도 프롬프트가 떠야 어떤 열쇠가 필요한지 알 수 있기 때문.
	//
	// Opening 도 true 를 돌려줘야 한다. UInteractionComponent 가 홀드 도중 매 틱
	// 이 함수로 대상 유효성을 확인하기 때문에, 여기서 false 를 주면 봉인 해제가
	// 시작하자마자 스스로 끊긴다. 진행 중임은 프롬프트를 비워서 알린다.
	return Interactor != nullptr
		&& (DoorState == ESealedDoorState::Sealed || DoorState == ESealedDoorState::Opening);
}

void ASealedDoorActor::Interact_Implementation(AActor* Interactor)
{
	// UInteractionComponent::Server_Interact 가 거리를 재검증한 뒤 서버에서 호출한다.
	if (!HasAuthority() || Interactor == nullptr)
	{
		return;
	}

	// 홀드가 끝난 시점에는 Opening 이고, 홀드 시간이 0 인 등급이면 Sealed 인 채로 바로 들어온다.
	if (DoorState == ESealedDoorState::Opened)
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

	FinishOpening();
}

float ASealedDoorActor::GetInteractHoldSeconds_Implementation() const
{
	const FSealedDoorGradeData* GradeData = FindGradeData();
	return GradeData ? GradeData->OpenDurationSeconds : 0.0f;
}

bool ASealedDoorActor::HasRequiredKey(AActor* Interactor) const
{
	if (bDebugSkipKeyCheck)
	{
		return true;
	}

	const FSealedDoorGradeData* GradeData = FindGradeData();
	if (GradeData == nullptr)
	{
		return false;
	}

	FGuid UnusedId;
	return FindMatchingKey(GetInteractorInventory(Interactor), GradeData->RequiredKeyRowName, UnusedId);
}

bool ASealedDoorActor::OnHoldBegin_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return false;
	}

	// 이미 다른 플레이어가 봉인을 해제하는 중이면 상태를 건드리지 않되, 이 플레이어의 홀드는 허용한다.
	// 먼저 끝낸 쪽이 문을 연다.
	if (DoorState == ESealedDoorState::Opening)
	{
		return HasRequiredKey(Interactor);
	}

	if (DoorState != ESealedDoorState::Sealed)
	{
		return false;
	}

	// 열쇠를 갖고 있지 않으면 시작 자체를 거절한다. 시작 시점에 방을 배치하기 때문에,
	// 열쇠 없는 플레이어가 눌렀다 떼는 것만으로 맵의 방들을 채울 수 있으면 안 된다.
	// 소비는 완료 시점(Interact)에 하므로 여기서는 보유 여부만 본다.
	if (!HasRequiredKey(Interactor))
	{
		return false;
	}

	HoldingInteractor = Interactor;

	// GameState 를 못 가져오면 SequenceStartServerTime 은 기본값(0)에 남는다 —
	// WasRecentlyStarted() 가 false 를 돌려주므로 원샷 연출이 재생되지 않는 안전한 방향이다.
	if (const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		SequenceStartServerTime = GameState->GetServerWorldTimeSeconds();
	}

	// 내용물을 여기서 미리 배치한다. 완료 시점에 스폰하면 새 액터 채널이 열리는 데 걸리는
	// 시간 때문에, 핑이 나쁜 클라이언트는 텅 빈 방이 열린 뒤 몬스터가 뒤늦게 나타나는 걸 본다.
	// 홀드 시간만큼 앞당기면 복제가 끝날 여유가 생기고, 몬스터의 AI 빙의·비헤이비어 트리
	// 초기화도 문이 열리기 전에 마무리된다.
	//
	// 홀드를 중간에 놓아도 배치된 내용물은 그대로 둔다 — bRoomGenerated 가 재생성을 막으므로
	// 눌렀다 떼기를 반복해도 스폰·파괴가 되풀이되지 않는다.
	RollRoomResult();
	GenerateRoom();

	DoorState = ESealedDoorState::Opening;

	// 서버에서는 RepNotify 가 자동 호출되지 않는다 (ABoxActor 와 동일한 처리)
	OnRep_DoorState();

	UE_LOG(LogTemp, Log,
		TEXT("[SealedDoor] %s: 봉인 해제 시작 (%.1f초 유지 필요)"),
		*GetName(), GetInteractHoldSeconds_Implementation());

	return true;
}

void ASealedDoorActor::OnHoldCancel_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || DoorState != ESealedDoorState::Opening)
	{
		return;
	}

	// 연출을 시작한 당사자가 놓았을 때만 되돌린다. 옆 사람이 잠깐 눌렀다 놓는 것으로
	// 진행 중인 봉인 해제가 끊기면 안 된다.
	if (HoldingInteractor != Interactor)
	{
		return;
	}

	HoldingInteractor = nullptr;

	// 열쇠는 완료 시점(Interact)에 소비하므로 여기서 되돌릴 것이 없다.
	DoorState = ESealedDoorState::Sealed;
	SequenceStartServerTime = 0.0f;

	OnRep_DoorState();

	UE_LOG(LogTemp, Log, TEXT("[SealedDoor] %s: 봉인 해제 중단됨"), *GetName());
}

void ASealedDoorActor::FinishOpening()
{
	if (!HasAuthority())
	{
		return;
	}

	HoldingInteractor = nullptr;

	// 정상 경로에서는 OnHoldBegin 이 이미 배치를 끝냈고, GenerateRoom 은 bRoomGenerated 로
	// 막힌다. 홀드를 거치지 않는 경로(홀드 시간 0, 디버그 강제 개방)를 위해 여기서도 호출한다.
	if (RoomResult == ESealedRoomResult::Undetermined)
	{
		RollRoomResult();
	}
	GenerateRoom();

	DoorState = ESealedDoorState::Opened;

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
			TEXT("[SealedDoor] %s: 이미 열렸거나 봉인 해제 중입니다. 다시 열려면 DebugReseal 을 먼저 실행하세요."),
			*GetName());
		return;
	}

	FinishOpening();
}

void ASealedDoorActor::DebugReseal()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SealedDoor] %s: DebugReseal 은 서버에서만 동작합니다."), *GetName());
		return;
	}

	HoldingInteractor = nullptr;

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
	SequenceStartServerTime = 0.0f;
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
			NSLOCTEXT("SealedDoor", "OpenPrompt", "{0} 개방 [F 길게]"),
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
	switch (DoorState)
	{
	case ESealedDoorState::Sealed:
		// 정상 게임플레이에서는 Sealed 로 돌아가지 않는다. DebugReseal 전용 경로다.
		ApplySealedVisual();
		break;

	case ESealedDoorState::Opening:
	{
		// 원샷 훅: 연출이 "방금" 시작됐을 때만 호출한다. 그렇지 않으면 넷 컬링에서 벗어나
		// 있던 클라이언트나 늦게 접속한 플레이어가, 실제로는 한참 전에 시작된 문에 대해
		// 봉인 파괴 연출과(PvP 위치 신호인) 개방 사운드를 재생하게 된다.
		if (WasRecentlyStarted())
		{
			const FSealedDoorGradeData* GradeData = FindGradeData();
			BP_OnSealBroken(Grade, GradeData ? GradeData->OpenDurationSeconds : 0.0f);
		}
		break;
	}

	case ESealedDoorState::Opened:
		// 상태 훅: 열려 있는 동안 항상 호출된다. 뒤늦게 relevant 해진 클라이언트도
		// 반드시 열린 시각적 상태를 봐야 하므로 조건 없이 호출한다.
		ApplyOpenedVisual();
		BP_OnDoorOpened(Grade);
		break;
	}
}

bool ASealedDoorActor::WasRecentlyStarted() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		return false;
	}

	// 클라이언트의 서버 시각 추정에는 약간의 오차가 있을 수 있으므로 절댓값으로 비교한다.
	const float Age = GameState->GetServerWorldTimeSeconds() - SequenceStartServerTime;
	return FMath::Abs(Age) <= RecentlyStartedWindowSeconds;
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
		// MaxCount 가 0 이면 이 항목은 절대 스폰되지 않는다 — 설정 실수로 본다.
		// 반면 MinCount 가 0 이고 MaxCount 가 1 이상인 것은 "가끔 등장" 을 의도한 정상 설정이므로,
		// 이번 추첨에서 0 이 나왔다고 경고하지 않는다.
		if (MaxCount <= 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SealedDoor] %s: '%s' 는 MaxCount 가 0 이라 절대 스폰되지 않습니다."),
				*GetName(), *Entry.ActorClass.ToString());
			continue;
		}

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
