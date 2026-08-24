// Copyright HeavenBreach Team5. All Rights Reserved.


#include "BoxActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Item/Item.h"
#include "GameFramework/Pawn.h"
#include "Interaction/InteractionComponent.h"
#include "Engine/StaticMesh.h"

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
	DOREPLIFETIME(ABoxActor, Grade);
}

void ABoxActor::BeginPlay()
{
	Super::BeginPlay();

	// 랜덤 등급이면 서버가 먼저 추첨(Grade 복제됨), 그 다음 등급 설정 적용.
	if (HasAuthority())
	{
		RollGradeIfNeeded();
	}
	ApplyGrade();

	// 늦게 접속한 클라이언트도 초기 외형이 상태와 일치하도록 한 번 갱신한다.
	BP_OnBoxStateChanged(BoxState);
}

bool ABoxActor::CanInteract_Implementation(AActor* Interactor) const
{
	// 상자는 내용물 유무와 무관하게 항상 열 수 있다. (거리 판정은 호출측 InteractionComponent 담당)
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

void ABoxActor::OnRep_Grade()
{
	// 등급이 클라에 도착하면 등급 외형(메시/색)을 적용.
	ApplyGrade();
}

FName ABoxActor::GradeToRowName(EBoxGrade InGrade)
{
	switch (InGrade)
	{
	case EBoxGrade::Common:		return FName(TEXT("Common"));
	case EBoxGrade::Rare:		return FName(TEXT("Rare"));
	case EBoxGrade::Epic:		return FName(TEXT("Epic"));
	case EBoxGrade::Celestial:	return FName(TEXT("Celestial"));
	default:					return FName(TEXT("Common"));
	}
}

void ABoxActor::RollGradeIfNeeded()
{
	if (!bRandomGrade || BoxGradeTable == nullptr)
	{
		return;
	}

	// 등급별 SpawnWeight를 모아 가중치 랜덤 추첨
	static const EBoxGrade Grades[] = { EBoxGrade::Common, EBoxGrade::Rare, EBoxGrade::Epic, EBoxGrade::Celestial };
	float Weights[4] = { 0.f, 0.f, 0.f, 0.f };
	float Total = 0.f;
	for (int32 i = 0; i < 4; ++i)
	{
		if (const FBoxGradeData* Row = BoxGradeTable->FindRow<FBoxGradeData>(GradeToRowName(Grades[i]), TEXT("ABoxActor::RollGradeIfNeeded")))
		{
			Weights[i] = FMath::Max(0.f, Row->SpawnWeight);
			Total += Weights[i];
		}
	}

	if (Total <= 0.f)
	{
		return;
	}

	float Roll = FMath::FRand() * Total;
	for (int32 i = 0; i < 4; ++i)
	{
		Roll -= Weights[i];
		if (Roll <= 0.f)
		{
			Grade = Grades[i];
			return;
		}
	}
}

void ABoxActor::ApplyGrade()
{
	if (BoxGradeTable == nullptr)
	{
		// 등급 테이블 미설정 시 기존 BoxID를 그대로 루트 필터로 사용 (하위 호환)
		return;
	}

	const FBoxGradeData* Row = BoxGradeTable->FindRow<FBoxGradeData>(GradeToRowName(Grade), TEXT("ABoxActor::ApplyGrade"));
	if (Row == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] ApplyGrade: 등급 행 없음 '%s': %s"), *GradeToRowName(Grade).ToString(), *GetName());
		return;
	}

	// 드랍 필터 ID: 지정 없으면 등급명 사용
	EffectiveLootID = Row->LootBoxID.IsEmpty() ? GradeToRowName(Grade).ToString() : Row->LootBoxID;
	GradeColor = Row->GradeColor;
	CachedEmptyMesh = Row->EmptyMesh;

	// 등급별 몸통 메시(내용물 있는 상태) 적용
	if (BoxMesh && !Row->FullMesh.IsNull())
	{
		BoxMesh->SetStaticMesh(Row->FullMesh.LoadSynchronous());
	}
}

UStaticMesh* ABoxActor::GetEmptyMesh() const
{
	return CachedEmptyMesh.IsNull() ? nullptr : CachedEmptyMesh.LoadSynchronous();
}

void ABoxActor::GenerateLoot()
{
	if (BoxInventory == nullptr || BoxLootTable == nullptr || ItemDataTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: BoxInventory/BoxLootTable/ItemDataTable 미설정: %s"), *GetName());
		return;
	}

	// 등급 설정이 있으면 등급의 LootBoxID, 없으면 기존 BoxID로 필터 (하위 호환)
	const FString FilterID = EffectiveLootID.IsEmpty() ? BoxID : EffectiveLootID;

	// DT_BoxLoot에서 이 상자(FilterID)에 해당하는 드랍 행만 추린다.
	TArray<FBoxLootData*> AllRows;
	BoxLootTable->GetAllRows<FBoxLootData>(TEXT("BoxActor::GenerateLoot"), AllRows);

	TArray<FBoxLootData*> Eligible;
	for (FBoxLootData* Row : AllRows)
	{
		if (Row && Row->BoxID == FilterID)
		{
			Eligible.Add(Row);
		}
	}

	if (Eligible.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: 드랍 필터 ID '%s' 행 없음: %s"), *FilterID, *GetName());
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

	UE_LOG(LogTemp, Log, TEXT("[BoxActor] GenerateLoot: '%s' 상자에 아이템 %d개 생성"), *FilterID, AddedCount);
}

void ABoxActor::OpenContainerUI(AActor* Interactor)
{
	// [서버] 요청 클라에게 "이 상자 인벤토리로 컨테이너 UI를 열어라"고 Client RPC로 지시한다. (데디케이트 서버 대응)
	// 실제 UI 오픈은 클라의 InteractionComponent → OnRequestOpenContainer → (BP) InventoryUI.OpenContainer로 이어진다.
	if (Interactor == nullptr)
	{
		return;
	}

	if (UInteractionComponent* InteractionComp = Interactor->FindComponentByClass<UInteractionComponent>())
	{
		InteractionComp->Client_OpenContainer(BoxInventory);
	}
}
