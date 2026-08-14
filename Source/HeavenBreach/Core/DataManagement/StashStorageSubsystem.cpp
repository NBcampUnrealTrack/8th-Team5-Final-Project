#include "StashStorageSubsystem.h"

#include "MemoryStashStorage.h"

void UStashStorageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Storage = MakeShared<FMemoryStashStorage>();

	if (!Storage->Initialize())
	{
		UE_LOG(LogTemp, Error, TEXT("Stash: storage initialization failed"));
		Storage.Reset();
	}
}

void UStashStorageSubsystem::Deinitialize()
{
	if (Storage.IsValid())
	{
		Storage->Shutdown();
		Storage.Reset();
	}

	Super::Deinitialize();
}