#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "OrbitBoundarySelectionComponent.generated.h"

/** Editor-only hit-testable line preview. This component never participates in collisions or gravity. */
UCLASS(ClassGroup=(Orbit), NotBlueprintable, NotPlaceable, HideCategories=(Collision, Physics, Rendering))
class ORBIT_API UOrbitBoundarySelectionComponent : public UPrimitiveComponent
{
    GENERATED_BODY()
public:
    UOrbitBoundarySelectionComponent();
    virtual bool IsEditorOnly() const override { return true; }
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if WITH_EDITOR
    struct FLine
    {
        FVector Start;
        FVector End;
        FColor Color;
        float Thickness;
    };
    void SetPreviewLines(TArray<FLine>&& NewLines);
    const TArray<FLine>& GetPreviewLines() const { return PreviewLines; }
private:
    TArray<FLine> PreviewLines;
#endif
};
