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

	// 종류 → 등급 → 후보.
	using FLootPools = TMap<EBoxLootCategory, TMap<EItemGrade, TArray<FLootCandidate>>>;

	// 드랍 가능한 행을 종류·등급별로 모은다. CategoryOf 는 행 하나를 받아 종류를 돌려준다.
	template<typename TRow, typename TCategoryOf>
	void CollectPools(UDataTable* Table, TCategoryOf CategoryOf, FLootPools& Out)
	{
		if (Table == nullptr)
		{
			return;
		}

		TArray<TRow*> Rows;
		Table->GetAllRows<TRow>(TEXT("BoxActor::CollectPools"), Rows);
		const TArray<FName> Names = Table->GetRowNames();

		for (int32 i = 0; i < Rows.Num() && i < Names.Num(); ++i)
		{
			if (!Rows[i])
			{
				continue;
			}

			// 에셋 미완성 등으로 드랍 안 시킬 행은 여기서 상자 후보 풀에서 제외된다.
			if (!Rows[i]->bDroppable)
			{
				continue;
			}

			FLootCandidate Candidate;
			Candidate.Table = Table;
			Candidate.RowName = Names[i];
			Out.FindOrAdd(CategoryOf(*Rows[i])).FindOrAdd(Rows[i]->Grade).Add(Candidate);
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

	// 무기·방패 표는 전부 장비. 일반 아이템 표는 ItemType 으로 나눈다(열쇠·소모품·방어구 등).
	FLootPools Pools;
	CollectPools<FItemData>(ItemDataTable, [](const FItemData& Row) { return HBBoxLoot::CategoryOfItemType(Row.ItemType); }, Pools);
	CollectPools<FWeaponData>(WeaponDataTable, [](const FWeaponData&) { return EBoxLootCategory::Equipment; }, Pools);
	CollectPools<FShieldData>(ShieldDataTable, [](const FShieldData&) { return EBoxLootCategory::Equipment; }, Pools);

	if (Pools.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: 아이템 소스 테이블 없음(Item/Weapon/Shield): %s"), *GetName());
		return;
	}

	// 이 상자 등급에서 나올 수 있는 등급(가중치 > 0)만 쓴다.
	auto GradeWeightsFor = [Cfg](const TMap<EItemGrade, TArray<FLootCandidate>>& ByGrade, TArray<EItemGrade>& OutGrades, TArray<float>& OutWeights)
	{
		for (const TPair<EItemGrade, float>& W : Cfg->ItemGradeWeights)
		{
			const TArray<FLootCandidate>* Pool = ByGrade.Find(W.Key);
			if (W.Value > 0.0f && Pool && Pool->Num() > 0)
			{
				OutGrades.Add(W.Key);
				OutWeights.Add(W.Value);
			}
		}
	};

	// 종류 비율. 이 상자 등급에서 하나도 나올 수 없는 종류는 빼서, 남은 종류끼리 비율을 나눈다.
	TArray<EBoxLootCategory> Categories;
	TArray<float> CategoryWeights;
	for (const TPair<EBoxLootCategory, float>& W : HBBoxLoot::ResolveCategoryWeights(Cfg->CategoryWeights))
	{
		const TMap<EItemGrade, TArray<FLootCandidate>>* ByGrade = Pools.Find(W.Key);
		if (W.Value <= 0.0f || ByGrade == nullptr)
		{
			continue;
		}
		TArray<EItemGrade> Grades;
		TArray<float> GradeWeights;
		GradeWeightsFor(*ByGrade, Grades, GradeWeights);
		if (Grades.Num() > 0)
		{
			Categories.Add(W.Key);
			CategoryWeights.Add(W.Value);
		}
	}

	if (Categories.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot: 뽑을 수 있는 종류·등급 없음: %s"), *GetName());
		return;
	}

	// 종류 → (그 종류 안에서) 등급 → 아이템 하나.
	auto AddOne = [this, &Pools, &Categories, &CategoryWeights, &GradeWeightsFor]() -> bool
	{
		const int32 CategoryIndex = HBBoxLoot::PickWeightedIndex(CategoryWeights, FMath::FRand());
		if (CategoryIndex == INDEX_NONE)
		{
			return false;
		}
		const TMap<EItemGrade, TArray<FLootCandidate>>& ByGrade = Pools.FindChecked(Categories[CategoryIndex]);

		TArray<EItemGrade> Grades;
		TArray<float> GradeWeights;
		GradeWeightsFor(ByGrade, Grades, GradeWeights);
		const int32 GradeIndex = HBBoxLoot::PickWeightedIndex(GradeWeights, FMath::FRand());
		if (GradeIndex == INDEX_NONE)
		{
			return false;
		}
		const TArray<FLootCandidate>& Pool = ByGrade.FindChecked(Grades[GradeIndex]);

		const FLootCandidate& Candidate = Pool[FMath::RandRange(0, Pool.Num() - 1)];

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
		if (AddOne())
		{
			++AddedCount;
		}
	}

	if (AddedCount == 0)
	{
		if (AddOne())
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
