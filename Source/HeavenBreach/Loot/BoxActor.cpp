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

namespace
{
	struct FLootCandidate
	{
		UDataTable* Table = nullptr;
		FName RowName = NAME_None;
	};

	template<typename TRow>
	void CollectByGrade(UDataTable* Table, TMap<EItemGrade, TArray<FLootCandidate>>& Out)
	{
		if (Table == nullptr)
		{
			return;
		}

		TArray<TRow*> Rows;
		Table->GetAllRows<TRow>(TEXT("BoxActor::CollectByGrade"), Rows);
		const TArray<FName> Names = Table->GetRowNames();

		for (int32 i = 0; i < Rows.Num() && i < Names.Num(); ++i)
		{
			if (Rows[i])
			{
				FLootCandidate Candidate;
				Candidate.Table = Table;
				Candidate.RowName = Names[i];
				Out.FindOrAdd(Rows[i]->Grade).Add(Candidate);
			}
		}
	}
}

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

	// 서칭 안 된(bRevealed=false) 아이템이 있으면 하나씩 공개하는 타이머를 시작한다.
	// 이미 돌고 있거나 전부 공개된 상태면 아무것도 안 하므로 매번 호출해도 안전 -
	// 화면을 닫았다가 다시 열어도 처음부터 다시 돌지 않고 이어서 진행된다.
	if (BoxInventory != nullptr)
	{
		BoxInventory->BeginItemRevealSequence();
	}

	OpenContainerUI(Interactor);
}

FText ABoxActor::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}

float ABoxActor::GetInteractHoldSeconds_Implementation() const
{
	// 이미 루팅을 마친 상자는 다시 탐색할 필요 없이 바로 열람 가능해야 한다.
	return bLootGenerated ? 0.0f : SearchDurationSeconds;
}

bool ABoxActor::OnHoldBegin_Implementation(AActor* Interactor)
{
	BP_OnSearchBegin();
	return true;
}

void ABoxActor::OnHoldCancel_Implementation(AActor* Interactor)
{
	BP_OnSearchCancel();
}

void ABoxActor::OnRep_BoxState()
{
	BP_OnBoxStateChanged(BoxState);
}

void ABoxActor::OnRep_Grade()
{
	RefreshGrade();
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
	BP_OnGradeChanged(Grade);
}

void ABoxActor::GenerateLoot()
{
	if (BoxInventory == nullptr || BoxGradeLootTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: BoxInventory/BoxGradeLootTable 미설정: %s"), *GetName());
		return;
	}

	const FBoxGradeLootData* Cfg = BoxGradeLootTable->FindRow<FBoxGradeLootData>(GradeToRowName(Grade), TEXT("BoxActor::GenerateLoot"));
	if (Cfg == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: 등급 설정 행 없음 '%s': %s"), *GradeToRowName(Grade).ToString(), *GetName());
		return;
	}

	TMap<EItemGrade, TArray<FLootCandidate>> ItemsByGrade;
	CollectByGrade<FItemData>(ItemDataTable, ItemsByGrade);
	CollectByGrade<FWeaponData>(WeaponDataTable, ItemsByGrade);
	CollectByGrade<FShieldData>(ShieldDataTable, ItemsByGrade);

	if (ItemsByGrade.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: 아이템 소스 테이블 없음(Item/Weapon/Shield): %s"), *GetName());
		return;
	}

	TArray<TPair<EItemGrade, float>> Weights;
	float TotalWeight = 0.0f;
	for (const TPair<EItemGrade, float>& W : Cfg->ItemGradeWeights)
	{
		if (W.Value > 0.0f && ItemsByGrade.Contains(W.Key))
		{
			Weights.Add(W);
			TotalWeight += W.Value;
		}
	}

	if (Weights.Num() == 0 || TotalWeight <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: 뽑을 수 있는 아이템 등급 없음: %s"), *GetName());
		return;
	}

	auto PickGrade = [&Weights, TotalWeight]() -> EItemGrade
	{
		float Roll = FMath::FRand() * TotalWeight;
		for (const TPair<EItemGrade, float>& W : Weights)
		{
			Roll -= W.Value;
			if (Roll <= 0.0f)
			{
				return W.Key;
			}
		}
		return Weights.Last().Key;
	};

	auto AddOneOfGrade = [this, &ItemsByGrade](EItemGrade InGrade) -> bool
	{
		const TArray<FLootCandidate>* Pool = ItemsByGrade.Find(InGrade);
		if (Pool == nullptr || Pool->Num() == 0)
		{
			return false;
		}

		const FLootCandidate& Candidate = (*Pool)[FMath::RandRange(0, Pool->Num() - 1)];

		FItemInstance Item;
		Item.ItemRow.DataTable = Candidate.Table;
		Item.ItemRow.RowName = Candidate.RowName;
		Item.InstanceId = FGuid::NewGuid();
		Item.StackCount = 1;
		// 상자에 갓 생성된 아이템은 아직 서칭 전 - 화면이 열린 뒤 BeginItemRevealSequence()가
		// 순서대로 공개해줄 때까지 "?"로 보인다.
		Item.bRevealed = false;

		FItemViewData ViewData;
		if (ResolveItemViewData(Item.ItemRow, ViewData) && ViewData.IsEquippable())
		{
			Item.CurrentDurability = GetMaxDurabilityForGrade(ViewData.Grade);
		}

		int32 Remainder = 0;
		BoxInventory->TryAddItem(Item, Remainder);
		return Remainder == 0;
	};

	const int32 MinDraws = FMath::Max(1, Cfg->MinDraws);
	const int32 MaxDraws = FMath::Max(MinDraws, Cfg->MaxDraws);
	const int32 Draws = FMath::RandRange(MinDraws, MaxDraws);

	int32 AddedCount = 0;
	for (int32 d = 0; d < Draws; ++d)
	{
		if (AddOneOfGrade(PickGrade()))
		{
			++AddedCount;
		}
	}

	if (AddedCount == 0)
	{
		if (AddOneOfGrade(PickGrade()))
		{
			++AddedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BoxActor] GenerateLoot: 등급 '%s' 상자에 아이템 %d개 생성"), *GradeToRowName(Grade).ToString(), AddedCount);
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
