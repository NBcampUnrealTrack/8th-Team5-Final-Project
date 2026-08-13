// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeTraceComponent.generated.h"


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
	void BeginTrace(const TArray<FName>& InSockets, TSubclassOf<UGameplayEffect> InDamageEffect, float InRadius);

	// 노티파이가 매 틱 호출. 실제 판정
	void UpdateTrace();

	// 노티파이가 호출. 다음 사용을 위해 변수 정리
	void EndTrace();

protected:
	virtual void BeginPlay() override;

	// 트레이스 과정에서 사용될 구의 반지름
	UPROPERTY(EditAnywhere, Category = "Trace", meta = (ClampMin = "1.0"))
	float TraceRadius = 30.f;

	// 만들어둔 채널 사용
	UPROPERTY(EditAnywhere, Category = "Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel1;


	// ----- 디버깅 -----
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, Category = "Debug")
	float DebugDrawTime = 0.5f;

private:
	// 데미지 적용
	void ApplyDamage(AActor* Target, const FHitResult& Hit);

	// 이번 구간의 소켓. 노티파이가 넘겨준 것
	TArray<FName> ActiveSockets;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> ActiveDamageEffect;

	// 이번 구간에서 이미 때린 대상. 없으면 수십 번 맞는다.
	TSet<TWeakObjectPtr<AActor>> HitActors;

	bool bTracing = false;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedMesh;
};
