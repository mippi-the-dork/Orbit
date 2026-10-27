#pragma once

#include "CoreMinimal.h"

/** Gravity-relative orientation helpers shared by runtime components and Blueprint queries.
 *  These helpers calculate orientation only. They do not move actors or alter camera controls.
 */
class ORBIT_API FOrbitGravityFrame
{
public:
    /** Keeps ReferenceForward projected onto the tangent plane; returns false for invalid Up. */
    static bool MakeTangentBasis(const FVector& Up, const FVector& ReferenceForward,
        FVector& OutForward, FVector& OutRight);

    /** Aligns local +Z to Up while preserving as much of CurrentRotation's forward heading as possible. */
    static bool MakeAlignedRotation(const FVector& Up, const FQuat& CurrentRotation, FQuat& OutRotation);

    /** Compute locomotion in a character's actual gravity-tangent plane (not world XY).
     *  Signed direction is +right, -left; zero means movement along the actor's forward axis. */
    static bool CalculateGravityRelativeLocomotion(const FVector& Velocity, const FVector& Up,
        const FVector& ActorForward, double& OutSpeed, double& OutForwardSpeed,
        double& OutRightSpeed, double& OutDirectionDegrees);

    /** Safe test of whether native near-floor landing assistance can even be considered.
     *  The movement component MUST still validate a collision sweep and walkable landing hit. */
    static bool ShouldAttemptNearFloorLanding(bool bFalling, double VerticalVelocityUp,
        double HorizontalSpeed, double MaxApproachSpeed, double FloorDistance,
        double MaxLandingDistance);

    /** Converts acceleration magnitude to CharacterMovement.GravityScale relative to native WorldSettings gravity.
     *  Does not invent an acceleration when world gravity is zero.
     */
    static bool MakeGravityScale(double DesiredAcceleration, double WorldGravityMagnitude,
        double MaxGravityScale, float& OutScale);
};
