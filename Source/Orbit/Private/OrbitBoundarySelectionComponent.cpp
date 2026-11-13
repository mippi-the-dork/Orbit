#include "OrbitBoundarySelectionComponent.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

UOrbitBoundarySelectionComponent::UOrbitBoundarySelectionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetHiddenInGame(true);
    CastShadow = false;
}

#if WITH_EDITOR
class FOrbitBoundarySceneProxy final : public FPrimitiveSceneProxy
{
public:
    explicit FOrbitBoundarySceneProxy(const UOrbitBoundarySelectionComponent* Component)
        : FPrimitiveSceneProxy(Component), Lines(Component->GetPreviewLines())
    {
        bWillEverBeLit = false;
    }

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
        uint32 VisibilityMap, FMeshElementCollector& Collector) const override
    {
        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
        {
            if (!(VisibilityMap & (1u << ViewIndex))) { continue; }
            FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
            for (const UOrbitBoundarySelectionComponent::FLine& Line : Lines)
            {
                PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, Line.Thickness);
            }
        }
    }
    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {
        FPrimitiveViewRelevance Result;
        Result.bDrawRelevance = IsShown(View);
        Result.bDynamicRelevance = true;
        Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
        return Result;
    }
    virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
    uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize(); }
    virtual SIZE_T GetTypeHash() const override
    {
        static size_t TypeHash = 0;
        return reinterpret_cast<SIZE_T>(&TypeHash);
    }
private:
    TArray<UOrbitBoundarySelectionComponent::FLine> Lines;
};
#endif

FPrimitiveSceneProxy* UOrbitBoundarySelectionComponent::CreateSceneProxy()
{
#if WITH_EDITOR
    if (PreviewLines.Num() > 0 && GetWorld() && !GetWorld()->IsGameWorld())
    {
        return new FOrbitBoundarySceneProxy(this);
    }
#endif
    return nullptr;
}

FBoxSphereBounds UOrbitBoundarySelectionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
#if WITH_EDITOR
    FBox Box(ForceInit);
    for (const FLine& Line : PreviewLines)
    {
        Box += Line.Start;
        Box += Line.End;
    }
    if (Box.IsValid)
    {
        // Line positions are authored in world space, not component-local coordinates.
        return FBoxSphereBounds(Box);
    }
#endif
    return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(1.0), 1.0);
}

#if WITH_EDITOR
void UOrbitBoundarySelectionComponent::SetPreviewLines(TArray<FLine>&& NewLines)
{
    // Avoid render-state churn when the editor viewport ticks without property changes.
    bool bChanged = NewLines.Num() != PreviewLines.Num();
    if (!bChanged)
    {
        for (int32 Index = 0; Index < NewLines.Num(); ++Index)
        {
            const FLine& A = NewLines[Index];
            const FLine& B = PreviewLines[Index];
            if (!A.Start.Equals(B.Start, 0.01) || !A.End.Equals(B.End, 0.01) ||
                A.Color != B.Color || !FMath::IsNearlyEqual(A.Thickness, B.Thickness))
            {
                bChanged = true;
                break;
            }
        }
    }
    if (bChanged)
    {
        PreviewLines = MoveTemp(NewLines);
        UpdateBounds();
        MarkRenderStateDirty();
    }
}
#endif
