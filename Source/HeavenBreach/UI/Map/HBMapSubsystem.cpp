#include "UI/Map/HBMapSubsystem.h"
#include "UI/Map/HB_MapWindow.h"
#include "UI/HB_UILayout.h"
#include "Core/Tag/UI/HB_UITag.h"
#include "Player/HBPlayerController.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

UHBMapSubsystem::UHBMapSubsystem()
{
	MapWindowClass = TSoftClassPtr<UHB_MapWindow>(FSoftObjectPath(TEXT("/Game/UI/Map/WBP_MapWindow.WBP_MapWindow_C")));
}

bool UHBMapSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

TStatId UHBMapSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHBMapSubsystem, STATGROUP_Tickables);
}

bool UHBMapSubsystem::CanOpenMap() const
{
	for (TActorIterator<AHBDungeonRuntime> It(GetWorld()); It; ++It)
	{
		return It->IsReady();
	}
	return false;
}

void UHBMapSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || Window.IsValid())
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->IsLocalController() && PC->GetPawn() && PC->WasInputKeyJustPressed(EKeys::M))
	{
		ToggleMap();
	}
}

void UHBMapSubsystem::ToggleMap()
{
	if (Window.IsValid())
	{
		Window->DeactivateWidget();
		Window.Reset();
		return;
	}
	if (!CanOpenMap())
	{
		return;
	}
	AHBPlayerController* PC = Cast<AHBPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC || !PC->GetUILayout())
	{
		return;
	}
	TSubclassOf<UHB_MapWindow> WindowClass = MapWindowClass.LoadSynchronous();
	if (!WindowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("HBMap: map window class not found (%s)"), *MapWindowClass.ToString());
		return;
	}
	UHB_MapWindow* NewWindow = PC->GetUILayout()->PushWidgetToLayer<UHB_MapWindow>(UITag::TAG_UI_Layer_Menu, WindowClass);
	if (!NewWindow)
	{
		return;
	}
	Window = NewWindow;
	NewWindow->OnDeactivated().AddWeakLambda(this, [this]()
		{
			Window.Reset();
		});
}
