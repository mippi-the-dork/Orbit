#include "OrbitGravityFrame.h"

#include "Math/RotationMatrix.h"

bool FOrbitGravityFrame::MakeTangentBasis(const FVector& Up, const FVector& ReferenceForward,
    FVector& OutForward, FVector& OutRight)
{
    OutForward = FVector::ZeroVector;
    OutRight = FVector::ZeroVector;
    if (Up.ContainsNaN() || Up.IsNearlyZero() || ReferenceForward.ContainsNaN())
    {
        return false;
    }

    const FVector NormalUp = Up.GetSafeNormal();
    OutForward = FVector::VectorPlaneProject(ReferenceForward, NormalUp).GetSafeNormal();
    if (OutForward.IsNearlyZero())
    {
        // A heading parallel to Up is undefined. Select the least-parallel cardinal axis,
        // rather than producing an invalid rotation or flipping between almost-parallel axes.
        const FVector Fallback = FMath::Abs(NormalUp.X) < 0.75 ? FVector::ForwardVector : FVector::RightVector;
        OutForward = FVector::VectorPlaneProject(Fallback, NormalUp).GetSafeNormal();
    }
    OutRight = FVector::CrossProduct(NormalUp, OutForward).GetSafeNormal();
    return !OutForward.IsNearlyZero() && !OutRight.IsNearlyZero() &&
        !OutForward.ContainsNaN() && !OutRight.ContainsNaN();
}

bool FOrbitGravityFrame::MakeGravityFacingRotation(const FVector& Up,
    const FVector& DesiredHeading, FQuat& OutRotation)
{
    OutRotation = FQuat::Identity;
    FVector Forward, Right;
    if (!MakeTangentBasis(Up, DesiredHeading, Forward, Right)) { return false; }
    OutRotation = FRotationMatrix::MakeFromXZ(Forward, Up.GetSafeNormal()).ToQuat().GetNormalized();
    return !OutRotation.ContainsNaN();
}

bool FOrbitGravityFrame::MakeAlignedRotation(const FVector& Up, const FQuat& CurrentRotation, FQuat& OutRotation)
{
    OutRotation = CurrentRotation;
    if (CurrentRotation.ContainsNaN()) { return false; }

    FVector TangentForward;
    FVector TangentRight;
    // For a near-vertical heading, try the existing right direction first so heading
    // remains stable while walking over the poles of a small planet.
    if (!MakeTangentBasis(Up, CurrentRotation.GetForwardVector(), TangentForward, TangentRight))
    {
        return false;
    }
    const FVector NormalUp = Up.GetSafeNormal();
    if (FMath::Abs(FVector::DotProduct(CurrentRotation.GetForwardVector(), NormalUp)) > 0.999)
    {
        const FVector ExistingRight = FVector::VectorPlaneProject(CurrentRotation.GetRightVector(), NormalUp).GetSafeNormal();
        if (!ExistingRight.IsNearlyZero())
        {
            TangentForward = FVector::CrossProduct(ExistingRight, NormalUp).GetSafeNormal();
        }
    }

    OutRotation = FRotationMatrix::MakeFromXZ(TangentForward, NormalUp).ToQuat().GetNormalized();
    return !OutRotation.ContainsNaN();
}

bool FOrbitGravityFrame::TransportTangentHeading(const FVector& PreviousUp,
    const FVector& PreviousForward, const FVector& NewUp, FVector& OutForward)
{
    OutForward = FVector::ZeroVector;
    if (PreviousUp.ContainsNaN() || NewUp.ContainsNaN() || PreviousForward.ContainsNaN() ||
        PreviousUp.IsNearlyZero() || NewUp.IsNearlyZero() || PreviousForward.IsNearlyZero())
    {
        return false;
    }
    const FVector FromUp = PreviousUp.GetSafeNormal();
    const FVector ToUp = NewUp.GetSafeNormal();
    const FQuat Transport = FQuat::FindBetweenNormals(FromUp, ToUp);
    const FVector Transported = Transport.RotateVector(PreviousForward);
    FVector Right;
    return MakeTangentBasis(ToUp, Transported, OutForward, Right);
}

