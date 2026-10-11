// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AI/GAS/EnemyMontageGABase.h"
#include "EnemySummonGABase.generated.h"

class AEnemyCharacterBase;
class USummonComponent;

/**
 * 한 번의 소환에서 무엇을 몇 마리 꺼낼지. 이게 한 줄이다.
 */
USTRUCT(BlueprintType)
struct FEnemySummonEntry
{
	GENERATED_BODY()

	// 에디터 표시용. 동작에는 영향 없다
	UPROPERTY(EditDefaultsOnly, Category = "Summon")
	FName EntryName = TEXT("Summon");

	UPROPERTY(EditDefaultsOnly, Category = "Summon")
	TSubclassOf<AEnemyCharacterBase> SummonClass;

	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/**
 * 잡몹을 소환하는 GA.
 *
 * 몽타주의 노티파이 두 개가 흐름을 만든다.
 *   Event.Summon.Mark   지점을 정하고 예고 이펙트를 띄운다
 *   Event.Summon.Spawn  그 지점에 실제로 꺼낸다
 * 둘 사이의 간격이 곧 플레이어가 피할 수 있는 시간이다. 그래서 코드에 딜레이가 없다.
 *
 * 시전자 본인의 이펙트는 여기서 하지 않는다. 위치가 뻔하므로 몽타주에 ANS 로 붙인다.
 * GA 가 띄우는 건 매번 달라지는 소환 지점뿐이다.
 */
UCLASS()
class HEAVENBREACH_API UEnemySummonGABase : public UEnemyMontageGABase
{
	GENERATED_BODY()

public:
	// 정원이 찼으면 발동 자체를 막는다.
	//
	// 왜 여기인가: TryActivateWeightedAbilityByTag 가 후보를 추릴 때 이걸 부른다.
	// 여기서 false 를 주면 소환이 가중치 추첨에 아예 안 들어가 다른 패턴이 대신 나간다.
	// Mark 노티파이에서 막으면 이미 긴 소환 몽타주가 다 나간 뒤라, 보스가 허공에
	// 손짓만 하고 쿨타임까지 먹는다.
	//
	// 쳐낼 조건: 정원이 차도 연출은 보여주고 싶다면 이 오버라이드를 지우면
	// 예고가 안 뜨는 기존 동작으로 돌아간다.
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UFUNCTION()
	void OnMarkEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnSpawnEventReceived(FGameplayEventData Payload);

	// 한 번에 꺼낼 구성. 위에 있는 줄이 먼저 자리를 가져간다.
	//
	// 왜 배열인가: "근접 2 + 원거리 1" 같은 조합이 보스 패턴의 성격을 만든다.
	// 클래스 하나에 개수만 두면 그 조합을 표현할 수 없다.
	//
	// 쳐낼 조건: 매번 같은 구성이 지겨워지면 줄마다 Min/Max 로 나누면 된다.
	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (TitleProperty = "EntryName"))
	TArray<FEnemySummonEntry> SummonEntries;

	// 동시 생존 상한. 이게 없으면 보스전이 터진다
	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "1"))
	int32 MaxAlive = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "0.0"))
	float SpawnRadiusMin = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "0.0"))
	float SpawnRadiusMax = 600.f;

	// 네비메시로 끌어올릴 때 허용 오차. 좁은 방이면 줄인다
	UPROPERTY(EditDefaultsOnly, Category = "Summon")
	FVector NavProjectExtent = FVector(200.f, 200.f, 500.f);

	// 소환수가 죽고 나서 사라지기까지
	UPROPERTY(EditDefaultsOnly, Category = "Summon", meta = (ClampMin = "0.0"))
	float DespawnDelay = 3.f;

	// 소환 지점 예고 큐. 위치를 넘겨야 해서 몽타주가 아니라 여기서 띄운다
	UPROPERTY(EditDefaultsOnly, Category = "Summon")
	FGameplayTag MarkCueTag;

	// 실제로 튀어나오는 순간의 큐
	UPROPERTY(EditDefaultsOnly, Category = "Summon")
	FGameplayTag SpawnCueTag;

	// 켜면 소환 위치에 보라색 구가 그려진다.
	// 기본은 꺼 둔다. 플레이 중 화면을 가린다는 팀 요청. 소환 위치를 확인할 때만 BP 에서 켠다.
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDrawDebug = false;

private:
	USummonComponent* GetSummonComp() const;

	// 주변의 네비메시 위 한 점. 못 찾으면 false
	bool FindSpawnPoint(const FVector& Origin, FVector& OutPoint) const;

	// 위치를 큐에 실어 보낸다. 태그가 비어 있으면 아무것도 안 한다
	void PlayCueAt(const FGameplayTag& CueTag, const FVector& Location);

	// Mark 에서 정하고 Spawn 에서 쓴다.
	//
	// 왜 클래스가 아니라 인덱스인가: UPROPERTY 가 아닌 배열에 UClass 포인터를
	// 담으면 GC 를 신경 써야 한다. SummonEntries 는 EditDefaultsOnly 라
	// 두 노티파이 사이에 바뀌지 않으므로 인덱스가 안전하고 더 싸다.
	struct FSummonPoint
	{
		int32 EntryIndex = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
	};

	TArray<FSummonPoint> PendingPoints;
};
