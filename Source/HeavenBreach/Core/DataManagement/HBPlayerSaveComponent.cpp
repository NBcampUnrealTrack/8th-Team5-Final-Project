#include "Core/DataManagement/HBPlayerSaveComponent.h"

#include "Core/DataManagement/HBPlayerSaveGame.h"
#include "Core/DataManagement/LoadoutStorage.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Player/PlayerState/HB_PlayerState.h"
#include "Inventory/StashComponent.h"
#include "Inventory/QuickSlotComponent.h"
#include "InventoryComponent.h"
#include "EquipmentComponent.h"

UHBPlayerSaveComponent::UHBPlayerSaveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// RPC 를 쓰려면 복제되는 컴포넌트여야 한다
	SetIsReplicatedByDefault(true);
}

void UHBPlayerSaveComponent::BeginPlay()
{
	Super::BeginPlay();

	if (SaveMode != EHBSaveMode::Local)
	{
		return;
	}

	// 다음 틱에 시작한다. 리슨 호스트 자신은 보내기 · 적용이 한 자리에서 바로 이어지는데,
	// 지금은 인벤토리 · 창고 컴포넌트가 아직 BeginPlay 전일 수 있다 (칸 초기화가 거기서 된다).
	// 그때 넣으면 칸이 없어 아이템이 빠지고, 이어서 초기화가 비워 버린다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UHBPlayerSaveComponent::TryBeginLocalLoad);
	}
}

void UHBPlayerSaveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadRetryTimer);
		World->GetTimerManager().ClearTimer(SaveTimer);
	}

	// 레벨 이동(로비 → 매치) · 종료 직전. 모아 두던 저장을 지금 바로 쓴다.
	SaveNow(false);

	Super::EndPlay(EndPlayReason);
}

// ================= 내 PC =================

bool UHBPlayerSaveComponent::IsMine(bool& bOutUndecided) const
{
	bOutUndecided = false;

	const AActor* PlayerState = GetOwner();
	if (!PlayerState)
	{
		return false;
	}

	// PlayerState 의 소유자는 그 플레이어의 컨트롤러다. 남의 PlayerState 는 내 PC 에 소유자가 없다.
	const APlayerController* PC = Cast<APlayerController>(PlayerState->GetOwner());
	if (!PC)
	{
		// 클라에서는 내 것이어도 소유자가 아직 복제 전일 수 있다. 판단을 미룬다.
		bOutUndecided = true;
		return false;
	}
	return PC->IsLocalController();
}

void UHBPlayerSaveComponent::TryBeginLocalLoad()
{
	bool bUndecided = false;
	if (!IsMine(bUndecided))
	{
		if (!bUndecided)
		{
			return;   // 남의 PlayerState. 내가 할 일이 없다
		}

		// 창고 불러오기의 Steam ID 재시도와 같은 방식. 끝까지 안 정해지면 내 것이 아니다.
		const int32 MaxRetries = 20;
		const float RetryDelay = 0.25f;
		++LoadRetryCount;
		if (LoadRetryCount < MaxRetries)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(LoadRetryTimer, this, &UHBPlayerSaveComponent::TryBeginLocalLoad, RetryDelay, false);
			}
		}
		return;
	}

	const FString SlotName = MakeSlotName();
	UHBPlayerSaveGame* Save = nullptr;

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Save = Cast<UHBPlayerSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		if (!Save)
		{
			bLoadFailed = true;
			UE_LOG(LogTemp, Error, TEXT("[Save] %s 를 못 읽었다 — 덮어쓰지 않게 이번 접속은 저장을 끈다"), *SlotName);
			return;
		}
	}
	else
	{
		// 첫 실행. 빈 데이터로 보낸다 — 서버는 빈 창고 · 인벤토리로 시작하고 테스트 골드를 준다.
		Save = Cast<UHBPlayerSaveGame>(UGameplayStatics::CreateSaveGameObject(UHBPlayerSaveGame::StaticClass()));
	}

	if (!Save)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Save] 불러옴 %s | 창고 %d 인벤 %d"), *SlotName, Save->Stash.Num(), Save->Inventory.Num());
	SubmitToServer(*Save);
}

FString UHBPlayerSaveComponent::MakeSlotName() const
{
	FString Account = TEXT("TestAccount");
	if (const AHB_PlayerState* PlayerState = Cast<AHB_PlayerState>(GetOwner()))
	{
		Account = PlayerState->ResolveAccountId();
	}

#if WITH_EDITOR
	// PIE 는 스팀이 없어서 모두 TestAccount 다. 같은 PC · 같은 폴더라 PIE 번호로 나눈다.
	// 이게 없으면 외부 서버 방식처럼 PIE 두 명이 서로 덮어쓴다. 실제 게임 빌드에는 안 들어간다.
	if (Account == TEXT("TestAccount"))
	{
		if (const UWorld* World = GetWorld())
		{
			Account += FString::Printf(TEXT("_%d"), World->GetOutermost()->GetPIEInstanceID());
		}
	}
#endif

	return FString::Printf(TEXT("Profile_%s"), *Account);
}

