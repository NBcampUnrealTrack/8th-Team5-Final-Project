#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HBMapSubsystem.generated.h"

class UHB_MapWindow;

UCLASS(Config = Game)
class HEAVENBREACH_API UHBMapSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UHBMapSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "Map")
	void ToggleMap();

	UFUNCTION(BlueprintPure, Category = "Map")
	bool IsMapOpen() const { return Window.IsValid(); }

private:
	bool CanOpenMap() const;

	UPROPERTY(Config)
	TSoftClassPtr<UHB_MapWindow> MapWindowClass;

	TWeakObjectPtr<UHB_MapWindow> Window;
};
