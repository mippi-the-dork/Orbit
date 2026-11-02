// Copyright Mippithedork 2026, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OrbitGravityTypes.h"

/** Pure decision policy. Keeping the zero-g and exit behavior separate makes it testable without an active Mover backend. */
struct ORBITMOVER_API FOrbitMoverGravityDecision
{
    bool bOverrideGravity = false;
    bool bOverrideUp = false;
    FVector Acceleration = FVector::ZeroVector;
    FVector Up = FVector::UpVector;
};

class ORBITMOVER_API FOrbitMoverGravityPolicy
{
public:
    /** No Orbit influence relinquishes overrides. Zero-g *with* influence is a real zero acceleration override. */
    static FOrbitMoverGravityDecision Resolve(const FOrbitGravitySample& Sample, const FVector& LastStableUp);
};