bool FOrbitGravityFrame::ChooseGravityRelativeFacing(const FVector& Up, const FVector& Velocity,
    const FVector& RequestedHeading, bool bHasRecentInput, bool bPreferVelocity,
    double VelocityThreshold, FVector& OutHeading)
{
    OutHeading = FVector::ZeroVector;
    if (Up.ContainsNaN() || Up.IsNearlyZero() || Velocity.ContainsNaN() ||
        RequestedHeading.ContainsNaN() || !FMath::IsFinite(VelocityThreshold))
    {
        return false;
    }
    const FVector NormalUp = Up.GetSafeNormal();
    if (bPreferVelocity)
    {
        const FVector TangentVelocity = FVector::VectorPlaneProject(Velocity, NormalUp);
        if (TangentVelocity.SizeSquared() > FMath::Square(FMath::Max(0.0, VelocityThreshold)))
        {
            OutHeading = TangentVelocity.GetSafeNormal();
            return !OutHeading.IsNearlyZero();
        }
    }
    if (bHasRecentInput)
    {
        OutHeading = FVector::VectorPlaneProject(RequestedHeading, NormalUp).GetSafeNormal();
        return !OutHeading.IsNearlyZero();
    }
    return false;
}

bool FOrbitGravityFrame::ShouldAttemptNearFloorLanding(bool bFalling, double VerticalVelocityUp,
    double HorizontalSpeed, double MaxApproachSpeed, double FloorDistance, double MaxLandingDistance)
{
    return bFalling && FMath::IsFinite(VerticalVelocityUp) &&
        FMath::IsFinite(HorizontalSpeed) && FMath::IsFinite(MaxApproachSpeed) &&
        FMath::IsFinite(FloorDistance) && FMath::IsFinite(MaxLandingDistance) &&
        VerticalVelocityUp <= 0.0 &&
        FMath::Abs(VerticalVelocityUp) <= MaxApproachSpeed &&
        HorizontalSpeed <= MaxApproachSpeed &&
        FloorDistance >= 0.0 && MaxLandingDistance > 0.0 &&
        FloorDistance <= MaxLandingDistance;
}

bool FOrbitGravityFrame::MakeGravityScale(double DesiredAcceleration, double WorldGravityMagnitude,
    double MaxGravityScale, float& OutScale)
{
    OutScale = 0.0f;
    if (!FMath::IsFinite(DesiredAcceleration) || !FMath::IsFinite(WorldGravityMagnitude) ||
        !FMath::IsFinite(MaxGravityScale) || WorldGravityMagnitude <= UE_DOUBLE_SMALL_NUMBER ||
        MaxGravityScale < 0.0)
    {
        return false;
    }
    OutScale = static_cast<float>(FMath::Clamp(FMath::Max(0.0, DesiredAcceleration) / WorldGravityMagnitude,
        0.0, MaxGravityScale));
    return true;
}


bool FOrbitGravityFrame::CalculateGravityRelativeLocomotion(const FVector& Velocity,
    const FVector& Up, const FVector& ActorForward, double& OutSpeed,
    double& OutForwardSpeed, double& OutRightSpeed, double& OutDirectionDegrees)
{
    OutSpeed = OutForwardSpeed = OutRightSpeed = OutDirectionDegrees = 0.0;
    if (Velocity.ContainsNaN()) { return false; }
    FVector Forward, Right;
    if (!MakeTangentBasis(Up, ActorForward, Forward, Right)) { return false; }
    const FVector TangentVelocity = FVector::VectorPlaneProject(Velocity, Up.GetSafeNormal());
    OutSpeed = TangentVelocity.Size();
    OutForwardSpeed = FVector::DotProduct(TangentVelocity, Forward);
    OutRightSpeed = FVector::DotProduct(TangentVelocity, Right);
    if (OutSpeed > UE_DOUBLE_SMALL_NUMBER)
    {
        OutDirectionDegrees = FMath::RadiansToDegrees(FMath::Atan2(OutRightSpeed, OutForwardSpeed));
    }
    return FMath::IsFinite(OutSpeed) && FMath::IsFinite(OutForwardSpeed) &&
        FMath::IsFinite(OutRightSpeed) && FMath::IsFinite(OutDirectionDegrees);
}
