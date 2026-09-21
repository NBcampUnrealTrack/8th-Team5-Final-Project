#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "HBDungeonPCGSettings.generated.h"

UCLASS(BlueprintType, ClassGroup=(Procedural))
class HEAVENBREACH_API UHBDungeonPlanSettings : public UPCGSettings
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("HBPlanDungeon"); }
	virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("HB - Plan and Validate Dungeon")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class HEAVENBREACH_API UHBDungeonLoadSettings : public UPCGSettings
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("HBLoadDungeon"); }
	virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("HB - Load Room Modules")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif
protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};
