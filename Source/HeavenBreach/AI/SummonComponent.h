// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SummonComponent.generated.h"

class AEnemyCharacterBase;

/**
 * 소환한 적들의 명부. 정원을 세고, 주인이 죽으면 같이 죽인다.
 *
 * 왜 어빌리티가 아니라 컴포넌트인가: 소환 GA 는 InstancedPerActor 라 목록을
 * 들 수는 있지만, 소환 어빌리티가 둘 이상이 되면 정원이 따로 세어진다.
 * 명부는 개체당 하나여야 한다.
 *
 * 붙이는 법: 아키타입 데이터의 ExtraComponents 에 넣는다. UGrabComponent 와 같다.
 * 소환수 쪽 데이터에는 넣지 말 것. 소환수가 또 소환하면 정원이 통제되지 않는다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API USummonComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USummonComponent();

	// 현재 살아있는 소환수 수. 죽었거나 사라진 항목을 걸러내며 센다
	UFUNCTION(BlueprintPure, Category = "Summon")
	int32 GetAliveCount();

	// 명부에 올린다
	void Register(AEnemyCharacterBase* Summoned);

	// 전부 죽인다. 주인이 죽으면 불린다
	UFUNCTION(BlueprintCallable, Category = "Summon")
	void KillAll();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleOwnerDied(AEnemyCharacterBase* DeadOwner);

	// 약한 참조인 이유: 소환수는 죽고 나서 스스로 사라진다.
	// 강한 참조로 잡으면 명부가 시체를 붙들어 메모리에 남긴다.
	TArray<TWeakObjectPtr<AEnemyCharacterBase>> Alive;
};
