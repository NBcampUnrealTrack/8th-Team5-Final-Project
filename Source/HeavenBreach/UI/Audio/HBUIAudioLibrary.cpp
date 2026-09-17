#include "UI/Audio/HBUIAudioLibrary.h"

#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/Settings/HBSettingsSubsystem.h"

UHBUIAudioLibrary::UHBUIAudioLibrary()
{
	const TCHAR* Names[] = {
		TEXT("SFX_UI_Hover"), TEXT("SFX_UI_Click"), TEXT("SFX_UI_Confirm"),
		TEXT("SFX_UI_Cancel"), TEXT("SFX_UI_Open"), TEXT("SFX_UI_Close"), TEXT("SFX_UI_Error"),
		TEXT("SFX_Item_Move"), TEXT("SFX_Item_Equip"), TEXT("SFX_Trade_Success"),
		TEXT("SFX_Craft_Success"), TEXT("SFX_Lobby_Ready")};
	static_assert(UE_ARRAY_COUNT(Names) == static_cast<uint8>(EHBUIAudioEvent::Count));
	for (const TCHAR* Name : Names)
	{
		const FString Path = FString::Printf(TEXT("/Game/UI/Audio/%s.%s"), Name, Name);
		ConstructorHelpers::FObjectFinder<USoundBase> Sound(*Path);
		Sounds.Add(Sound.Object);
	}
}

USoundBase* UHBUIAudioLibrary::GetSound(EHBUIAudioEvent Event)
{
	const UHBUIAudioLibrary* Library = GetDefault<UHBUIAudioLibrary>();
	const int32 Index = static_cast<int32>(Event);
	return Library->Sounds.IsValidIndex(Index) ? Library->Sounds[Index].Get() : nullptr;
}

void UHBUIAudioLibrary::PlayForOwner(const UObject* OwnerContext, EHBUIAudioEvent Event)
{
	if (!IsValid(OwnerContext)) return;
	const UWorld* World = OwnerContext->GetWorld();
	if (!World || !World->IsGameWorld() || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer) return;
	if (const UGameInstance* Instance = World->GetGameInstance())
	{
		if (const UHBSettingsSubsystem* Settings = Instance->GetSubsystem<UHBSettingsSubsystem>())
		{
			// A newly muted setting takes effect immediately, including its own confirmation.
			if (Settings->GetMasterVolume() <= 0.f || Settings->GetEffectsVolume() <= 0.f) return;
		}
	}

	const APlayerController* Player = nullptr;
	if (const UUserWidget* Widget = Cast<UUserWidget>(OwnerContext))
	{
		Player = Widget->GetOwningPlayer();
	}
	const AActor* Actor = Cast<AActor>(OwnerContext);
	if (const UActorComponent* Component = Cast<UActorComponent>(OwnerContext)) Actor = Component->GetOwner();
	for (const AActor* Current = Actor; Current && !Player; Current = Current->GetOwner())
	{
		Player = Cast<APlayerController>(Current);
		if (const APawn* Pawn = Cast<APawn>(Current)) Player = Cast<APlayerController>(Pawn->GetController());
		if (const APlayerState* State = Cast<APlayerState>(Current)) Player = State->GetPlayerController();
	}
	if (Player && Player->IsLocalController())
	{
		UGameplayStatics::PlaySound2D(Player, GetSound(Event), 1.f, 1.f, 0.f, nullptr, Player, true);
	}
}
