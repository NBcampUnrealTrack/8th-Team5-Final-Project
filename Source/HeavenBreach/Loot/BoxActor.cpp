// Copyright HeavenBreach Team5. All Rights Reserved.


#include "BoxActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

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
	// TODO(loot): FLootTableRow + DataTable 기반 가중치 랜덤 추첨(최소 1개 보장) 후
	//             상자 UInventoryComponent에 아이템을 추가한다. 서버 전용.
	UE_LOG(LogTemp, Warning, TEXT("[BoxActor] GenerateLoot() 미구현 - Loot/Inventory 연동 필요: %s"), *GetName());
}

void ABoxActor::OpenContainerUI(AActor* Interactor)
{
	// TODO(inventory/ui): 상자 인벤토리 참조를 담아 Interactor에게 Client RPC로 이중 UI를 열고,
	//                     GAS 잠금 태그(State.Interacting.Container)를 부여해 이동/공격을 차단한다.
	UE_LOG(LogTemp, Warning, TEXT("[BoxActor] OpenContainerUI() 미구현 - Inventory/UI 연동 필요: %s -> %s"),
		*GetName(), *GetNameSafe(Interactor));
}
