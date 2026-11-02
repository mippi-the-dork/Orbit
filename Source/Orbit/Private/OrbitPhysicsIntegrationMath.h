// Copyright Mippithedork 2026, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Small, testable gates shared by the runtime integration and the native tests.
// Never assume world Z is gravity Up for character support decisions.
namespace OrbitPhysicsIntegrationMath
{
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