void UHBPlayerSaveComponent::SubmitToServer(const UHBPlayerSaveGame& Save)
{
	Server_BeginSubmit();

	for (int32 Start = 0; Start < Save.Stash.Num(); Start += StashChunkSize)
	{
		const int32 Count = FMath::Min(StashChunkSize, Save.Stash.Num() - Start);
		TArray<FInventoryEntry> Chunk;
		Chunk.Append(Save.Stash.GetData() + Start, Count);
		Server_SubmitStashChunk(Chunk);
	}

	// Reliable RPC 는 보낸 순서대로 도착한다. 창고가 다 간 뒤에 이게 간다.
	Server_FinishSubmit(Save.Inventory, Save.Equipment, Save.QuickSlots, Save.StartingLoadout);
}

void UHBPlayerSaveComponent::Client_SubmitApplied_Implementation()
{
	// 여기서 바로 저장하지 않는다. 이 알림이 복제 데이터보다 먼저 올 수 있어서, 지금 쓰면 빈 데이터가 들어간다.
	// 묶어 두면 복제가 도착할 때 바뀜 알림이 와서 저장된다.
	bLoadApplied = true;
	BindSaveTriggers();
}

void UHBPlayerSaveComponent::BindSaveTriggers()
{
	AHB_PlayerState* PlayerState = Cast<AHB_PlayerState>(GetOwner());
	if (!PlayerState)
	{
		return;
	}

	if (UStashComponent* Stash = PlayerState->GetStash())
	{
		Stash->OnInventoryChanged.AddUniqueDynamic(this, &UHBPlayerSaveComponent::RequestSave);
	}
	if (UInventoryComponent* Inventory = PlayerState->GetInventory())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UHBPlayerSaveComponent::RequestSave);
	}
	if (UInventoryComponent* BagInventory = PlayerState->GetBagInventory())
	{
		BagInventory->OnInventoryChanged.AddUniqueDynamic(this, &UHBPlayerSaveComponent::RequestSave);
	}
	if (UEquipmentComponent* Equipment = PlayerState->GetEquipment())
	{
		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UHBPlayerSaveComponent::RequestSave);
	}
	if (UQuickSlotComponent* QuickSlot = PlayerState->GetQuickSlot())
	{
		QuickSlot->OnQuickSlotChanged.AddUObject(this, &UHBPlayerSaveComponent::RequestSave);
	}
	PlayerState->OnStartingLoadoutChanged.AddUniqueDynamic(this, &UHBPlayerSaveComponent::RequestSave);
}

void UHBPlayerSaveComponent::RequestSave()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// 창고 · 로드아웃 저장과 같은 방식. 연달아 바뀌면 마지막 것만 쓴다.
	World->GetTimerManager().ClearTimer(SaveTimer);
	FTimerDelegate SaveDelegate;
	SaveDelegate.BindUObject(this, &UHBPlayerSaveComponent::SaveNow, true);
	World->GetTimerManager().SetTimer(SaveTimer, SaveDelegate, SaveDelay, false);
}

void UHBPlayerSaveComponent::SaveNow(bool bAsync)
{
	if (SaveMode != EHBSaveMode::Local)
	{
		return;
	}
	if (!bLoadApplied)
	{
		return;   // 적용 전엔 빈 데이터라 쓰면 파일이 지워진다
	}
	if (bLoadFailed)
	{
		return;
	}

	const AHB_PlayerState* PlayerState = Cast<AHB_PlayerState>(GetOwner());
	if (!PlayerState)
	{
		return;
	}

	UHBPlayerSaveGame* Save = Cast<UHBPlayerSaveGame>(UGameplayStatics::CreateSaveGameObject(UHBPlayerSaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}

	// 복제받은 내 데이터를 그대로 담는다. 서버가 정한 값이라 내 PC 가 따로 계산하지 않는다.
	if (const UStashComponent* Stash = PlayerState->GetStash())
	{
		Save->Stash = Stash->GetEntries();
	}
	if (const UInventoryComponent* Inventory = PlayerState->GetInventory())
	{
		Save->Inventory = Inventory->GetEntries();
	}
	if (const UEquipmentComponent* Equipment = PlayerState->GetEquipment())
	{
		Save->Equipment = Equipment->GetSlots();
	}
	if (const UQuickSlotComponent* QuickSlot = PlayerState->GetQuickSlot())
	{
		Save->QuickSlots = QuickSlot->GetSlots();
	}
	Save->StartingLoadout = PlayerState->GetStartingLoadout();

	const FString SlotName = MakeSlotName();
	if (bAsync)
	{
		UGameplayStatics::AsyncSaveGameToSlot(Save, SlotName, 0);
	}
	else
	{
		UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
	}
	UE_LOG(LogTemp, Log, TEXT("[Save] 저장 %s | 창고 %d 인벤 %d"), *SlotName, Save->Stash.Num(), Save->Inventory.Num());
}

// ================= 서버 =================

void UHBPlayerSaveComponent::Server_BeginSubmit_Implementation()
{
	PendingStash.Reset();
}

void UHBPlayerSaveComponent::Server_SubmitStashChunk_Implementation(const TArray<FInventoryEntry>& Chunk)
{
	if (bSubmitAccepted)
	{
		return;
	}

	const AHB_PlayerState* PlayerState = Cast<AHB_PlayerState>(GetOwner());
	if (!PlayerState)
	{
		return;
	}
	const UStashComponent* Stash = PlayerState->GetStash();
	if (!Stash)
	{
		return;
	}

	// 창고 칸 수보다 많이 올 수는 없다. 넘으면 버린다 — 메모리를 채우는 조작을 막는다.
	const FIntPoint Grid = Stash->GetGridSize();
	if (PendingStash.Num() + Chunk.Num() > Grid.X * Grid.Y)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] 창고 줄이 칸 수(%d)보다 많다 — 넘는 것은 버린다"), Grid.X * Grid.Y);
		return;
	}
	PendingStash.Append(Chunk);
}

