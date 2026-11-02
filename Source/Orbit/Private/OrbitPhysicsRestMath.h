// Copyright Mippithedork 2026, Inc. All Rights Reserved.

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

    /** Native damping must never reduce the owner's original physics settings. */
    inline float NativeSupportedDamping(float Original, float Supported)
    {
        return FMath::Max(FMath::Max(0.0f, Original), FMath::Max(0.0f, Supported));
    }

    /** Frame-independent support direction check. No world-Z or world-XY projection. */
    inline double SupportUpAlignment(const FVector& GravityDown, const FVector& SupportNormal)
    {
        const FVector Down = GravityDown.GetSafeNormal();
        const FVector Normal = SupportNormal.GetSafeNormal();
        return Down.IsNearlyZero() || Normal.IsNearlyZero() ? -1.0 :
            FVector::DotProduct(-Down, Normal);
    }

    /** A moving support can physically collide but must not become an artificial sleep anchor.
     *  All speeds are world-space magnitudes and independent of gravity axis. */
    inline bool IsStableDynamicSupport(bool bSimulating, const FVector& SupportVelocityAtContact,
        const FVector& SupportAngularVelocity, double MaxLinearSpeed, double MaxAngularSpeed)
    {
        return !bSimulating || (SupportVelocityAtContact.SizeSquared() <= FMath::Square(FMath::Max(0.0, MaxLinearSpeed)) &&
            SupportAngularVelocity.SizeSquared() <= FMath::Square(FMath::Max(0.0, MaxAngularSpeed)));
    }

    /** Prefer a stable candidate when normal quality differs by at most 0.10.
     *  Stronger, clearly better-aligned geometry still wins; allows legitimate slopes.
     *  This is a near-contact query ranking, not a Chaos contact-manifold decision. */
    inline bool PreferSupportCandidate(double CandidateAlignment, bool bCandidateStable,
        double CurrentAlignment, bool bCurrentStable, bool bHasCurrent)
    {
        if (CandidateAlignment < 0.70) { return false; }
        if (!bHasCurrent) { return true; }
        if (bCandidateStable && !bCurrentStable && CandidateAlignment + 0.10 >= CurrentAlignment)
        {
            return true;
        }
        if (!bCandidateStable && bCurrentStable && CurrentAlignment + 0.10 >= CandidateAlignment)
        {
            return false;
        }
        return CandidateAlignment > CurrentAlignment;
    }

    /** At most 2 optional extra geometry queries; never retry a verified hit.
     *  Rejected candidates are ignored ONLY for the next geometric query. */
    inline bool ShouldRetrySupportProbe(bool bHasSupportedHit, bool bHasRejectedBlocker,
        int32 AttemptIndex, int32 ConfiguredRetryCount)
    {
        return !bHasSupportedHit && bHasRejectedBlocker &&
            AttemptIndex < FMath::Clamp(ConfiguredRetryCount, 0, 2);
    }

    /** Suppress repeated manual sleeping after an immediate post-sleep wake, without
     *  suppressing gravity forces, physical wakes, or support-independent collision. */
    inline bool IsImmediateManualSleepWake(double Now, double RequestedSleepAt, double WindowSeconds = 0.15)
    {
        return RequestedSleepAt >= 0.0 && Now >= RequestedSleepAt &&
            Now - RequestedSleepAt <= FMath::Max(0.0, WindowSeconds);
    }

    inline bool MayRequestManualSleep(bool bFreshSupport, bool bSupportStable, bool bSameSupport,
        bool bSlowBody, double Now, double RetryAfter)
    {
        return bFreshSupport && bSupportStable && bSameSupport && bSlowBody && Now >= RetryAfter;
    }

    /** Repeated immediate wakeups should stop *manual* sleep requests for the
     *  current rest episode; native Chaos sleeping and all external wakes remain enabled. */
    inline int32 NextManualSleepFailureCount(int32 Previous, bool bImmediateWake, int32 MaxFailures)
    {
        return bImmediateWake ? FMath::Min(FMath::Max(0, Previous) + 1,
            FMath::Max(1, MaxFailures)) : FMath::Max(0, Previous);
    }

    inline bool SuppressManualSleep(int32 FailureCount, int32 MaxFailures)
    {
        return FailureCount >= FMath::Max(1, MaxFailures);
    }

    /** Normal solver velocity can remain nonzero under sustained gravity contact;
     *  allow it only if *actual* displacement, tangential velocity, and angular
     *  velocity stay low over the same support episode. No gravity cancellation. */
    inline bool IsStationaryRestCandidate(const FVector& LinearVelocity, const FVector& AngularVelocity,
        const FVector& Down, const FVector& CurrentPosition, const FVector& AnchorPosition,
        double MaxLinearSpeed, double MaxAngularSpeed, double MaxSolverNormalSpeed,
        double MaxStableDisplacement)
    {
        const FVector UnitDown = Down.GetSafeNormal();
        if (UnitDown.IsNearlyZero() ||
            AngularVelocity.SizeSquared() > FMath::Square(FMath::Max(0.0, MaxAngularSpeed)) ||
            FVector::DistSquared(CurrentPosition, AnchorPosition) > FMath::Square(FMath::Max(0.0, MaxStableDisplacement)))
        {
            return false;
        }
        if (LinearVelocity.SizeSquared() <= FMath::Square(FMath::Max(0.0, MaxLinearSpeed)))
        {
            return true;
        }
        return TangentialVelocity(LinearVelocity, Down).SizeSquared() <=
                FMath::Square(FMath::Max(0.0, MaxLinearSpeed)) &&
            FMath::Abs(FVector::DotProduct(LinearVelocity, UnitDown)) <=
                FMath::Max(0.0, MaxSolverNormalSpeed);
    }

    inline bool ShouldRearmManualSleep(bool bSuppressed, double UnstableDuration, double RequiredDuration)
    {
        return bSuppressed && UnstableDuration >= FMath::Max(0.0, RequiredDuration);
    }

    /** Stable suppression can recover without requiring a collision or field change.
     *  The cooldown and a continuous stability window must BOTH complete. */
    inline bool ShouldRecoverSuppressedSleep(bool bSuppressed, bool bStableNow,
        double StableSeconds, double RequiredStableSeconds, double Now, double RetryAfter)
    {
        return bSuppressed && bStableNow &&
            StableSeconds >= FMath::Max(0.0, RequiredStableSeconds) && Now >= RetryAfter;
    }

    /** A recovered body gets a single probationary request: one more immediate
     *  wake returns it to suppression, rather than replaying the original burst. */
    inline int32 ProbationaryFailureCount(int32 MaxFailures)
    {
        return FMath::Max(0, FMath::Max(1, MaxFailures) - 1);
    }

    /** Only simple sphere collision gets a broader position envelope; other
     *  primitives retain the existing strict non-sliding rest threshold. */
    inline float RestDisplacementBudget(bool bSimpleSphere, float Generic, float Sphere)
    {
        return FMath::Max(0.0f, bSimpleSphere ? Sphere : Generic);
    }

    /** A cached side blocker must remain close and in the same gravity frame.
     *  Cache validity is geometric and time-bounded, never based on Actor label. */
    inline bool MayUseCachedSideObstruction(bool bHasBlocker, bool bHasRecoveredSupport,
        double Now, double ExpiryTime, const FVector& CurrentBodyPosition,
        const FVector& CachedBodyPosition, const FVector& CurrentBlockerPosition,
        const FVector& CachedBlockerPosition, const FQuat& CurrentBlockerRotation,
        const FQuat& CachedBlockerRotation, const FVector& CurrentDown,
        const FVector& CachedDown)
    {
        return bHasBlocker && bHasRecoveredSupport && Now < ExpiryTime &&
            FVector::DistSquared(CurrentBodyPosition, CachedBodyPosition) <= FMath::Square(0.5) &&
            FVector::DistSquared(CurrentBlockerPosition, CachedBlockerPosition) <= FMath::Square(0.5) &&
            CurrentBlockerRotation.Equals(CachedBlockerRotation, 0.01) &&
            FVector::DotProduct(CurrentDown.GetSafeNormal(), CachedDown.GetSafeNormal()) >= 0.98;
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
