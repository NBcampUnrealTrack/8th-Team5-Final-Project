// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MeleeTraceComponent.generated.h"

class UAnimMontage;
class UGameplayEffect;
class USkeletalMeshComponent;


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HEAVENBREACH_API UMeleeTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMeleeTraceComponent();

	// 노티파이가 호출. 어디를 검사할지와 무엇을 적용할지 받는다
	void BeginTrace(const TArray<FName>& InSockets, TSubclassOf<UGameplayEffect> InDamageEffect, float InRadius, float InDamageMultiplier, bool bInUseHitStop, float InHitStopDuration, float InHitStopPlayRate);

	// 노티파이가 매 틱 호출. 실제 판정
	void UpdateTrace();

	// 노티파이가 호출. 다음 사용을 위해 변수 정리
	void EndTrace();

	// 클라이언트 히트스탑 호출용도.
	UFUNCTION(Client, Unreliable)
	void ClientHitStop(float Duration, float PlayRate);

protected:
	virtual void BeginPlay() override;

	// 트레이스 과정에서 사용될 구의 반지름
	UPROPERTY(EditAnywhere, Category = "Trace", meta = (ClampMin = "1.0"))
	float TraceRadius = 30.f;

	// 해당 트레이스의 데미지 배율
	UPROPERTY(EditAnywhere, Category = "Trace")
	float DamageMultiplier = 1.f;

	// 만들어둔 채널 사용
	UPROPERTY(EditAnywhere, Category = "Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel3;

	// 소유자의 팀 태그
	UPROPERTY(EditAnywhere, Category = "Trace")
	FGameplayTagContainer OwnerTeamTag;

	// 이 태그가 붙은 컴포넌트(무기 메쉬)의 소켓을 먼저 본다.
	// 소켓이나 무기가 없으면 캐릭터 메쉬에서 본다.
	UPROPERTY(EditAnywhere, Category = "Trace")
	FName WeaponComponentTag = TEXT("Weapon");

	// ----- 히트스톱 -----
	// 켜고 끄기는 노티파이가 정한다. 여기 값은 캐릭터별 튜닝용.

	// 정지 시간
	UPROPERTY(EditAnywhere, Category = "HitStop", meta = (ClampMin = "0.0"))
	float HitStopDuration = 0.4f;
	// 정지 시 배율
	UPROPERTY(EditAnywhere, Category = "HitStop", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float HitStopPlayRate = 0.05f;


	// ----- 디버깅 -----
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, Category = "Debug")
	float DebugDrawTime = 0.5f;

private:
	// 데미지 적용. 실제로 때렸으면 true.
	//
	// 왜 반환값이 있나: 벽이나 바닥처럼 Damageable 이 아닌 것도 트레이스에는 걸린다.
	// 그걸 "한 대 때렸다" 로 세면 히트스톱이 걸리고 스윙이 거기서 끝나서
	// 뒤에 있는 진짜 적을 못 때린다.
	bool ApplyDamage(AActor* Target, const FHitResult& Hit);

	// 지정된 소켓이 있는 컴포넌트를 고른다. 하위 자식 메시 -> 캐릭터 메쉬 순서.
	USceneComponent* ResolveSocketSource() const;

	// 적중햇으면 몽타주를 느리게 만들어 히트했다는걸 알기쉽게함.
	UFUNCTION()
	void TriggerHitStop();

	// 정지된 몽타주 다시 원복
	UFUNCTION()
	void EndHitStop();

	// 이번 구간의 소켓. 노티파이가 넘겨준 것
	TArray<FName> ActiveSockets;

	// 이번 구간에 실제로 소켓을 읽을 컴포넌트
	TWeakObjectPtr<USceneComponent> ActiveSocketSource;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> ActiveDamageEffect;

	// 이번 구간에서 이미 때린 대상. 없으면 수십 번 맞는다.
	TSet<TWeakObjectPtr<AActor>> HitActors;

	bool bTracing = false;

	// 스윙 1회당 무기 내구도 1만 깎이게 하는 플래그. 여러 명을 맞혀도 한 번만 깎여야 한다.
	bool bDurabilityConsumedThisSwing = false;

	// 히트스톱관련 저장될 정보들
	bool bUseHitStop = false;
	bool bHitStopActive = false;
	float SavedPlayRate = 1.f;
	TWeakObjectPtr<UAnimMontage> HitStopMontage;
	FTimerHandle HitStopTimer;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedMesh;
};
