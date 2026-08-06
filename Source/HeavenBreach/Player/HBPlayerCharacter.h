#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HBPlayerCharacter.generated.h"

class UCameraComponent;

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHBPlayerCharacter();

	void MoveForward(float Value);
	void MoveRight(float Value);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
};
