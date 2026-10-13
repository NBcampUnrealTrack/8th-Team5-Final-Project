#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HBUIAudioLibrary.generated.h"

class USoundBase;

UENUM(BlueprintType)
enum class EHBUIAudioEvent : uint8
{
	Hover, Click, Confirm, Cancel, Open, Close, Error,
	ItemMove, ItemEquip, TradeSuccess, CraftSuccess, LobbyReady,
	WeaponBreak, PortalTravel,
	Count UMETA(Hidden)
};

/** Shared, cooked sound references. Gameplay outcomes are played only for their owning local player. */
UCLASS()
class HEAVENBREACH_API UHBUIAudioLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UHBUIAudioLibrary();

	UFUNCTION(BlueprintCallable, Category = "UI|Audio")
	static void PlayForOwner(const UObject* OwnerContext, EHBUIAudioEvent Event);

	UFUNCTION(BlueprintPure, Category = "UI|Audio")
	static USoundBase* GetSound(EHBUIAudioEvent Event);

	/** Call only after a server-authoritative outcome; PlayerController delivers the sound once to its owner. */
	static void PlayForOwningClient(const UObject* OwnerContext, EHBUIAudioEvent Event);

private:
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Sounds;
};
