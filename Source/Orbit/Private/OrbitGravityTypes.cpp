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
    if (Position.ContainsNaN() || Field.Center.ContainsNaN() ||
        !FMath::IsFinite(Field.Strength) || !FMath::IsFinite(Field.VortexSpin)) { return false; }
    // Optional legacy box mask remains independent of the new Volume boundary.
    const FVector Local = Field.Rotation.Inverse().RotateVector(Position - Field.Center);
    if (Local.ContainsNaN()) { return false; }
    if (Field.bLimitToBox &&
        (FMath::Abs(Local.X) > Field.BoxExtents.X ||
         FMath::Abs(Local.Y) > Field.BoxExtents.Y ||
         FMath::Abs(Local.Z) > Field.BoxExtents.Z)) { return false; }

    if (Field.Shape == EOrbitGravityShape::Volume)
    {
        const double Radius = FMath::Max(Field.VolumeRadius, 1.0);
        const double HalfHeight = FMath::Max(Field.VolumeHalfHeight, 1.0);
        switch (Field.VolumeShape)
        {
        case EOrbitVolumeShape::Box:
            if (FMath::Abs(Local.X) > Field.BoxExtents.X ||
                FMath::Abs(Local.Y) > Field.BoxExtents.Y ||
                FMath::Abs(Local.Z) > Field.BoxExtents.Z) { return false; }
            break;
        case EOrbitVolumeShape::Sphere:
            if (Local.SizeSquared() > FMath::Square(Radius)) { return false; }
            break;
        case EOrbitVolumeShape::Cylinder:
            if (FMath::Abs(Local.Z) > HalfHeight ||
                FMath::Square(Local.X) + FMath::Square(Local.Y) > FMath::Square(Radius)) { return false; }
            break;
        case EOrbitVolumeShape::Capsule:
        {
            // Capsule Z is the source actor's local Up axis; HalfHeight includes the caps.
            const double SegmentHalfHeight = FMath::Max(HalfHeight - Radius, 0.0);
            const FVector Closest = FVector(0.0, 0.0, FMath::Clamp(Local.Z, -SegmentHalfHeight, SegmentHalfHeight));
            if ((Local - Closest).SizeSquared() > FMath::Square(Radius)) { return false; }
            break;
        }
        }
    }

    const bool bVortexInward = Field.Direction == EOrbitGravityDirection::VortexInward;
    const bool bVortexOutward = Field.Direction == EOrbitGravityDirection::VortexOutward;
    const bool bVortex = bVortexInward || bVortexOutward;
    // Signed vortex uses a clockwise tangent when viewed from positive local
    // axis toward negative. A negative multiplier reverses to counterclockwise.
    FVector ClockwiseTangent = FVector::ZeroVector;
    FVector Direction = FVector::ZeroVector;
    double Distance = 0.0;
    switch (Field.Shape)
    {
    case EOrbitGravityShape::Point:
    {
        const FVector Radial = Position - Field.Center;
        Distance = Radial.Size();
        Direction = Radial.GetSafeNormal();
        if (bVortex)
        {
            // At the poles, tangent magnitude smoothly goes to zero.
            ClockwiseTangent = FVector::CrossProduct(Direction, Field.Axis.GetSafeNormal());
        }
        break;
    }
    case EOrbitGravityShape::Axis:
    {
        const FVector Axis = Field.Axis.GetSafeNormal();
        if (Axis.IsNearlyZero()) { return false; }
        const FVector Offset = Position - Field.Center;
        const FVector Radial = Offset - Axis * FVector::DotProduct(Offset, Axis);
        Distance = Radial.Size();
        Direction = Radial.GetSafeNormal();
        if (bVortex) { ClockwiseTangent = FVector::CrossProduct(Direction, Axis); }
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
        if (bVortex && Field.Curve.IsValidIndex(Segment + 1))
        {
            const FVector Tangent = (Field.Curve[Segment + 1] - Field.Curve[Segment]).GetSafeNormal();
            ClockwiseTangent = FVector::CrossProduct(Direction, Tangent);
        }
        if ((Field.Direction == EOrbitGravityDirection::SplineUp || Field.Direction == EOrbitGravityDirection::SplineDown) && Field.CurveUp.IsValidIndex(Segment + 1))
        {
            Direction = FMath::Lerp(Field.CurveUp[Segment], Field.CurveUp[Segment + 1], Alpha).GetSafeNormal();
            if (Field.Direction == EOrbitGravityDirection::SplineDown) { Direction *= -1.0; }
        }
        break;
    }
    }

    if (Field.MaxDistance > 0.0 && Distance > Field.MaxDistance) { return false; }
    if ((Field.Shape == EOrbitGravityShape::Point || Field.Shape == EOrbitGravityShape::Axis || Field.Shape == EOrbitGravityShape::Spline) &&
        (Field.Direction == EOrbitGravityDirection::Inward || bVortexInward))
    {
        Direction *= -1.0;
    }
    else if ((Field.Shape == EOrbitGravityShape::Directional || Field.Shape == EOrbitGravityShape::Volume) &&
        (Field.Direction == EOrbitGravityDirection::Outward || bVortexOutward))
    {
        Direction *= -1.0;
    }

    // Mix radial attraction/repulsion with tangential acceleration, then
    // normalize so Strength continues to mean TOTAL acceleration magnitude.
    // 0 spin is exactly radial; +1 is a 45-degree spiral on the axis.
    // For Point, the tangent shrinks smoothly toward the spin-axis poles.
    if (bVortex && (Field.Shape == EOrbitGravityShape::Point ||
                    Field.Shape == EOrbitGravityShape::Axis ||
                    Field.Shape == EOrbitGravityShape::Spline))
    {
        const double Spin = FMath::Clamp(Field.VortexSpin, -10.0, 10.0);
        Direction = (Direction + ClockwiseTangent * Spin).GetSafeNormal();
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

bool FOrbitFieldEvaluator::EvaluateForBody(const FOrbitFieldDefinition& Field,
    const FVector& Position, const FVector& Velocity, FVector& OutAcceleration)
{
    // Keep the ordinary field evaluator authoritative for source bounds, strength,
    // falloff, and direction. No body may be captured by an uninfluential field.
    if (!Evaluate(Field, Position, OutAcceleration)) { return false; }
    if (Field.Direction != EOrbitGravityDirection::VortexInward ||
        Field.VortexPhysicsMode != EOrbitVortexPhysicsMode::SpiralCapture ||
        FMath::Abs(Field.VortexSpin) <= UE_DOUBLE_SMALL_NUMBER ||
        (Field.Shape != EOrbitGravityShape::Point && Field.Shape != EOrbitGravityShape::Axis &&
         Field.Shape != EOrbitGravityShape::Spline) || Velocity.ContainsNaN())
    {
        return true;
    }

    // Resolve a locally radial, tangential, and along-axis frame.
    // Point gravity uses full 3D velocity. Axis and spline fields leave
    // along-axis travel untouched, so e.g. a character can travel down a tube.
    FVector Radial = FVector::ZeroVector;
    FVector Axis = Field.Axis.GetSafeNormal();
    bool bPreserveAxisVelocity = false;
    if (Field.Shape == EOrbitGravityShape::Point)
    {
        Radial = Position - Field.Center;
    }
    else if (Field.Shape == EOrbitGravityShape::Axis)
    {
        if (Axis.IsNearlyZero()) { return true; }
        const FVector Offset = Position - Field.Center;
        Radial = Offset - Axis * FVector::DotProduct(Offset, Axis);
        bPreserveAxisVelocity = true;
    }
    else
    {
        int32 Segment = INDEX_NONE;
        double Alpha = 0.0;
        const FVector Closest = ClosestOnCurve(Field.Curve, Position, Segment, Alpha);
        if (Segment == INDEX_NONE || !Field.Curve.IsValidIndex(Segment + 1)) { return true; }
        Axis = (Field.Curve[Segment + 1] - Field.Curve[Segment]).GetSafeNormal();
        if (Axis.IsNearlyZero()) { return true; }
        Radial = Position - Closest;
        bPreserveAxisVelocity = true;
    }

    const double Radius = Radial.Size();
    // The ordinary vector is zero *at* the singularity, but the authored
    // strength is still available to brake a body crossing the centerline.
    const double AvailableAcceleration = Radius <= 1e-3
        ? FMath::Max(0.0, Field.Strength) : OutAcceleration.Size();
    if (AvailableAcceleration <= UE_DOUBLE_SMALL_NUMBER) { return true; }
    const double Response = FMath::Clamp(Field.VortexResponseRate, 0.1, 30.0);
    // At the centerline there is no unique radial/tangential basis. Brake only
    // transverse velocity so bodies cannot slingshot from the singularity.
    if (Radius <= 1e-3)
    {
        const FVector TransverseVelocity = bPreserveAxisVelocity
            ? Velocity - Axis * FVector::DotProduct(Velocity, Axis) : Velocity;
        OutAcceleration = (-TransverseVelocity * Response).GetClampedToMaxSize(
            AvailableAcceleration * FMath::Clamp(Field.VortexMaxAccelerationFactor, 0.1, 20.0));
        return !OutAcceleration.ContainsNaN();
    }

    const FVector Outward = Radial / Radius;
    const FVector Tangent = FVector::CrossProduct(Outward, Axis).GetSafeNormal();
    const double InwardSpeed = FMath::Max(0.0, Field.VortexConvergenceRate) * Radius;
    const double TangentialSpeed = FMath::Max(0.0, Field.VortexAngularRateRadians) *
        Radius * FMath::Abs(Field.VortexSpin);
    FVector DesiredVelocity = -Outward * InwardSpeed +
        Tangent * (Field.VortexSpin >= 0.0 ? TangentialSpeed : -TangentialSpeed);
    DesiredVelocity = DesiredVelocity.GetClampedToMaxSize(FMath::Max(1.0, Field.VortexMaxFlowSpeed));

    const FVector MeasuredVelocity = bPreserveAxisVelocity
        ? Velocity - Axis * FVector::DotProduct(Velocity, Axis) : Velocity;
    const double MaxSteeringAcceleration = AvailableAcceleration *
        FMath::Clamp(Field.VortexMaxAccelerationFactor, 0.1, 20.0);
    OutAcceleration = ((DesiredVelocity - MeasuredVelocity) * Response)
        .GetClampedToMaxSize(MaxSteeringAcceleration);
    return !OutAcceleration.ContainsNaN();
}

static FOrbitGravitySample CombineFieldsInternal(const TArray<FOrbitFieldDefinition>& Fields, const FVector& Position,
    EOrbitGravityChannel Channel, uint8 ReceiverMask, const FVector* BodyVelocity)
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
        if ((Field.ChannelMask & ReceiverMask) == 0u ||
            (Channel == EOrbitGravityChannel::Physics && !Field.bPhysics) ||
            (Channel == EOrbitGravityChannel::Character && !Field.bCharacters)) { continue; }
        FVector Acceleration;
        if (!(BodyVelocity ? FOrbitFieldEvaluator::EvaluateForBody(Field, Position, *BodyVelocity, Acceleration)
                            : FOrbitFieldEvaluator::Evaluate(Field, Position, Acceleration))) { continue; }
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
            (Field.ChannelMask & ReceiverMask) == 0u ||
            (bOverrideFound && Field.Priority <= OverridePriority) ||
            (Channel == EOrbitGravityChannel::Physics && !Field.bPhysics) ||
            (Channel == EOrbitGravityChannel::Character && !Field.bCharacters)) { continue; }
        FVector Acceleration;
        if (!(BodyVelocity ? FOrbitFieldEvaluator::EvaluateForBody(Field, Position, *BodyVelocity, Acceleration)
                            : FOrbitFieldEvaluator::Evaluate(Field, Position, Acceleration))) { continue; }
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


