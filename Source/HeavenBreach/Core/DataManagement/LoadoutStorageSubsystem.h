#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoadoutStorageSubsystem.generated.h"

class ILoadoutStorage;

UCLASS()
class HEAVENBREACH_API ULoadoutStorageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TSharedPtr<ILoadoutStorage> GetStorage() const { return Storage; }

private:
	TSharedPtr<ILoadoutStorage> Storage;
};