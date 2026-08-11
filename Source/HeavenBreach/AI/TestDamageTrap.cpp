// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/TestDamageTrap.h"

#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "GameplayEffect.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemGlobals.h"
#include "AI/Enum/EAlertState.h"

// Sets default values
ATestDamageTrap::ATestDamageTrap()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetBoxExtent(FVector(100.f));
	SetRootComponent(TriggerBox);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATestDamageTrap::OnOverlapBegin);
}

void ATestDamageTrap::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 판정은 서버 전용.
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (!ASC)
	{
		return;
	}

	// 데미지 이펙트 사용방식
	if (DamageEffect)
	{
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DamageEffect, 1.f, ASC->MakeEffectContext());

		// 대상이 스스로 피해 입도록 만듬
		if (Spec.IsValid())
		{
			// 대상(적일거임)은 이펙트에 따라 피해처리
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// 어빌리티 발동
	if (!AbilityTagsToActivate.IsEmpty())
	{
		// 대상(적)은 어빌리티를 발동 하도록 함(아마 공격몽타주여야할것)
		bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityTagsToActivate);
		UE_LOG(LogEnemyAI, Log, TEXT("[GAS] %s 어빌리티 발동 %s"), *GetNameSafe(OtherActor), bActivated ? TEXT("성공") : TEXT("실패"));
	}
}