void UHBPlayerSaveComponent::Server_FinishSubmit_Implementation(const TArray<FInventoryEntry>& InInventory,
	const TArray<FItemInstance>& InEquipment, const TArray<FGuid>& InQuickSlots, const FHBStartingLoadout& InStartingLoadout)
{
	AHB_PlayerState* PlayerState = Cast<AHB_PlayerState>(GetOwner());
	if (!PlayerState)
	{
		return;
	}
	if (bSubmitAccepted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] 두 번째 제출 — 무시한다 | %s"), *GetNameSafe(PlayerState));
		return;
	}
	bSubmitAccepted = true;

	TSet<FGuid> SeenIds;

	TArray<FInventoryEntry> StashEntries = MoveTemp(PendingStash);
	PendingStash.Reset();
	SanitizeEntries(StashEntries, SeenIds);

	FPlayerLoadout Loadout;
	Loadout.InventoryEntries = InInventory;
	SanitizeEntries(Loadout.InventoryEntries, SeenIds);

	// 장착 칸은 자리 번호가 곧 슬롯이라 빼지 않고 비운다
	Loadout.EquipmentSlots = InEquipment;
	for (FItemInstance& Slot : Loadout.EquipmentSlots)
	{
		if (Slot.ItemRow.IsNull())
		{
			continue;
		}
		if (!SanitizeItem(Slot, SeenIds))
		{
			Slot = FItemInstance();
		}
	}
	Loadout.QuickSlots = InQuickSlots;

	PlayerState->ApplySavedData(StashEntries, Loadout, InStartingLoadout);
	Client_SubmitApplied();
}

void UHBPlayerSaveComponent::SanitizeEntries(TArray<FInventoryEntry>& InOutEntries, TSet<FGuid>& InOutSeenIds) const
{
	for (int32 Index = InOutEntries.Num() - 1; Index >= 0; --Index)
	{
		if (!SanitizeItem(InOutEntries[Index].Item, InOutSeenIds))
		{
			InOutEntries.RemoveAt(Index);
		}
	}
}

bool UHBPlayerSaveComponent::SanitizeItem(FItemInstance& InOutItem, TSet<FGuid>& InOutSeenIds) const
{
	FItemViewData View;
	if (!ResolveItemViewData(InOutItem.ItemRow, View))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] 없는 아이템 %s — 뺀다"), *InOutItem.ItemRow.RowName.ToString());
		return false;
	}

	InOutItem.StackCount = FMath::Clamp(InOutItem.StackCount, 1, View.MaxStack);

	if (View.IsEquippable())
	{
		const float MaxDurability = GetMaxDurabilityForGrade(View.Grade);
		if (InOutItem.CurrentDurability > MaxDurability)
		{
			InOutItem.CurrentDurability = MaxDurability;
		}
	}

	// 같은 ID 가 둘이면 이동 · 장착이 엉뚱한 아이템을 잡는다. 겹치면 새로 준다.
	if (!InOutItem.InstanceId.IsValid())
	{
		InOutItem.InstanceId = FGuid::NewGuid();
	}
	if (InOutSeenIds.Contains(InOutItem.InstanceId))
	{
		InOutItem.InstanceId = FGuid::NewGuid();
	}
	InOutSeenIds.Add(InOutItem.InstanceId);

	// 내 소지품은 서칭 대상이 아니다
	InOutItem.bRevealed = true;

	// 각인 값은 1차에서 믿는다 (협력 PvE). 범위로 자르려면 여기서 DT_Engrave 와 비교한다.
	return true;
}
