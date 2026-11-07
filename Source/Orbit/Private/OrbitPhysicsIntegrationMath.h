#pragma once

#include "CoreMinimal.h"

// Small, testable gates shared by the runtime integration and the native tests.
// Never assume world Z is gravity Up for character support decisions.
namespace OrbitPhysicsIntegrationMath
{
    // Startup claim must be a separate, explicitly enabled experiment. Standard
    // and existing Experimental projects do not change their first-frame behavior.
    inline bool ShouldPreclaimExperimentalGravity(bool bPhysicsEnabled, bool bAutomaticBodies,
        bool bExperimentalMethod, bool bEarlyOwnershipOptIn, bool bEligibleSolverField)
    {
        return bPhysicsEnabled && bAutomaticBodies && bExperimentalMethod &&
            bEarlyOwnershipOptIn && bEligibleSolverField;
    }

    inline bool CanPreclaimExperimentalBody(bool bRegistered, bool bSimulating,
        bool bOriginallyGravityEnabled, bool bIgnored, bool bInMatchingField)
    {
        return bRegistered && bSimulating && bOriginallyGravityEnabled &&
            !bIgnored && bInMatchingField;
    }

    // The separate multi-source experiment must never activate merely because
    // a user previously opted in to single-source Chaos gravity.
    inline bool ShouldActivateMultiplePointGravity(bool bExperimentalMethod,
        bool bMultiplePointOptIn, bool bEligibleFields)
    {
        return bExperimentalMethod && bMultiplePointOptIn && bEligibleFields;
    }

    // Selection priority is shared by startup preclaim and per-tick ownership.
    // A new mixed configuration must never silently activate just because
    // existing users enabled the older multiple-Point option.
    enum class EExperimentalSolverSelection : uint8
    {
        None,
        Single,
        MultiplePoint,
        MixedPointDirectional,
        ExpandedFields
    };

    inline EExperimentalSolverSelection SelectExperimentalSolver(bool bExperimentalMethod,
        bool bSingleEligible, bool bMultiPointOptIn, bool bMultiPointEligible,
        bool bMixedOptIn, bool bMixedEligible,
        bool bExpandedOptIn = false, bool bExpandedEligible = false)
    {
        if (!bExperimentalMethod) { return EExperimentalSolverSelection::None; }
        if (bSingleEligible) { return EExperimentalSolverSelection::Single; }
        if (bMultiPointOptIn && bMultiPointEligible) { return EExperimentalSolverSelection::MultiplePoint; }
        if (bMixedOptIn && bMixedEligible) { return EExperimentalSolverSelection::MixedPointDirectional; }
        // Last in priority: never changes the existing three experimental modes.
        if (bExpandedOptIn && bExpandedEligible) { return EExperimentalSolverSelection::ExpandedFields; }
        return EExperimentalSolverSelection::None;
    }

    inline bool ShouldLimitStandingForce(bool bEnabled, bool bInsideOrbitField,
        bool bWalking, bool bBaseSimulating, bool bBaseManagedByOrbit)
    {
        return bEnabled && bInsideOrbitField && bWalking &&
            bBaseSimulating && bBaseManagedByOrbit;
    }

    // Only possible for truly world-aligned, constant-magnitude gravity at a body.
    // The native comparison mode is deliberately not a substitute for spherical gravity.
    inline bool CanCompareWithNativeWorldGravity(bool bEnabled, bool bWasNativeGravityEnabled,
        bool bHasInfluence, const FVector& OrbitAcceleration, double WorldGravityZ)
    {
        if (!bEnabled || !bWasNativeGravityEnabled || !bHasInfluence ||
            !FMath::IsFinite(WorldGravityZ) || FMath::IsNearlyZero(WorldGravityZ) ||
            OrbitAcceleration.ContainsNaN())
        {
            return false;
        }
        const FVector NativeAcceleration(0.0, 0.0, WorldGravityZ);
        // Explicit numerical guard prevents switching delivery modes due to tiny field noise.
        return FVector::DistSquared(OrbitAcceleration, NativeAcceleration) <= 1.0;
    }
}
