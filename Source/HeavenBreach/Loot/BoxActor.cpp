// Copyright HeavenBreach Team5. All Rights Reserved.


#include "BoxActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Item.h"
#include "GameFramework/Pawn.h"

ABoxActor::ABoxActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 상자 상태/추첨을 서버가 결정하고 모든 클라이언트에 동기화한다.
	bReplicates = true;

	// 상자 본체 메시를 루트로 사용
	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	RootComponent = BoxMesh;
	BoxMesh->SetCollisionProfileName(FName("BlockAll"));

	// 상호작용 감지 볼륨
	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(BoxMesh);
	InteractionVolume->SetBoxExtent(FVector(80.0f, 80.0f, 80.0f));
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldStatic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 상자 내용물 인벤토리 (그리드 크기는 컴포넌트 Details에서 조정)
	BoxInventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("BoxInventory"));
}

void ABoxActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABoxActor, BoxState);
	DOREPLIFETIME(ABoxActor, bLootGenerated);
}

void ABoxActor::BeginPlay()
{
	Super::BeginPlay();

	// 늦게 접속한 클라이언트도 초기 외형이 상태와 일치하도록 한 번 갱신한다.
	BP_OnBoxStateChanged(BoxState);
}

bool ABoxActor::CanInteract_Implementation(AActor* Interactor) const
{
	// 상자는 열린 뒤에도 남은 아이템 회수를 위해 재접근이 가능하다. (거리 판정은 호출측 담당)
	return Interactor != nullptr;
}

void ABoxActor::Interact_Implementation(AActor* Interactor)
{
	// 상태 전환/추첨은 서버 권한으로만 수행한다.
	if (!HasAuthority())
	{
		return;
	}

	if (Interactor == nullptr)
	{
		return;
	}

	// 최초 개방: 상태 전환 + 무작위 아이템 생성 (1회)
	if (!bLootGenerated)
	{
		BoxState = EBoxState::Opened;
		GenerateLoot();
		bLootGenerated = true;

		// 서버(리슨 서버 포함)에서도 외형을 즉시 갱신. RepNotify는 서버 setter에서 자동 호출되지 않음.
		OnRep_BoxState();
	}

	// 최초/재접근 공통: 이중 인벤토리 UI를 열고 이동/공격을 잠근다.
	OpenContainerUI(Interactor);
}

FText ABoxActor::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}

void ABoxActor::OnRep_BoxState()
{
	// 모든 클라이언트에서 열림/닫힘 외형을 동일하게 갱신한다. 세부 연출은 Blueprint에서 구현.
	BP_OnBoxStateChanged(BoxState);
}

void ABoxActor::GenerateLoot()
{
	if (BoxInventory == nullptr || BoxLootTable == nullptr || ItemDataTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: BoxInventory/BoxLootTable/ItemDataTable 미설정: %s"), *GetName());
		return;
	}

	// DT_BoxLoot에서 이 상자(BoxID)에 해당하는 드랍 행만 추린다.
	TArray<FBoxLootData*> AllRows;
	BoxLootTable->GetAllRows<FBoxLootData>(TEXT("BoxActor::GenerateLoot"), AllRows);

	TArray<FBoxLootData*> Eligible;
	for (FBoxLootData* Row : AllRows)
	{
		if (Row && Row->BoxID == BoxID)
		{
			Eligible.Add(Row);
		}
	}

	if (Eligible.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: BoxID '%s' 드랍 행 없음: %s"), *BoxID, *GetName());
		return;
	}

	// 아이템 1개를 상자 인벤토리에 넣는다. (스택형은 TryAddItem이 기존 스택에 병합)
	auto AddOne = [this](const FBoxLootData& Row) -> bool
	{
		FItemInstance Item;
		Item.ItemRow.DataTable = ItemDataTable;
		Item.ItemRow.RowName = FName(*Row.ItemID);
		Item.InstanceId = FGuid::NewGuid();
		Item.StackCount = 1;

		int32 Remainder = 0;
		BoxInventory->TryAddItem(Item, Remainder);
		return Remainder == 0;
	};

	int32 AddedCount = 0;

	// 행마다 DropChance로 판정, 성공 시 1~MaxItemCount개 추가
	for (const FBoxLootData* Row : Eligible)
	{
		if (FMath::FRand() <= Row->DropChance)
		{
			const int32 Count = FMath::RandRange(1, FMath::Max(1, Row->MaxItemCount));
			for (int32 i = 0; i < Count; ++i)
			{
				if (AddOne(*Row))
				{
					++AddedCount;
				}
			}
		}
	}

	// 최소 1개 보장: 하나도 안 나왔으면 DropChance가 가장 높은 행에서 1개 강제 추가
	if (AddedCount == 0)
	{
		const FBoxLootData* Best = Eligible[0];
		for (const FBoxLootData* Row : Eligible)
		{
			if (Row->DropChance > Best->DropChance)
			{
				Best = Row;
			}
		}
		if (AddOne(*Best))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BoxActor] GenerateLoot: '%s' 상자에 아이템 %d개 생성"), *BoxID, AddedCount);
}

void ABoxActor::OpenContainerUI(AActor* Interactor)
{
	// 실제 위젯 생성/입력 잠금은 BP_OpenContainer에서 처리한다. (호스트/스탠드얼론 기준)
	// NOTE: 인벤토리 미복제 상태라, 원격 클라 동기화는 Inventory 복제 구현 후 완성된다.
	BP_OpenContainer(Cast<APawn>(Interactor));
}