FOrbitGravitySample FOrbitFieldEvaluator::Combine(const TArray<FOrbitFieldDefinition>& Fields,
    const FVector& Position, EOrbitGravityChannel Channel, uint8 ReceiverMask)
{
    return CombineFieldsInternal(Fields, Position, Channel, ReceiverMask, nullptr);
}

FOrbitGravitySample FOrbitFieldEvaluator::CombineForBody(const TArray<FOrbitFieldDefinition>& Fields,
    const FVector& Position, const FVector& Velocity, uint8 ReceiverMask)
{
    return CombineFieldsInternal(Fields, Position, EOrbitGravityChannel::Physics, ReceiverMask, &Velocity);
}

int32 FOrbitFieldEvaluator::TraceStreamline(const FOrbitFieldDefinition& Field,
    const FVector& Seed, double StepDistance, int32 MaxSteps, TArray<FVector>& OutPoints)
{
    OutPoints.Reset();
    if (Seed.ContainsNaN() || !FMath::IsFinite(StepDistance) || StepDistance <= 0.0 || MaxSteps <= 0)
    {
        return 0;
    }

    FVector Position = Seed;
    FVector Acceleration;
    if (!Evaluate(Field, Position, Acceleration) || Acceleration.IsNearlyZero()) { return 0; }
    OutPoints.Add(Position);

    const int32 Count = FMath::Clamp(MaxSteps, 0, 512);
    for (int32 Step = 0; Step < Count; ++Step)
    {
        FVector AtStart, AtMiddle, AtEnd;
        if (!Evaluate(Field, Position, AtStart) || AtStart.IsNearlyZero()) { break; }
        const FVector Midpoint = Position + AtStart.GetSafeNormal() * (0.5 * StepDistance);
        if (!Evaluate(Field, Midpoint, AtMiddle) || AtMiddle.IsNearlyZero()) { break; }
        const FVector Heading = AtMiddle.GetSafeNormal();
        const FVector Next = Position + Heading * StepDistance;
        if (Next.ContainsNaN() || !Evaluate(Field, Next, AtEnd) || AtEnd.IsNearlyZero()) { break; }
        // Crossing a radial singularity can reverse the acceleration vector.
        // Never let the visualizer ping-pong across the center of a field.
        if (FVector::DotProduct(AtEnd.GetSafeNormal(), Heading) < -0.5) { break; }
        OutPoints.Add(Next);
        Position = Next;
    }
    return OutPoints.Num() - 1;
}
