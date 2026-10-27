#include "OrbitMoverGravityPolicy.h"

FOrbitMoverGravityDecision FOrbitMoverGravityPolicy::Resolve(
    const FOrbitGravitySample& Sample, const FVector& LastStableUp)
{
    FOrbitMoverGravityDecision Decision;
    if (!Sample.bHasInfluence || Sample.Acceleration.ContainsNaN())
    {
        return Decision;
    }

    Decision.bOverrideGravity = true;
    Decision.Acceleration = Sample.Acceleration;
    Decision.bOverrideUp = true;
    if (Sample.bHasValidDirection && !Sample.Up.ContainsNaN() && !Sample.Up.IsNearlyZero())
    {
        Decision.Up = Sample.Up.GetSafeNormal();
    }
    else if (!LastStableUp.ContainsNaN() && !LastStableUp.IsNearlyZero())
    {
        Decision.Up = LastStableUp.GetSafeNormal();
    }
    else
    {
        Decision.Up = FVector::UpVector;
    }
    return Decision;
}
