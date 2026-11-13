#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IPropertyHandle;

/** Shares the same shape-sensitive picker between dedicated Actors and embedded Components. */
class FOrbitSourceDirectionDetails final : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override;
private:
    TSharedRef<SWidget> MakeDirectionMenu();
    FText GetDirectionLabel() const;
    bool IsSplineShape() const;
    bool IsDirectionEditable() const;
    TSharedPtr<IPropertyHandle> VolumeDirectionModeHandle;
    TSharedPtr<IPropertyHandle> ShapeHandle;
    TSharedPtr<IPropertyHandle> DirectionHandle;
};
