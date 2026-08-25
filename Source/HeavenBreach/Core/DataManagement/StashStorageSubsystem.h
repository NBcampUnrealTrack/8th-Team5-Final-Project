#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StashStorageSubsystem.generated.h"

class IStashStorage;

UCLASS()
class HEAVENBREACH_API UStashStorageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TSharedPtr<IStashStorage> GetStorage() const { return Storage; }

private:
	TSharedPtr<IStashStorage> Storage;
};