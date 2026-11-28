#include "OrbitGravityDebugDrawComponent.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

class FOrbitGravityDebugSceneProxy final : public FPrimitiveSceneProxy
{
public:
    explicit FOrbitGravityDebugSceneProxy(const UOrbitGravityDebugDrawComponent* Component,
        const TArray<FOrbitGravityDebugLine>& InLines)
        : FPrimitiveSceneProxy(Component), Lines(InLines)
    {
        bWillEverBeLit = false;
    }
    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
        const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
        FMeshElementCollector& Collector) const override
    {
        for (int32 Index = 0; Index < Views.Num(); ++Index)
        {
            if ((VisibilityMap & (1u << Index)) == 0u) { continue; }
            FPrimitiveDrawInterface* PDI = Collector.GetPDI(Index);
            for (const FOrbitGravityDebugLine& Line : Lines)
            {
                PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, Line.Thickness);
            }
        }
    }
    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {
        FPrimitiveViewRelevance Relevance;
        Relevance.bDrawRelevance = IsShown(View);
        Relevance.bDynamicRelevance = true;
        Relevance.bEditorPrimitiveRelevance = UseEditorCompositing(View);
        return Relevance;
    }
    virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
    uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize(); }
    virtual SIZE_T GetTypeHash() const override
    {
        static size_t Hash = 0;
        return reinterpret_cast<SIZE_T>(&Hash);
    }
private:
    TArray<FOrbitGravityDebugLine> Lines;
};

UOrbitGravityDebugDrawComponent::UOrbitGravityDebugDrawComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetHiddenInGame(false);
    CastShadow = false;
}

void UOrbitGravityDebugDrawComponent::SetLines(TArray<FOrbitGravityDebugLine>&& NewLines)
{
    Lines = MoveTemp(NewLines);
    UpdateBounds();
    MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UOrbitGravityDebugDrawComponent::CreateSceneProxy()
{
    return Lines.IsEmpty() ? nullptr : new FOrbitGravityDebugSceneProxy(this, Lines);
}

FBoxSphereBounds UOrbitGravityDebugDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBox Box(ForceInit);
    for (const FOrbitGravityDebugLine& Line : Lines)
    {
        Box += Line.Start;
        Box += Line.End;
    }
    return Box.IsValid ? FBoxSphereBounds(Box) : FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(1), 1.0);
}
