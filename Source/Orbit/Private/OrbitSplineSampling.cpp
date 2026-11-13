#include "OrbitSplineSampling.h"
#include "Components/SplineComponent.h"
#include "Misc/Crc.h"
#include "HAL/PlatformTime.h"

namespace
{
    struct FOrbitSamplePoint
    {
        FVector Position = FVector::ZeroVector;
        FVector Up = FVector::UpVector;
    };

    FOrbitSamplePoint EvaluateSplineKey(const USplineComponent* Spline, double Key)
    {
        FOrbitSamplePoint Result;
        Result.Position = Spline->GetLocationAtSplineInputKey(static_cast<float>(Key), ESplineCoordinateSpace::World);
        Result.Up = Spline->GetUpVectorAtSplineInputKey(static_cast<float>(Key), ESplineCoordinateSpace::World).GetSafeNormal();
        return Result;
    }

    double DistanceToChord(const FVector& Point, const FVector& A, const FVector& B)
    {
        const FVector Delta = B - A;
        const double LengthSq = Delta.SizeSquared();
        if (LengthSq <= UE_DOUBLE_SMALL_NUMBER) { return FVector::Distance(Point, A); }
        const double Alpha = FMath::Clamp(FVector::DotProduct(Point - A, Delta) / LengthSq, 0.0, 1.0);
        return FVector::Distance(Point, A + Delta * Alpha);
    }
}

void FOrbitSplineSampling::Build(const USplineComponent* Spline, double MaxWorldChord, int32 MaxSegments,
    TArray<FVector>& OutPoints, TArray<FVector>& OutUps)
{
    OutPoints.Reset();
    OutUps.Reset();
    if (!IsValid(Spline)) { return; }

    const int32 PointCount = Spline->GetNumberOfSplinePoints();
    const int32 IntervalCount = Spline->IsClosedLoop() ? PointCount : PointCount - 1;
    if (IntervalCount < 1) { return; }

    const double Chord = FMath::Max(MaxWorldChord, 25.0);
    // This is an absolute WORLD-space deviation, including scaled actors/components.
    const double Tolerance = FMath::Max(2.0, Chord * 0.05);
    // Never allocate enormous paths from malformed configuration; favor one sample per
    // authored segment, with remaining budget allocated to curved areas.
    const int32 Budget = FMath::Max(IntervalCount, FMath::Clamp(MaxSegments, 1, 32768));
    OutPoints.Reserve(FMath::Min(Budget + 1, 4096));
    OutUps.Reserve(FMath::Min(Budget + 1, 4096));

    const FOrbitSamplePoint Start = EvaluateSplineKey(Spline, 0.0);
    OutPoints.Add(Start.Position);
    OutUps.Add(Start.Up);

    // Each knot is emitted even if adaptive subdivision exhausts its budget.
    // Reserve enough slots for every remaining interval before refining this one.
    for (int32 Interval = 0; Interval < IntervalCount; ++Interval)
    {
        const double KeyA = static_cast<double>(Interval);
        const double KeyB = KeyA + 1.0;
        const FOrbitSamplePoint Begin{OutPoints.Last(), OutUps.Last()};
        const FOrbitSamplePoint End = EvaluateSplineKey(Spline, KeyB);
        const int32 ReservedForLater = IntervalCount - Interval;
        const int32 SubdivisionLimit = Budget - ReservedForLater;

        // Iterative depth-first subdivision keeps the code predictable and avoids
        // deep recursion on unusually long or malformed curves.
        struct FInterval
        {
            double A, B;
            FOrbitSamplePoint Begin, End;
            int32 Depth;
        };
        TArray<FInterval, TInlineAllocator<32>> Pending;
        Pending.Add({KeyA, KeyB, Begin, End, 0});
        while (Pending.Num() > 0)
        {
            const FInterval Current = Pending.Pop(EAllowShrinking::No);
            const double MidKey = (Current.A + Current.B) * 0.5;
            const FOrbitSamplePoint Middle = EvaluateSplineKey(Spline, MidKey);
            const double MidError = DistanceToChord(Middle.Position, Current.Begin.Position, Current.End.Position);
            const double QuarterError = DistanceToChord(
                EvaluateSplineKey(Spline, (Current.A + MidKey) * 0.5).Position,
                Current.Begin.Position, Current.End.Position);
            const double ThreeQuarterError = DistanceToChord(
                EvaluateSplineKey(Spline, (MidKey + Current.B) * 0.5).Position,
                Current.Begin.Position, Current.End.Position);
            const bool bNeedsSplit = FVector::Distance(Current.Begin.Position, Current.End.Position) > Chord ||
                FMath::Max3(MidError, QuarterError, ThreeQuarterError) > Tolerance;
            // Pending intervals still require one output each; reserve the final knot.
            if (bNeedsSplit && Current.Depth < 12 && OutPoints.Num() + Pending.Num() < SubdivisionLimit)
            {
                Pending.Add({MidKey, Current.B, Middle, Current.End, Current.Depth + 1});
                Pending.Add({Current.A, MidKey, Current.Begin, Middle, Current.Depth + 1});
            }
            else
            {
                OutPoints.Add(Current.End.Position);
                OutUps.Add(Current.End.Up);
            }
        }
    }
}


