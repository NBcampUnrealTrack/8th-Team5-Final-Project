#include "PCGPrototype/HBPCGPreviewDetails.h"
#if WITH_EDITOR
#include "PCGPrototype/HBPCGPreview.h"
#include "PCGModule/HBDungeonPortal.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    struct FPortalOption { FName Id; FString Label; };
    TArray<FPortalOption> ReadPortals(const TSoftObjectPtr<UWorld>& Map)
    {
        TArray<FPortalOption> Result;
        UWorld* World = Map.LoadSynchronous();
        if (World && World->PersistentLevel)
            for (AActor* Actor : World->PersistentLevel->Actors)
                if (const auto* Portal = Cast<AHBDungeonPortal>(Actor))
                    Result.Add({Portal->PortalId, Portal->GetActorLabel() + TEXT(" [") + Portal->PortalId.ToString() + TEXT("]")});
        Result.Sort([](const auto& A, const auto& B) { return A.Label < B.Label; });
        return Result;
    }
    TSoftObjectPtr<UWorld> ReadMap(const TSharedPtr<IPropertyHandle>& Handle)
    {
        FString Value;
        Handle->GetValueAsFormattedString(Value);
        return TSoftObjectPtr<UWorld>(FSoftObjectPath(FPackageName::ExportTextPathToObjectPath(Value)));
    }
}

TSharedRef<IDetailCustomization> FHBPCGPreviewDetails::MakeInstance() { return MakeShared<FHBPCGPreviewDetails>(); }

