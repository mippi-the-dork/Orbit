#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class USplineComponent;

/** World-space spline approximation shared by actor sources, component sources and editor previews.
 *  Samples each authored spline interval independently, preserving all spline knots when the
 *  budget permits. Chord length and curvature error are measured in world centimeters, so
 *  uniformly scaled actor/component hierarchies do not silently lower fidelity.
 */
struct ORBIT_API FOrbitSplineSampling
{
    static void Build(const USplineComponent* Spline, double MaxWorldChord, int32 MaxSegments,
        TArray<FVector>& OutPoints, TArray<FVector>& OutUps);
};

/** Compact, exact AABB lower bounds for blocks of 32 polyline segments.
 *  Traversal skips a block only if its minimum possible distance exceeds the
 *  current best. It therefore returns the same nearest segment as a full scan. */
struct ORBIT_API FOrbitSplineSegmentBlock
{
    FVector Min = FVector::ZeroVector;
    FVector Max = FVector::ZeroVector;
};

struct ORBIT_API FOrbitSplineSampleCache
{
    void Get(const USplineComponent* Spline, double Spacing, int32 MaxSegments,
        TArray<FVector>& OutPoints, TArray<FVector>& OutUps,
        TArray<FOrbitSplineSegmentBlock>& OutBlocks, bool bReuseUnchangedOutput = false,
        double ValidationIntervalSeconds = 0.0, bool bManualValidation = false) const;
    void Invalidate() const { CachedSpline.Reset(); LastGeometryValidationTime = -1.0; Points.Reset(); Ups.Reset(); Blocks.Reset(); }
    uint64 GetRebuildCount() const { return RebuildCount; }
private:
    mutable TWeakObjectPtr<USplineComponent> CachedSpline;
    mutable FTransform CachedWorldTransform = FTransform::Identity;
    mutable uint32 CachedCurveHash = 0;
    mutable double LastGeometryValidationTime = -1.0;
    mutable double CachedSpacing = -1.0;
    mutable int32 CachedMaxSegments = -1;
    mutable uint64 RebuildCount = 0;
    mutable TArray<FVector> Points;
    mutable TArray<FVector> Ups;
    mutable TArray<FOrbitSplineSegmentBlock> Blocks;
};

/** AABB lower-bound traversal, exact result and tie-breaking matching linear scans. */
ORBIT_API FVector OrbitClosestOnCurveAccelerated(const TArray<FVector>& Curve,
    const TArray<FOrbitSplineSegmentBlock>& Blocks, const FVector& Position,
    int32& OutSegment, double& OutAlpha);
