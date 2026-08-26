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

ABoxActor::ABoxActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	RootComponent = BoxMesh;
	BoxMesh->SetCollisionProfileName(FName("BlockAll"));

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(BoxMesh);
	InteractionVolume->SetBoxExtent(FVector(80.0f, 80.0f, 80.0f));
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldStatic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	BoxInventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("BoxInventory"));

	GradeBoxIDs.Add(EBoxGrade::Common, FName(TEXT("BOX_001")));
	GradeBoxIDs.Add(EBoxGrade::Rare, FName(TEXT("BOX_002")));
	GradeBoxIDs.Add(EBoxGrade::Epic, FName(TEXT("BOX_003")));
	GradeBoxIDs.Add(EBoxGrade::Celestial, FName(TEXT("BOX_004")));

	GradeSpawnWeights.Add(EBoxGrade::Common, 60.0f);
	GradeSpawnWeights.Add(EBoxGrade::Rare, 25.0f);
	GradeSpawnWeights.Add(EBoxGrade::Epic, 12.0f);
	GradeSpawnWeights.Add(EBoxGrade::Celestial, 3.0f);
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

	if (HasAuthority())
	{
		RollGradeIfNeeded();
	}

	RefreshGrade();

	BP_OnBoxStateChanged(BoxState);
}

bool ABoxActor::CanInteract_Implementation(AActor* Interactor) const
{
	return Interactor != nullptr;
}

void ABoxActor::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Interactor == nullptr)
	{
		return;
	}

	if (!bLootGenerated)
	{
		BoxState = EBoxState::Opened;
		GenerateLoot();
		bLootGenerated = true;

		OnRep_BoxState();
	}

	OpenContainerUI(Interactor);
}

FText ABoxActor::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}

void ABoxActor::OnRep_BoxState()
{
	BP_OnBoxStateChanged(BoxState);
}

void ABoxActor::OnRep_Grade()
{
	RefreshGrade();
}

void ABoxActor::RollGradeIfNeeded()
{
	if (!bRandomGrade || GradeSpawnWeights.Num() == 0)
	{
		return;
	}

	float Total = 0.0f;
	for (const TPair<EBoxGrade, float>& Pair : GradeSpawnWeights)
	{
		Total += FMath::Max(0.0f, Pair.Value);
	}

	if (Total <= 0.0f)
	{
		return;
	}

	float Roll = FMath::FRand() * Total;
	for (const TPair<EBoxGrade, float>& Pair : GradeSpawnWeights)
	{
		Roll -= FMath::Max(0.0f, Pair.Value);
		if (Roll <= 0.0f)
		{
			Grade = Pair.Key;
			return;
		}
	}
}

void ABoxActor::RefreshGrade()
{
	if (const FName* Found = GradeBoxIDs.Find(Grade))
	{
		EffectiveBoxID = *Found;
	}
	else
	{
		EffectiveBoxID = NAME_None;
	}

	BP_OnGradeChanged(Grade);
}

void ABoxActor::GenerateLoot()
{
	if (BoxInventory == nullptr || BoxLootTable == nullptr || ItemDataTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: BoxInventory/BoxLootTable/ItemDataTable 미설정: %s"), *GetName());
		return;
	}

	if (EffectiveBoxID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: 등급에 매핑된 BoxID 없음(GradeBoxIDs 확인): %s"), *GetName());
		return;
	}

	const FString FilterID = EffectiveBoxID.ToString();

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
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: BoxID '%s' 드랍 행 없음: %s"), *FilterID, *GetName());
		return;
	}

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
	if (Interactor == nullptr)
	{
		return;
	}

	if (UInteractionComponent* InteractionComp = Interactor->FindComponentByClass<UInteractionComponent>())
	{
		InteractionComp->Client_OpenContainer(BoxInventory);
	}
}
