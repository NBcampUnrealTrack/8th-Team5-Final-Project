// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TestDamageTrap.generated.h"

class UBoxComponent;
class UGameplayEffect;

UCLASS()
class HEAVENBREACH_API ATestDamageTrap : public AActor
{
	GENERATED_BODY()
	
public:
	ATestDamageTrap();

protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<UBoxComponent> TriggerBox;

	// BP 에서 Damage 를 지정한다. 
	UPROPERTY(EditAnywhere, Category = "Trap")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 비어 있지 않으면 이 태그의 어빌리티를 발동시킨다. 테스트용
	UPROPERTY(EditAnywhere, Category = "Trap")
	FGameplayTagContainer AbilityTagsToActivate;
};
