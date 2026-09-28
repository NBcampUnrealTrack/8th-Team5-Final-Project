#pragma once
#if WITH_EDITOR
#include "IDetailCustomization.h"
class FHBPCGPreviewDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override;
};
#endif
