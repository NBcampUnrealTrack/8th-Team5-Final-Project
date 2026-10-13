#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HBRoomNameSubsystem.generated.h"

class AHBDungeonRuntime;
class UHBRoomNameWidget;

UCLASS()
class HEAVENBREACH_API UHBRoomNameSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "RoomName")
	static FText GetRoomDisplayName(FName MapName);

	UFUNCTION(BlueprintPure, Category = "RoomName")
	FText GetCurrentRoomName() const { return CurrentRoomName; }

private:
	void CheckRoom();
	AHBDungeonRuntime* FindRuntime();

	TWeakObjectPtr<AHBDungeonRuntime> Runtime;
	TWeakObjectPtr<UHBRoomNameWidget> Widget;
	FTimerHandle CheckTimer;
	int32 CurrentRoomId = INDEX_NONE;
	int32 GenerationId = INDEX_NONE;
	FText CurrentRoomName;
};