void FHBPCGPreviewDetails::CustomizeDetails(IDetailLayoutBuilder& Builder)
{
    // Use flat category keys. Mixing pipe-delimited default subcategories with
    // custom categories can create two headings for the same section.
    const TArray<FName> Categories = {TEXT("PCG Preview - Generation"), TEXT("PCG Preview - Modules"),
        TEXT("PCG Preview - Transitions"), TEXT("PCG Preview - Room Counts"), TEXT("PCG Preview - Search"),
        TEXT("PCG Preview - Exploration"), TEXT("PCG Preview - Result"), TEXT("PCG Preview - Debug"), TEXT("PCG Preview - Advanced")};
    for (int32 I = 0; I < Categories.Num(); ++I) Builder.EditCategory(Categories[I]).SetSortOrder(I);
    TArray<TWeakObjectPtr<UObject>> Objects;
    Builder.GetObjectsBeingCustomized(Objects);
    auto Rules = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(AHBPCGPreviewGenerator, DungeonRules));
    Builder.HideProperty(Rules);
    if (Objects.Num() != 1)
    {
        Builder.EditCategory(Categories[2]).AddCustomRow(FText::FromString(TEXT("Selection")))
            .WholeRowContent()[SNew(STextBlock).Text(FText::FromString(TEXT("Select one generator to configure transitions and rule counts.")))];
        return;
    }
    TWeakObjectPtr<AHBPCGPreviewGenerator> Generator = Cast<AHBPCGPreviewGenerator>(Objects[0].Get());
    if (!Generator.IsValid()) return;
    auto RulesVisible = TAttribute<EVisibility>::CreateLambda([Generator]()
    { return Generator.IsValid() && Generator->bUseDungeonRules ? EVisibility::Visible : EVisibility::Collapsed; });
    auto& Transitions = Builder.EditCategory(Categories[2]);
    auto AddMap = [&](FName Name, const FString& Label, bool bCorridor, FName PortalName)
    {
        auto MapHandle = Rules->GetChildHandle(Name);
        auto PortalHandle = PortalName.IsNone() ? TSharedPtr<IPropertyHandle>() : Rules->GetChildHandle(PortalName);
        Transitions.AddCustomRow(FText::FromString(Label)).Visibility(RulesVisible)
        .NameContent()[SNew(STextBlock).Text(FText::FromString(Label))]
        .ValueContent().MinDesiredWidth(280)[SNew(SComboButton)
            .OnGetMenuContent_Lambda([Generator, MapHandle, PortalHandle, bCorridor]() -> TSharedRef<SWidget>
            {
                FMenuBuilder Menu(true, nullptr);
                auto Add = [&](const TSoftObjectPtr<UWorld>& Map, const FString& Text)
                {
                    Menu.AddMenuEntry(FText::FromString(Text), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([MapHandle, PortalHandle, Map]()
                    {
                        if (ReadMap(MapHandle) == Map) return;
                        const FScopedTransaction Transaction(FText::FromString(TEXT("Select dungeon transition module")));
                        MapHandle->SetValueFromFormattedString(Map.ToSoftObjectPath().ToString());
                        if (PortalHandle.IsValid())
                        {
                            const auto Portals = ReadPortals(Map);
                            PortalHandle->SetValue(Portals.Num() == 1 ? Portals[0].Id : NAME_None);
                        }
                    })));
                };
                Add({}, TEXT("None"));
                if (Generator.IsValid()) for (const auto& Map : Generator->ModuleMaps)
                {
                    UWorld* World = Map.LoadSynchronous();
                    if (!World || !World->PersistentLevel) continue;
                    for (AActor* Actor : World->PersistentLevel->Actors)
                        if (const auto* Definition = Cast<AHBDungeonModuleDefinition>(Actor))
                        {
                            if ((Definition->ModuleKind == EHBDungeonModuleKind::Corridor) == bCorridor)
                                Add(Map, Map.GetAssetName() + TEXT("  (") + Map.ToSoftObjectPath().GetLongPackageName() + TEXT(")"));
                            break;
                        }
                }
                return Menu.MakeWidget();
            })
            .ButtonContent()[SNew(STextBlock).Text_Lambda([MapHandle]()
            { const auto Map = ReadMap(MapHandle); return FText::FromString(Map.IsNull() ? TEXT("Select a registered module") : Map.GetAssetName()); })]];
        if (!PortalHandle.IsValid()) return;
        // Labels are cached; opening the menu refreshes actors without loading maps every frame.
        auto Options = MakeShared<TArray<FPortalOption>>(ReadPortals(ReadMap(MapHandle)));
        MapHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([Options, MapHandle]() { *Options = ReadPortals(ReadMap(MapHandle)); }));
        Transitions.AddCustomRow(FText::FromString(Label + TEXT(" Portal"))).Visibility(RulesVisible)
        .NameContent()[SNew(STextBlock).Text(FText::FromString(Label + TEXT(" Portal")))]
        .ValueContent().MinDesiredWidth(280)[SNew(SComboButton)
            .OnGetMenuContent_Lambda([Options, MapHandle, PortalHandle]() -> TSharedRef<SWidget>
            {
                *Options = ReadPortals(ReadMap(MapHandle));
                FMenuBuilder Menu(true, nullptr);
                for (const auto& Option : *Options)
                    Menu.AddMenuEntry(FText::FromString(Option.Label), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([PortalHandle, Id = Option.Id]() { PortalHandle->SetValue(Id); })));
                if (Options->IsEmpty()) Menu.AddMenuEntry(FText::FromString(TEXT("No portal: place a Dungeon Portal in this map")), FText::GetEmpty(), FSlateIcon(), FUIAction());
                return Menu.MakeWidget();
            })
            .ButtonContent()[SNew(STextBlock).Text_Lambda([Options, PortalHandle]()
            {
                FName Id; PortalHandle->GetValue(Id);
                if (Id.IsNone() && Options->Num() == 1) return FText::FromString(TEXT("Auto: ") + (*Options)[0].Label);
                for (const auto& Option : *Options) if (Option.Id == Id && !Id.IsNone()) return FText::FromString(Option.Label);
                return FText::FromString(!Id.IsNone() ? TEXT("Missing portal: ") + Id.ToString() : Options->IsEmpty() ? TEXT("No portal in selected map") : TEXT("Select a portal actor"));
            })]];
    };
    AddMap(TEXT("Floor1ExitMap"), TEXT("Floor 1 Exit"), false, TEXT("Floor1ExitPortalId"));
    AddMap(TEXT("Floor2EntryMap"), TEXT("Floor 2 Entry"), false, TEXT("Floor2EntryPortalId"));
    AddMap(TEXT("BossCorridorMap"), TEXT("Boss Corridor"), true, NAME_None);
    Transitions.AddProperty(Rules->GetChildHandle(TEXT("Floor2Offset"))).Visibility(RulesVisible);
    for (FName Name : {FName(TEXT("Floor1MainRooms")), FName(TEXT("Floor2MainRooms")), FName(TEXT("Floor1BranchRooms")), FName(TEXT("Floor2BranchRooms"))})
        Builder.EditCategory(Categories[3]).AddProperty(Rules->GetChildHandle(Name)).Visibility(RulesVisible);
    Builder.EditCategory(Categories[3]).AddCustomRow(FText::FromString(TEXT("Total"))).Visibility(RulesVisible)
        .WholeRowContent()[SNew(STextBlock).Text_Lambda([Generator]()
        {
            if (!Generator.IsValid()) return FText::GetEmpty();
            const auto& R = Generator->DungeonRules;
            return FText::FromString(FString::Printf(TEXT("Total: %d rooms + 1 boss corridor. Main counts include transition rooms and boss."),
                R.Floor1MainRooms + R.Floor2MainRooms + R.Floor1BranchRooms + R.Floor2BranchRooms));
        })];
    Builder.EditCategory(Categories[8]).AddCustomRow(FText::FromString(TEXT("PCG Settings")))
        .WholeRowContent()[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("Generator Seed and Graph control generation. PCG component values are overwritten when generating.")))];
}
#endif
