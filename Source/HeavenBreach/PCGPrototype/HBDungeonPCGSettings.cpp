#include "PCGPrototype/HBDungeonPCGSettings.h"
#include "PCGPrototype/HBPCGPreview.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGElement.h"
#include "Engine/World.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

namespace
{
	AHBPCGPreviewGenerator* Generator(FPCGContext* Context)
	{
		UPCGComponent* Component = Cast<UPCGComponent>(Context->ExecutionSource.GetObject());
		return Component ? Cast<AHBPCGPreviewGenerator>(Component->GetOwner()) : nullptr;
	}
	class FHBPlanElement : public IPCGElement
	{
	public:
		virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
		virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
	protected:
		virtual bool ExecuteInternal(FPCGContext* Context) const override
		{
			auto* G = Generator(Context);
			if (!G || !G->GetWorld()->IsGameWorld() || !G->PlanLayout()) return true;
			auto* Data = NewObject<UPCGPointData>();
			auto* Id = Data->Metadata->CreateAttribute<int32>(TEXT("RoomId"), -1, false, true);
			auto* Parent = Data->Metadata->CreateAttribute<int32>(TEXT("ParentId"), -1, false, true);
			auto* Floor = Data->Metadata->CreateAttribute<int32>(TEXT("Floor"), 1, false, true);
			auto* Role = Data->Metadata->CreateAttribute<FString>(TEXT("Role"), TEXT(""), false, true);
            auto* Module = Data->Metadata->CreateAttribute<FString>(TEXT("ModuleId"), TEXT(""), false, true);
            auto* Map = Data->Metadata->CreateAttribute<FString>(TEXT("ModuleMap"), TEXT(""), false, true);
			for (const auto& Room : G->Rooms)
			{
				FPCGPoint& P = Data->GetMutablePoints().AddDefaulted_GetRef();
				P.Transform = Room.Transform; P.Density = 1; P.Seed = G->ActiveSeed + Room.Id;
				P.BoundsMin = Room.LocalBounds.Min;
				P.BoundsMax = Room.LocalBounds.Max;
				P.Color = FVector4(FLinearColor(HBDungeonLayout::RoleColor(Room.Role)));
				P.MetadataEntry = Data->Metadata->AddEntry();
				Id->SetValue(P.MetadataEntry, Room.Id); Parent->SetValue(P.MetadataEntry, Room.ParentId);
				Floor->SetValue(P.MetadataEntry, Room.Floor); Role->SetValue(P.MetadataEntry, HBDungeonLayout::RoleName(Room.Role));
                Module->SetValue(P.MetadataEntry, Room.ModuleId.ToString()); Map->SetValue(P.MetadataEntry, Room.ModuleMap.ToString());
			}
			FPCGTaggedData& Out = Context->OutputData.TaggedData.AddDefaulted_GetRef();
			Out.Data = Data; Out.Pin = TEXT("Rooms");
			return true;
		}
	};
	class FHBLoadElement : public IPCGElement
	{
	public:
		virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
		virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
	protected:
		virtual bool ExecuteInternal(FPCGContext* Context) const override
		{
			if (auto* G = Generator(Context))
			{
				int32 Count = 0;
				for (const auto& Input : Context->InputData.GetInputsByPin(TEXT("Rooms")))
					if (const auto* Points = Cast<UPCGPointData>(Input.Data)) Count += Points->GetPoints().Num();
				if (Count) G->MaterializeLayout(Count);
			}
			Context->OutputData = Context->InputData;
			for (auto& Out : Context->OutputData.TaggedData) Out.Pin = TEXT("Out");
			return true;
		}
	};
}

TArray<FPCGPinProperties> UHBDungeonPlanSettings::OutputPinProperties() const
{
	return { FPCGPinProperties(TEXT("Rooms"), EPCGDataType::Point) };
}
FPCGElementPtr UHBDungeonPlanSettings::CreateElement() const { return MakeShared<FHBPlanElement>(); }
TArray<FPCGPinProperties> UHBDungeonLoadSettings::InputPinProperties() const
{
	FPCGPinProperties Pin(TEXT("Rooms"), EPCGDataType::Point);
	Pin.SetRequiredPin();
	return { Pin };
}
TArray<FPCGPinProperties> UHBDungeonLoadSettings::OutputPinProperties() const
{
	return { FPCGPinProperties(TEXT("Out"), EPCGDataType::Point) };
}
FPCGElementPtr UHBDungeonLoadSettings::CreateElement() const { return MakeShared<FHBLoadElement>(); }
