#include "StashStorageSubsystem.h"

#include "BackendStashStorage.h"
#include "MemoryStashStorage.h"

void UStashStorageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Storage = MakeShared<FBackendStashStorage>();
	// 배포서버가 작동하지 않을때 아래 코드로 진행
	// Storage = MakeShared<FMemoryStashStorage>();

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