namespace
{
    uint32 OrbitSplineGeometryHash(const USplineComponent* Spline)
    {
        // Hash authored position/tangent data rather than the spline length: moving
        // tangents can alter the curve without changing its length or endpoints.
        uint32 Hash = GetTypeHash(Spline->GetNumberOfSplinePoints());
        Hash = HashCombine(Hash, GetTypeHash(Spline->IsClosedLoop()));
        const FVector DefaultUp = Spline->GetDefaultUpVector(ESplineCoordinateSpace::Local);
        Hash = HashCombine(Hash, FCrc::MemCrc32(&DefaultUp, sizeof(DefaultUp)));
        const auto& Knots = Spline->GetSplinePointsPosition().Points;
        for (const auto& Knot : Knots)
        {
            Hash = HashCombine(Hash, FCrc::MemCrc32(&Knot.InVal, sizeof(Knot.InVal)));
            Hash = HashCombine(Hash, FCrc::MemCrc32(&Knot.OutVal, sizeof(Knot.OutVal)));
            Hash = HashCombine(Hash, FCrc::MemCrc32(&Knot.ArriveTangent, sizeof(Knot.ArriveTangent)));
            Hash = HashCombine(Hash, FCrc::MemCrc32(&Knot.LeaveTangent, sizeof(Knot.LeaveTangent)));
            Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Knot.InterpMode)));
        }
        // Up vectors can change independently of position/tangents.
        const auto& Rotations = Spline->GetSplinePointsRotation().Points;
        for (const auto& Knot : Rotations)
        {
            Hash = HashCombine(Hash, FCrc::MemCrc32(&Knot.OutVal, sizeof(Knot.OutVal)));
            Hash = HashCombine(Hash, FCrc::MemCrc32(&Knot.ArriveTangent, sizeof(Knot.ArriveTangent)));
            Hash = HashCombine(Hash, FCrc::MemCrc32(&Knot.LeaveTangent, sizeof(Knot.LeaveTangent)));
            Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Knot.InterpMode)));
        }
        return Hash;
    }

    void OrbitBuildSegmentBlocks(const TArray<FVector>& Curve, TArray<FOrbitSplineSegmentBlock>& Blocks)
    {
        constexpr int32 BlockSize = 32;
        Blocks.Reset();
        const int32 Segments = FMath::Max(0, Curve.Num() - 1);
        Blocks.Reserve(FMath::DivideAndRoundUp(Segments, BlockSize));
        for (int32 First = 0; First < Segments; First += BlockSize)
        {
            FVector Min = Curve[First];
            FVector Max = Min;
            const int32 End = FMath::Min(First + BlockSize, Segments);
            for (int32 PointIndex = First + 1; PointIndex <= End; ++PointIndex)
            {
                Min = Min.ComponentMin(Curve[PointIndex]);
                Max = Max.ComponentMax(Curve[PointIndex]);
            }
            FOrbitSplineSegmentBlock& Block = Blocks.AddDefaulted_GetRef();
            Block.Min = Min;
            Block.Max = Max;
        }
    }

    double OrbitDistanceSquaredToAABB(const FVector& Position, const FOrbitSplineSegmentBlock& Block)
    {
        const FVector Nearest = Position.ComponentMax(Block.Min).ComponentMin(Block.Max);
        return FVector::DistSquared(Position, Nearest);
    }
}

