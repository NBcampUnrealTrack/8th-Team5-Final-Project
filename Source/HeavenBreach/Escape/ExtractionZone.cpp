// Copyright HeavenBreach Team5. All Rights Reserved.


#include "ExtractionZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Core/GameFlow/HeavenBreachGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"

AExtractionZone::AExtractionZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(FName("BlockAll"));

	// 탈출 판정 존: Pawn 오버랩만 감지 (채널링 유지/취소 판단용)
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetupAttachment(Mesh);
	Zone->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionObjectType(ECC_WorldStatic);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AExtractionZone::BeginPlay()
{
	Super::BeginPlay();

	// 채널링 취소 판정을 위해 존 오버랩 종료를 구독 (서버에서만 판정)
	if (HasAuthority())
	{
		Zone->OnComponentEndOverlap.AddDynamic(this, &AExtractionZone::OnZoneEndOverlap);
	}
}

void AExtractionZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 진행 중인 타이머 정리
	if (UWorld* World = GetWorld())
	{
		for (TPair<APawn*, FTimerHandle>& Pair : ChannelingPlayers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	ChannelingPlayers.Empty();

	Super::EndPlay(EndPlayReason);
}

bool AExtractionZone::CanInteract_Implementation(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	if (Pawn == nullptr)
	{
		return false;
	}

	// 존 안에 있고, 아직 채널링 중이 아닐 때만 시작 가능. (매치 진행 여부는 서버 Interact에서 재확인)
	return IsPawnInZone(Pawn) && !ChannelingPlayers.Contains(const_cast<APawn*>(Pawn));
}

void AExtractionZone::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Interactor);
	if (Pawn == nullptr || !IsPawnInZone(Pawn) || ChannelingPlayers.Contains(Pawn))
	{
		return;
	}

	// 매치 진행 중일 때만 탈출 허용
	const AHeavenBreachGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		return;
	}

	StartChannel(Pawn);
}

FText AExtractionZone::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}

void AExtractionZone::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 채널링 중인 플레이어가 존을 벗어나면 취소
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (ChannelingPlayers.Contains(Pawn))
		{
			CancelChannel(Pawn);
		}
	}
}

bool AExtractionZone::IsPawnInZone(const APawn* Pawn) const
{
	return Pawn != nullptr && Zone != nullptr && Zone->IsOverlappingActor(Pawn);
}

void AExtractionZone::StartChannel(APawn* Pawn)
{
	FTimerHandle& Handle = ChannelingPlayers.Add(Pawn);

	if (ExtractionDuration <= 0.0f)
	{
		// 즉시 완료
		CompleteChannel(Pawn);
		return;
	}

	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AExtractionZone::CompleteChannel, Pawn);
	GetWorld()->GetTimerManager().SetTimer(Handle, Del, ExtractionDuration, false);

	// 연출/진행바 훅 (서버). 클라 진행 UI는 후속 연결.
	BP_OnExtractStarted(Pawn, ExtractionDuration);
}

void AExtractionZone::CancelChannel(APawn* Pawn)
{
	if (FTimerHandle* Handle = ChannelingPlayers.Find(Pawn))
	{
		GetWorld()->GetTimerManager().ClearTimer(*Handle);
		ChannelingPlayers.Remove(Pawn);
		BP_OnExtractCanceled(Pawn);
	}
}

void AExtractionZone::CompleteChannel(APawn* Pawn)
{
	// 타이머 콜백. 폰이 유효하고 여전히 채널링 중일 때만 진행.
	if (!ChannelingPlayers.Contains(Pawn))
	{
		return;
	}
	ChannelingPlayers.Remove(Pawn);

	if (!IsValid(Pawn))
	{
		return;
	}

	ExtractPawn(Pawn);
	BP_OnExtractCompleted(Pawn);
}

void AExtractionZone::ExtractPawn(APawn* Pawn)
{
	// 매치 시스템에 탈출 통지
	if (AHeavenBreachGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHeavenBreachGameMode>() : nullptr)
	{
		GameMode->NotifyPlayerExtracted(Pawn->GetPlayerState());
	}

	// 폰 숨김 + 충돌 해제 (bHidden은 복제되어 모든 클라에서 사라짐)
	// TODO(player): 입력 잠금은 소유 클라에서 처리 필요 (PlayerController DisableInput 등) — Player 담당과 연동.
	Pawn->SetActorHiddenInGame(true);
	Pawn->SetActorEnableCollision(false);
}
