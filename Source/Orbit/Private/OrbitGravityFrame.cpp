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
