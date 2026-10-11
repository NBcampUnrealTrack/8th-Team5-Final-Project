// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GrabComponent.generated.h"

class UGameplayEffect;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct FGrabSettings
{
	GENERATED_BODY()

	// 인접한 소켓끼리 이어서 검사한다. 최소 2개.
	UPROPERTY(EditAnywhere, Category = "Grab")
	TArray<FName> Sockets;

	UPROPERTY(EditAnywhere, Category = "Grab", meta = (ClampMin = "1.0"))
	float TraceRadius = 60.f;

	// 공격채널. 어지간하면 이거고정
	UPROPERTY(EditAnywhere, Category = "Grab")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel3;

	// 잡은 대상을 소켓들의 중점으로 옮긴다. 끄면 이동만 막고 제자리에 둔다.
	UPROPERTY(EditAnywhere, Category = "Grab")
	bool bSnapToHoldLocation = true;

	// 켜면 소켓 높이까지 맞춰 들어올린다. 끄면 대상의 원래 높이를 유지한다.
	UPROPERTY(EditAnywhere, Category = "Grab")
	bool bMatchSocketHeight = false;

	// 소켓 중점에서 소유자 정면으로 이만큼 떨어뜨린다. 음수면 안쪽으로 당긴다.
	UPROPERTY(EditAnywhere, Category = "Grab")
	float HoldForwardOffset = 0.f;

	// 잡힌 대상을 잡힘 상태이상으로 이동속도를 0으로 만드는 GE
	UPROPERTY(EditAnywhere, Category = "Grab")
	TSubclassOf<UGameplayEffect> GrabbedEffect;

	// 공격타입 태그. 사운드,이펙트GC출력용
	UPROPERTY(EditAnywhere, Category = "Grab")
	FGameplayTag ImpactTypeTag;

	// ----- 흡혈 -----
	// 물기 시 회복할 GE
	UPROPERTY(EditAnywhere, Category = "Heal")
	TSubclassOf<UGameplayEffect> BiteHealEffect;

	// 대상 체력 어트리뷰트가 잇어야 적용가능...
	UPROPERTY(EditAnywhere, Category = "Heal")
	FGameplayAttribute TargetHealthAttribute;

	// 피해 흡수율.
	UPROPERTY(EditAnywhere, Category = "Heal", meta = (ClampMin = "0.0"))
	float LifestealRatio = 1.f;

	// GE에서 건드릴 대상의 스텟 태그가 일치해야함
	UPROPERTY(EditAnywhere, Category = "Heal")
	FGameplayTag HealMagnitudeTag;

	// ----- 디버깅 -----
	// 기본은 꺼 둔다. 플레이 중 화면을 가린다는 팀 요청.
	// 잡기 판정 범위를 확인할 때만 BP 의 컴포넌트 디테일에서 켠다.
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, Category = "Debug")
	float DebugDrawTime = 0.5f;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HEAVENBREACH_API UGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrabComponent();

	// 범위 내에서 한 플레이어를 잡는다.
	UFUNCTION(BlueprintCallable, Category = "Grab")
	bool TryGrab(const FGrabSettings& InSettings);

	// 실제 물기공격을 시행한다. 잡힌 대상이 있어야 한다.
	UFUNCTION(BlueprintCallable, Category = "Grab")
	void BiteGrabbed(float DamageMultiplier);

	// 구속을 해제한다. 
	UFUNCTION(BlueprintCallable, Category = "Grab")
	void ReleaseGrab();

	// 잡은 대상이 무엇인지 알기용
	UFUNCTION(BlueprintCallable, Category = "Grab")
	AActor* GetGrabbedActor() const { return GrabbedActor.Get(); }

protected:
	virtual void BeginPlay() override;

private:
	// 팀 소속 확인용
	void CacheOwnerTeamTag();
	// 공격에 유효한 타겟인지 확인용
	bool Isenemy(const AActor* Target) const;
	// 공격자의 공격력 확인용. 공격배율까지합산해서 최종데미지 가함
	float GetOwnerAttackPower() const;
	// 대상의 체력. 가한피해만큼 체력흡수를 위해사용.
	float GetTargetHealth(const AActor* Target, bool& bOutFound) const;
	// 잡은 대상을 둘 소켓들의 평균지점.
	bool GetHoldLocation(FVector& OutLocation) const;

	// 노티파이가 넘겨준 설정
	FGrabSettings Settings;

	// 소유자의 팀 태그. 잡을 때 계산한다.
	FGameplayTagContainer OwnerTeamTag;

	TWeakObjectPtr<AActor> GrabbedActor;
	FActiveGameplayEffectHandle GrabbedHandle;

	// 잡을 때의 히트정보. 물기마다 재질,컴포넌트를 재사용한다.
	FHitResult GrabHit;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedMesh;
};