void FOrbitSplineSampleCache::Get(const USplineComponent* Spline, double Spacing, int32 MaxSegments,
    TArray<FVector>& OutPoints, TArray<FVector>& OutUps,
    TArray<FOrbitSplineSegmentBlock>& OutBlocks, bool bReuseUnchangedOutput,
    double ValidationIntervalSeconds, bool bManualValidation) const
{
    if (!IsValid(Spline))
    {
        Invalidate();
        OutPoints.Reset(); OutUps.Reset(); OutBlocks.Reset();
        return;
    }
    const FTransform World = Spline->GetComponentTransform();
    const double Now = FPlatformTime::Seconds();
    const bool bStructuralChange = CachedSpline.Get() != Spline ||
        !CachedWorldTransform.Equals(World) || CachedSpacing != Spacing || CachedMaxSegments != MaxSegments;
    // Manual skips authored-point inspection until explicitly invalidated. Automatic
    // uses a short safety poll because external Blueprint/C++ spline mutation has no
    // universal property notification. World transform changes are always immediate.
    const bool bCheckGeometry = bStructuralChange || (!bManualValidation &&
        (ValidationIntervalSeconds <= 0.0 || LastGeometryValidationTime < 0.0 ||
         Now - LastGeometryValidationTime >= ValidationIntervalSeconds));
    const uint32 Hash = bCheckGeometry ? OrbitSplineGeometryHash(Spline) : CachedCurveHash;
    if (bCheckGeometry) { LastGeometryValidationTime = Now; }
    const bool bChanged = bStructuralChange || (bCheckGeometry && CachedCurveHash != Hash);
    if (bChanged)
    {
        FOrbitSplineSampling::Build(Spline, Spacing, MaxSegments, Points, Ups);
        OrbitBuildSegmentBlocks(Points, Blocks);
        ++RebuildCount;
        CachedSpline = const_cast<USplineComponent*>(Spline);
        CachedWorldTransform = World;
        CachedCurveHash = Hash;
        CachedSpacing = Spacing;
        CachedMaxSegments = MaxSegments;
    }
    // An unchanged source's previous snapshot can move its existing arrays forward.
    // Never reuse those arrays if the spline or its authored geometry was invalidated.
    if (bReuseUnchangedOutput && !bChanged && OutPoints.Num() == Points.Num() &&
        OutUps.Num() == Ups.Num() && OutBlocks.Num() == Blocks.Num() && !OutPoints.IsEmpty())
    {
        return;
    }
    OutPoints = Points;
    OutUps = Ups;
    OutBlocks = Blocks;
}

FVector OrbitClosestOnCurveAccelerated(const TArray<FVector>& Curve,
    const TArray<FOrbitSplineSegmentBlock>& Blocks, const FVector& Position,
    int32& OutSegment, double& OutAlpha)
{
    OutSegment = INDEX_NONE;
    OutAlpha = 0.0;
    FVector Closest = FVector::ZeroVector;
    double BestDistSq = TNumericLimits<double>::Max();
    constexpr int32 BlockSize = 32;
    const int32 SegmentCount = Curve.Num() - 1;
    // Authored/test definitions without acceleration metadata retain exact behavior.
    if (SegmentCount <= 0) { return Closest; }
    const bool bValidBlocks = Blocks.Num() == FMath::DivideAndRoundUp(SegmentCount, BlockSize);
    for (int32 BlockIndex = 0; BlockIndex < (bValidBlocks ? Blocks.Num() : 1); ++BlockIndex)
    {
        if (bValidBlocks && OrbitDistanceSquaredToAABB(Position, Blocks[BlockIndex]) > BestDistSq)
        {
            continue;
        }
        const int32 Start = bValidBlocks ? BlockIndex * BlockSize : 0;
        const int32 End = bValidBlocks ? FMath::Min(Start + BlockSize, SegmentCount) : SegmentCount;
        for (int32 I = Start; I < End; ++I)
        {
            const FVector Delta = Curve[I + 1] - Curve[I];
            const double LengthSq = Delta.SizeSquared();
            if (LengthSq <= UE_DOUBLE_SMALL_NUMBER) { continue; }
            const double Alpha = FMath::Clamp(FVector::DotProduct(Position - Curve[I], Delta) / LengthSq, 0.0, 1.0);
            const FVector Candidate = Curve[I] + Delta * Alpha;
            const double DistSq = FVector::DistSquared(Position, Candidate);
            if (DistSq < BestDistSq)
            {
                BestDistSq = DistSq;
                Closest = Candidate;
                OutSegment = I;
                OutAlpha = Alpha;
            }
        }
    }
    return Closest;
}
