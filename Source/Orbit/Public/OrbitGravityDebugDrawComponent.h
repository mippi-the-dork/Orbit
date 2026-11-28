#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "OrbitGravityDebugDrawComponent.generated.h"

/** One world-space line used by the Orbit gravity field viewport visualization. */
struct FOrbitGravityDebugLine
{
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    FColor Color = FColor::Cyan;
    float Thickness = 1.5f;
};

/** Lightweight render-only component. Uses Unreal's native visibility/Hidden In Game behavior. */
UCLASS(ClassGroup=(Orbit), meta=(BlueprintSpawnableComponent))
class ORBIT_API UOrbitGravityDebugDrawComponent : public UPrimitiveComponent
{
    GENERATED_BODY()
public:
    UOrbitGravityDebugDrawComponent();
    void SetLines(TArray<FOrbitGravityDebugLine>&& NewLines);
    /** Read-only view of the rendered snapshot, useful for diagnostics and automation. */
    const TArray<FOrbitGravityDebugLine>& GetDebugLines() const { return Lines; }
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
private:
    TArray<FOrbitGravityDebugLine> Lines;
};
