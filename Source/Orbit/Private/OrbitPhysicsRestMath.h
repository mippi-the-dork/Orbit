#pragma once

#include "CoreMinimal.h"

/** Friction-like decay applied only when a supporting collision has been verified. */
namespace OrbitPhysicsRestMath
{
    /** Removes movement toward or away from the support plane. No radial gravity is cancelled. */
    inline FVector TangentialVelocity(const FVector& Velocity, const FVector& GravityDown)
    {
        const FVector UnitDown = GravityDown.GetSafeNormal();
        return UnitDown.IsNearlyZero() ? FVector::ZeroVector :
            Velocity - FVector::DotProduct(Velocity, UnitDown) * UnitDown;
    }

    /** Acceleration in cm/s^2, opposed to lateral movement; not scaled by DeltaTime. */
    inline FVector TangentialDampingAcceleration(const FVector& Velocity, const FVector& GravityDown, double Rate)
    {
        return -TangentialVelocity(Velocity, GravityDown) * FMath::Max(0.0, Rate);
    }

    /** Angular acceleration in rad/s^2, opposed to all angular motion. */
    inline FVector AngularDampingAcceleration(const FVector& AngularVelocity, double Rate)
    {
        return -AngularVelocity * FMath::Max(0.0, Rate);
    }

    /** Frame-independent support direction check. No world-Z or world-XY projection. */
    inline double SupportUpAlignment(const FVector& GravityDown, const FVector& SupportNormal)
    {
        const FVector Down = GravityDown.GetSafeNormal();
        const FVector Normal = SupportNormal.GetSafeNormal();
        return Down.IsNearlyZero() || Normal.IsNearlyZero() ? -1.0 :
            FVector::DotProduct(-Down, Normal);
    }

    /** A recent hit may cover a missed sweep only if the body remains near the same support
     *  along gravity Up, is not lifting off, and the field direction has not reversed. */
    inline bool MayRetainSupport(const FVector& CurrentPosition, const FVector& PreviousSupportedPosition,
        const FVector& Velocity, const FVector& CurrentDown, const FVector& PreviousUp,
        double MaxSeparation, double MaxOutwardSpeed)
    {
        const FVector Up = (-CurrentDown).GetSafeNormal();
        const FVector PriorUp = PreviousUp.GetSafeNormal();
        if (Up.IsNearlyZero() || PriorUp.IsNearlyZero() ||
            FVector::DotProduct(Up, PriorUp) < 0.94)
        {
            return false;
        }
        const double UpwardSeparation = FVector::DotProduct(CurrentPosition - PreviousSupportedPosition, Up);
        const double OutwardSpeed = FVector::DotProduct(Velocity, Up);
        return UpwardSeparation <= FMath::Max(0.0, MaxSeparation) &&
            OutwardSpeed <= FMath::Max(0.0, MaxOutwardSpeed);
    }

    /** A fast inward/outward impact is not resting contact and must not be damped. */
    inline bool IsGentleContact(const FVector& Velocity, const FVector& GravityDown, double MaxNormalSpeed)
    {
        const FVector UnitDown = GravityDown.GetSafeNormal();
        return !UnitDown.IsNearlyZero() &&
            FMath::Abs(FVector::DotProduct(Velocity, UnitDown)) <= FMath::Max(0.0, MaxNormalSpeed);
    }
}
