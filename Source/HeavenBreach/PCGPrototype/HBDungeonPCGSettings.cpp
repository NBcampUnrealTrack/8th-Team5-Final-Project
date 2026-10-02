#include "PCGPrototype/HBDungeonPCGSettings.h"
#include "PCGPrototype/HBPCGPreview.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGElement.h"
#include "Engine/World.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

namespace
{
    struct FHBDungeonGraphOwner
    {
        AHBPCGPreviewGenerator* Preview = nullptr;
        AHBDungeonRuntime* Runtime = nullptr;
        explicit operator bool() const { return Preview || Runtime; }
        UWorld* GetWorld() const { return Preview ? Preview->GetWorld() : Runtime->GetWorld(); }
        bool PlanLayout() const { return Preview ? Preview->PlanLayout() : Runtime->PlanLayout(); }
        const TArray<FHBPrototypeRoom>& GetRooms() const { return Preview ? Preview->Rooms : Runtime->Rooms; }
        int32 GetSeed() const { return Preview ? Preview->ActiveSeed : Runtime->ActiveSeed; }
        void MaterializeLayout(int32 Count) const { if (Preview) Preview->MaterializeLayout(Count); else Runtime->MaterializeLayout(Count); }
    };
    FHBDungeonGraphOwner Generator(FPCGContext* Context)
    {
        UPCGComponent* Component = Cast<UPCGComponent>(Context->ExecutionSource.GetObject());
        AActor* Owner = Component ? Component->GetOwner() : nullptr;
        auto* Runtime = Cast<AHBDungeonRuntime>(Owner);
        if (Runtime && !Runtime->CanPlanGeneration()) return {};
        return {Cast<AHBPCGPreviewGenerator>(Owner), Runtime};
    }
	class FHBPlanElement : public IPCGElement
	{
	public:
		virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
		virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
	protected:
		virtual bool ExecuteInternal(FPCGContext* Context) const override
		{
			auto G = Generator(Context);
			if (!G || !G.GetWorld()->IsGameWorld() || !G.PlanLayout()) return true;
			auto* Data = NewObject<UPCGPointData>();
			auto* Id = Data->Metadata->CreateAttribute<int32>(TEXT("RoomId"), -1, false, true);
			auto* Parent = Data->Metadata->CreateAttribute<int32>(TEXT("ParentId"), -1, false, true);
			auto* Floor = Data->Metadata->CreateAttribute<int32>(TEXT("Floor"), 1, false, true);
			auto* Role = Data->Metadata->CreateAttribute<FString>(TEXT("Role"), TEXT(""), false, true);
            auto* Module = Data->Metadata->CreateAttribute<FString>(TEXT("ModuleId"), TEXT(""), false, true);
            auto* Map = Data->Metadata->CreateAttribute<FString>(TEXT("ModuleMap"), TEXT(""), false, true);
            auto* ParentConnector = Data->Metadata->CreateAttribute<FString>(TEXT("ParentConnectorId"), TEXT(""), false, true);
            auto* EntryConnector = Data->Metadata->CreateAttribute<FString>(TEXT("EntryConnectorId"), TEXT(""), false, true);
            auto* Corridor = Data->Metadata->CreateAttribute<bool>(TEXT("IsCorridor"), false, false, true);
            auto* Connection = Data->Metadata->CreateAttribute<FString>(TEXT("ConnectionType"), TEXT("Door"), false, true);
            auto* ParentPortal = Data->Metadata->CreateAttribute<FString>(TEXT("ParentPortalId"), TEXT(""), false, true);
            auto* EntryPortal = Data->Metadata->CreateAttribute<FString>(TEXT("EntryPortalId"), TEXT(""), false, true);
			for (const auto& Room : G.GetRooms())
			{
				FPCGPoint& P = Data->GetMutablePoints().AddDefaulted_GetRef();
				P.Transform = Room.Transform; P.Density = 1; P.Seed = static_cast<int32>(static_cast<uint32>(G.GetSeed()) + static_cast<uint32>(Room.Id));
				P.BoundsMin = Room.LocalBounds.Min;
				P.BoundsMax = Room.LocalBounds.Max;
				P.Color = FVector4(FLinearColor(Room.bCorridor ? FColor::Silver : HBDungeonLayout::RoleColor(Room.Role)));
				P.MetadataEntry = Data->Metadata->AddEntry();
				Id->SetValue(P.MetadataEntry, Room.Id); Parent->SetValue(P.MetadataEntry, Room.ParentId);
				Floor->SetValue(P.MetadataEntry, Room.Floor); Role->SetValue(P.MetadataEntry, Room.bCorridor ? TEXT("CORRIDOR") : HBDungeonLayout::RoleName(Room.Role));
                Corridor->SetValue(P.MetadataEntry, Room.bCorridor);
                Connection->SetValue(P.MetadataEntry, Room.Connection == EHBDungeonConnection::Portal ? TEXT("Portal") : TEXT("Door"));
                ParentPortal->SetValue(P.MetadataEntry, Room.ParentPortalId.ToString());
                EntryPortal->SetValue(P.MetadataEntry, Room.EntryPortalId.ToString());
                Module->SetValue(P.MetadataEntry, Room.ModuleId.ToString()); Map->SetValue(P.MetadataEntry, Room.ModuleMap.ToString());
                ParentConnector->SetValue(P.MetadataEntry, Room.ParentConnectorId.ToString());
                EntryConnector->SetValue(P.MetadataEntry, Room.EntryConnectorId.ToString());
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
			if (auto G = Generator(Context))
			{
				int32 Count = 0;
				for (const auto& Input : Context->InputData.GetInputsByPin(TEXT("Rooms")))
					if (const auto* Points = Cast<UPCGPointData>(Input.Data)) Count += Points->GetPoints().Num();
				if (Count) G.MaterializeLayout(Count);
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
