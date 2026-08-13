// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableInterface.h"
#include "ExtractionZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class APawn;

/**
 *  탈출(추출) 지점.
 *  - IInteractableInterface 구현 → 박스와 동일하게 UInteractionComponent가 F로 상호작용.
 *  - F 상호작용 시 서버에서 채널링(ExtractionDuration) 시작. 존에 머무는 동안 진행, 벗어나면 취소.
 *  - 완료 시 GameMode.NotifyPlayerExtracted 호출 + 폰 숨김/충돌 해제(입력 잠금은 BP/후속).
 *
 *  실제 배치는 이 클래스를 상속한 Blueprint(BP_ExtractionZone)로 메시/존 크기를 지정해 사용한다.
 */
UCLASS(Blueprintable)
class HEAVENBREACH_API AExtractionZone : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:

	AExtractionZone();

	//~ Begin IInteractableInterface
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	//~ End IInteractableInterface

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 탈출 지점 비주얼 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 탈출 판정 존 (여기 머무는 동안만 채널링 유지) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Zone;

	/** 채널링 시간(초). F 상호작용 후 이 시간 동안 존에 머물면 탈출 완료. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction", meta = (ClampMin = 0, Units = "s"))
	float ExtractionDuration = 5.0f;

	/** 상호작용 프롬프트 문구 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Extraction")
	FText InteractPrompt = FText::FromString(TEXT("탈출 [F]"));

	/** 채널링 플레이어가 존을 벗어나면 취소 */
	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 채널링 시작 연출/진행바 훅 (BP). 클라 진행 UI는 후속으로 연결. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction", meta = (DisplayName = "On Extract Started"))
	void BP_OnExtractStarted(APawn* Player, float Duration);

	/** 채널링 취소 훅 (BP) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction", meta = (DisplayName = "On Extract Canceled"))
	void BP_OnExtractCanceled(APawn* Player);

	/** 채널링 완료(탈출) 훅 (BP) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Extraction", meta = (DisplayName = "On Extract Completed"))
	void BP_OnExtractCompleted(APawn* Player);

private:

	/** [서버] 채널링 중인 플레이어별 타이머 */
	TMap<APawn*, FTimerHandle> ChannelingPlayers;

	/** 폰이 지금 이 존 안에 있는지 */
	bool IsPawnInZone(const APawn* Pawn) const;

	/** [서버] 채널링 시작 */
	void StartChannel(APawn* Pawn);

	/** [서버] 채널링 취소 */
	void CancelChannel(APawn* Pawn);

	/** [서버] 채널링 완료 → 탈출 처리 (타이머 콜백) */
	void CompleteChannel(APawn* Pawn);

	/** [서버] 실제 탈출 처리: GameMode 통지 + 폰 숨김/충돌 해제 */
	void ExtractPawn(APawn* Pawn);
};
