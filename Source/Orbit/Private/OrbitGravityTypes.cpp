#include "OrbitGravityTypes.h"

FVector FOrbitFieldEvaluator::ClosestOnCurve(const TArray<FVector>& Curve, const FVector& Position, int32& OutSegment, double& OutAlpha)
{
    OutSegment = INDEX_NONE;
    OutAlpha = 0.0;
    FVector Closest = FVector::ZeroVector;
    double BestDistSq = TNumericLimits<double>::Max();
    for (int32 Index = 0; Index + 1 < Curve.Num(); ++Index)
    {
        const FVector Delta = Curve[Index + 1] - Curve[Index];
        const double LengthSq = Delta.SizeSquared();
        if (LengthSq <= UE_DOUBLE_SMALL_NUMBER) { continue; }
        const double Alpha = FMath::Clamp(FVector::DotProduct(Position - Curve[Index], Delta) / LengthSq, 0.0, 1.0);
        const FVector Candidate = Curve[Index] + Delta * Alpha;
        const double DistSq = FVector::DistSquared(Position, Candidate);
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            Closest = Candidate;
            OutSegment = Index;
            OutAlpha = Alpha;
        }
    }
    return Closest;
}

bool FOrbitFieldEvaluator::Evaluate(const FOrbitFieldDefinition& Field, const FVector& Position, FVector& OutAcceleration)
{
    OutAcceleration = FVector::ZeroVector;
    if (Position.ContainsNaN() || Field.Center.ContainsNaN() || !FMath::IsFinite(Field.Strength)) { return false; }
    if (Field.bLimitToBox || Field.Shape == EOrbitGravityShape::Volume)
    {
        const FVector Local = Field.Rotation.Inverse().RotateVector(Position - Field.Center);
        if (FMath::Abs(Local.X) > Field.BoxExtents.X ||
            FMath::Abs(Local.Y) > Field.BoxExtents.Y ||
            FMath::Abs(Local.Z) > Field.BoxExtents.Z) { return false; }
    }

    FVector Direction = FVector::ZeroVector;
    double Distance = 0.0;
    switch (Field.Shape)
    {
    case EOrbitGravityShape::Point:
    {
        const FVector Radial = Position - Field.Center;
        Distance = Radial.Size();
        Direction = Radial.GetSafeNormal();
        break;
    }
    case EOrbitGravityShape::Axis:
    {
        const FVector Axis = Field.Axis.GetSafeNormal();
        const FVector Offset = Position - Field.Center;
        const FVector Radial = Offset - Axis * FVector::DotProduct(Offset, Axis);
        Distance = Radial.Size();
        Direction = Radial.GetSafeNormal();
        break;
    }
    case EOrbitGravityShape::Volume:
    case EOrbitGravityShape::Directional:
        Direction = Field.DirectionVector.GetSafeNormal();
        Distance = (Position - Field.Center).Size();
        break;
    case EOrbitGravityShape::Spline:
    {
        int32 Segment = INDEX_NONE;
        double Alpha = 0.0;
        const FVector Closest = ClosestOnCurve(Field.Curve, Position, Segment, Alpha);
        if (Segment == INDEX_NONE) { return false; }
        const FVector Radial = Position - Closest;
        Distance = Radial.Size();
        Direction = Radial.GetSafeNormal();
        if (Field.Direction == EOrbitGravityDirection::SplineUp && Field.CurveUp.IsValidIndex(Segment + 1))
        {
            Direction = FMath::Lerp(Field.CurveUp[Segment], Field.CurveUp[Segment + 1], Alpha).GetSafeNormal();
        }
        break;
    }
    }

    if (Field.MaxDistance > 0.0 && Distance > Field.MaxDistance) { return false; }
    if (Field.Shape != EOrbitGravityShape::Directional && Field.Direction == EOrbitGravityDirection::Inward)
    {
        Direction *= -1.0;
    }
    else if ((Field.Shape == EOrbitGravityShape::Directional || Field.Shape == EOrbitGravityShape::Volume) && Field.Direction == EOrbitGravityDirection::Outward)
    {
        Direction *= -1.0;
    }

    double Multiplier = 1.0;
    if (Field.Falloff == EOrbitGravityFalloff::Linear && Field.MaxDistance > 0.0)
    {
        Multiplier = FMath::Clamp(1.0 - Distance / Field.MaxDistance, 0.0, 1.0);
    }
    else if (Field.Falloff == EOrbitGravityFalloff::InverseSquare)
    {
        const double Reference = FMath::Max(Field.ReferenceDistance, 1.0);
        const double ClampedDistance = FMath::Max(Distance, Reference);
        Multiplier = FMath::Square(Reference / ClampedDistance);
    }
    // At a radial singularity, the field still claims influence but provides zero acceleration.
    OutAcceleration = Direction * FMath::Max(0.0, Field.Strength) * Multiplier;
    return !OutAcceleration.ContainsNaN();
}

FOrbitGravitySample FOrbitFieldEvaluator::Combine(const TArray<FOrbitFieldDefinition>& Fields, const FVector& Position, EOrbitGravityChannel Channel)
{
    FOrbitGravitySample Result;
    FVector Additive = FVector::ZeroVector;
    FVector Override = FVector::ZeroVector;
    int32 OverridePriority = TNumericLimits<int32>::Lowest();
    int32 BestId = INDEX_NONE;
    double BestMagnitudeSq = -1.0;
    bool bOverrideFound = false;
    bool bAny = false;

    // An overriding field suppresses additive fields at the same or lower priority.
    for (const FOrbitFieldDefinition& Field : Fields)
    {
        if ((Channel == EOrbitGravityChannel::Physics && !Field.bPhysics) ||
            (Channel == EOrbitGravityChannel::Character && !Field.bCharacters)) { continue; }
        FVector Acceleration;
        if (!Evaluate(Field, Position, Acceleration)) { continue; }
        bAny = true;
        if (Field.Combine == EOrbitGravityCombine::Override)
        {
            if (!bOverrideFound || Field.Priority > OverridePriority)
            {
                OverridePriority = Field.Priority;
                Override = Acceleration;
                BestId = Field.SourceId;
                bOverrideFound = true;
            }
        }
    }

    for (const FOrbitFieldDefinition& Field : Fields)
    {
        if (Field.Combine != EOrbitGravityCombine::Additive ||
            (bOverrideFound && Field.Priority <= OverridePriority) ||
            (Channel == EOrbitGravityChannel::Physics && !Field.bPhysics) ||
            (Channel == EOrbitGravityChannel::Character && !Field.bCharacters)) { continue; }
        FVector Acceleration;
        if (!Evaluate(Field, Position, Acceleration)) { continue; }
        Additive += Acceleration;
        const double MagnitudeSq = Acceleration.SizeSquared();
        if (!bOverrideFound && MagnitudeSq > BestMagnitudeSq)
        {
            BestMagnitudeSq = MagnitudeSq;
            BestId = Field.SourceId;
        }
    }

    Result.bHasInfluence = bAny;
    Result.Acceleration = Additive + (bOverrideFound ? Override : FVector::ZeroVector);
    Result.Magnitude = Result.Acceleration.Size();
    Result.bHasValidDirection = Result.Magnitude > UE_DOUBLE_SMALL_NUMBER;
    Result.Down = Result.bHasValidDirection ? Result.Acceleration / Result.Magnitude : FVector::ZeroVector;
    Result.Up = Result.bHasValidDirection ? -Result.Down : FVector::UpVector;
    Result.DominantSourceId = BestId;
    return Result;
}
