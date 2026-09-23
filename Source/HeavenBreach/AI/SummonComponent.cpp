// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/SummonComponent.h"

#include "AI/EnemyCharacterBase.h"
#include "AI/Enum/EAlertState.h"

USummonComponent::USummonComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USummonComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소환 자체가 서버에서만 일어난다. 클라에는 명부가 필요 없다.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(GetOwner());
	if (!Enemy)
	{
		UE_LOG(LogEnemyAI, Warning, TEXT("[Summon] 명부가 적이 아닌 액터에 붙었다: %s"), *GetNameSafe(GetOwner()));
		return;
	}

	Enemy->OnDied.AddUObject(this, &USummonComponent::HandleOwnerDied);
}

void USummonComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AEnemyCharacterBase* Enemy = Cast<AEnemyCharacterBase>(GetOwner()))
	{
		Enemy->OnDied.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

int32 USummonComponent::GetAliveCount()
{
	// 뒤에서부터 지우면 인덱스가 밀리지 않는다
	for (int32 i = Alive.Num() - 1; i >= 0; --i)
	{
		AEnemyCharacterBase* S = Alive[i].Get();
		if (!S)
		{
			Alive.RemoveAtSwap(i);
			continue;
		}

		if (S->IsDead())
		{
			Alive.RemoveAtSwap(i);
			continue;
		}
	}

	return Alive.Num();
}

int32 USummonComponent::CountAlive() const
{
	int32 Count = 0;

	for (const TWeakObjectPtr<AEnemyCharacterBase>& Ptr : Alive)
	{
		const AEnemyCharacterBase* S = Ptr.Get();
		if (!S)
		{
			continue;
		}

		if (S->IsDead())
		{
			continue;
		}

		++Count;
	}

	return Count;
}

void USummonComponent::Register(AEnemyCharacterBase* Summoned)
{
	if (!Summoned)
	{
		return;
	}

	Alive.Add(Summoned);
}

void USummonComponent::KillAll()
{
	// 사본을 도는 이유: BecomeDead 가 타이머를 걸고 나중에 Destroy 한다.
	// 그 과정에서 명부가 흔들려도 순회가 깨지지 않게 한다.
	TArray<TWeakObjectPtr<AEnemyCharacterBase>> Snapshot = Alive;
	Alive.Reset();

	int32 Killed = 0;
	for (const TWeakObjectPtr<AEnemyCharacterBase>& Ptr : Snapshot)
	{
		AEnemyCharacterBase* S = Ptr.Get();
		if (!S)
		{
			continue;
		}

		if (S->IsDead())
		{
			continue;
		}

		// 사라지는 게 아니라 죽는다. 사망 몽타주가 나오고 나서 소멸 타이머를 탄다
		S->BecomeDead();
		++Killed;
	}

	UE_LOG(LogEnemyAI, Log, TEXT("[Summon] %s 사망 — 소환수 %d마리 정리"), *GetNameSafe(GetOwner()), Killed);
}

void USummonComponent::HandleOwnerDied(AEnemyCharacterBase* DeadOwner)
{
	KillAll();
}
