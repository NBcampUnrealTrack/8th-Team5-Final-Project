#include "LoadoutStorageSubsystem.h"

#include "BackendLoadoutStorage.h"

void ULoadoutStorageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Storage = MakeShared<FBackendLoadoutStorage>();

	if (!Storage->Initialize())
	{
		UE_LOG(LogTemp, Error, TEXT("Loadout: storage initialization failed"));
		Storage.Reset();
	}
}

void ULoadoutStorageSubsystem::Deinitialize()
{
	if (Storage.IsValid())
	{
		Storage->Shutdown();
		Storage.Reset();
	}

	Super::Deinitialize();
}