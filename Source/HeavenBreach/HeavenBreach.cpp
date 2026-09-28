// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeavenBreach.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "PCGPrototype/HBPCGPreviewDetails.h"
#endif

class FHeavenBreachModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        FDefaultGameModuleImpl::StartupModule();
#if WITH_EDITOR
        auto& Editor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        Editor.RegisterCustomClassLayout("HBDungeonRuntime", FOnGetDetailCustomizationInstance::CreateStatic(&FHBPCGPreviewDetails::MakeInstance));
        Editor.RegisterCustomClassLayout("HBPCGPreviewGenerator", FOnGetDetailCustomizationInstance::CreateStatic(&FHBPCGPreviewDetails::MakeInstance));
#endif
    }
    virtual void ShutdownModule() override
    {
#if WITH_EDITOR
        if (auto* Editor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
        {
            Editor->UnregisterCustomClassLayout("HBPCGPreviewGenerator");
            Editor->UnregisterCustomClassLayout("HBDungeonRuntime");
        }
#endif
        FDefaultGameModuleImpl::ShutdownModule();
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FHeavenBreachModule, HeavenBreach, "HeavenBreach");

DEFINE_LOG_CATEGORY(LogHeavenBreach)