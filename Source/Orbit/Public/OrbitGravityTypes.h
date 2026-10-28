#pragma once

#include "CoreMinimal.h"
#include "OrbitGravityTypes.generated.h"

UENUM(BlueprintType)
enum class EOrbitGravityShape : uint8
{
    Point UMETA(DisplayName="Point (Spherical)"),
    Axis UMETA(DisplayName="Axis (Cylindrical)"),
    Directional UMETA(DisplayName="Directional"),
    Volume UMETA(DisplayName="Volume (Directional)"),
    Spline UMETA(DisplayName="Spline (Curve / Torus)")
};

UENUM(BlueprintType)
enum class EOrbitGravityDirection : uint8
{
    Inward UMETA(DisplayName="Inward / Down"),
    Outward UMETA(DisplayName="Outward / Reversed"),
    SplineUp UMETA(DisplayName="Spline Up (Oriented)"),
    SplineDown UMETA(DisplayName="Spline Down (Oriented)"),
    /** Appended to retain existing serialized enum values for SplineUp/SplineDown. */
    VortexInward UMETA(DisplayName="Vortex Inward"),
    VortexOutward UMETA(DisplayName="Vortex Outward")
};

/** How a physics-simulated body responds to inward vortex fields. */
UENUM(BlueprintType)
enum class EOrbitVortexPhysicsMode : uint8
{
    /** Velocity-aware inward spiral. Steers bodies toward a converging rotational flow and avoids runaway tangential acceleration. */
    SpiralCapture UMETA(DisplayName="Spiral Capture (Recommended)"),
    /** Original v0.1.7 behavior. Tangential acceleration continuously adds energy and can eject bodies. */
    ContinuousForce UMETA(DisplayName="Continuous Force (Legacy)")
};

/** Boundary for a Volume gravity source. All shapes follow the source actor rotation. */
UENUM(BlueprintType)
enum class EOrbitVolumeShape : uint8
{
    Box,
    Sphere,
    Cylinder,
    Capsule
};

UENUM(BlueprintType)
enum class EOrbitGravityFalloff : uint8
{
    Constant,
    Linear,
    InverseSquare UMETA(DisplayName="Inverse Square")
};

/** How equal-priority Override sources compete. Explicit Priority always takes precedence. */
UENUM(BlueprintType)
enum class EOrbitOverrideResolution : uint8
{
    /** Compatibility: prefer the nearer center only when both sources are Point fields. */
    PriorityOverride UMETA(DisplayName="Priority Override (Legacy)"),
    /** Highest weighted effective gravity influence wins, with optional receiver hysteresis. */
    StrongestInfluence UMETA(DisplayName="Strongest Influence"),
    /** Average eligible acceleration vectors by weighted influence; opposing vectors can cancel. */
    BlendedInfluence UMETA(DisplayName="Blended Influence")
};

UENUM(BlueprintType)
enum class EOrbitGravityCombine : uint8
{
    Additive,
    Override
};

/**
 * Independent gravity-selection mask. Similar to Lighting Channels, channels are
 * NOT physics collision channels. Every source and receiver starts on channel 0.
 * Sources and receivers interact when any bit overlaps. An empty mask matches nothing.
 */
USTRUCT(BlueprintType)
struct ORBIT_API FOrbitGravityChannels
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Channels")
    bool bChannel0 = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Channels")
    bool bChannel1 = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Channels")
    bool bChannel2 = false;

    uint8 ToMask() const
    {
        return (bChannel0 ? 1u : 0u) | (bChannel1 ? 2u : 0u) | (bChannel2 ? 4u : 0u);
    }
};

/** Existing integration selector, NOT a gravity source/receiver channel. */
UENUM(BlueprintType)
enum class EOrbitGravityChannel : uint8
{
    General,
    Physics,
    Character
};

/** Field evaluation is an immutable, UObject-free snapshot. Units are Unreal cm and seconds. */
struct ORBIT_API FOrbitFieldDefinition
{
    EOrbitGravityShape Shape = EOrbitGravityShape::Point;
    EOrbitGravityDirection Direction = EOrbitGravityDirection::Inward;
    EOrbitGravityFalloff Falloff = EOrbitGravityFalloff::Constant;
    EOrbitVolumeShape VolumeShape = EOrbitVolumeShape::Box;
    EOrbitGravityCombine Combine = EOrbitGravityCombine::Override;
    FVector Center = FVector::ZeroVector;
    FVector Axis = FVector::UpVector;
    FVector DirectionVector = FVector::DownVector;
    FQuat Rotation = FQuat::Identity;
    FVector BoxExtents = FVector(1000.0);
    double VolumeRadius = 1000.0;
    double VolumeHalfHeight = 1000.0;
    TArray<FVector> Curve;
    TArray<FVector> CurveUp;
    double Strength = 980.0;
    /** Signed tangent-to-radial acceleration ratio. + = clockwise looking along -local Z (or -spline tangent). */
    double VortexSpin = 1.0;
    EOrbitVortexPhysicsMode VortexPhysicsMode = EOrbitVortexPhysicsMode::SpiralCapture;
    /** Fraction of current distance to travel inward per second in the desired flow. */
    double VortexConvergenceRate = 0.5;
    /** Angular rotation rate for abs(VortexSpin) = 1. In radians/second. */
    double VortexAngularRateRadians = 1.1;
    /** Velocity-correction rate in reciprocal seconds. */
    double VortexResponseRate = 6.0;
    /** Upper speed limit for the desired spiral flow (cm/sec). */
    double VortexMaxFlowSpeed = 1500.0;
    /** Maximum steering acceleration as a multiple of the gravity magnitude. */
    double VortexMaxAccelerationFactor = 3.0;
    // For Volumes a positive MaxDistance caps the volume's local extents/radius/half-height;
    // other source shapes retain their usual distance-based ranges.
    double MaxDistance = 0.0; // 0 means uncapped
    double ReferenceDistance = 100.0;
    bool bLimitToBox = false;
    bool bPhysics = true;
    bool bCharacters = true;
    uint8 ChannelMask = 1u; // Channel 0 by default, independent of physics/character filtering.
    int32 Priority = 0;
    int32 SourceId = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct ORBIT_API FOrbitGravitySample
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Orbit")
    FVector Acceleration = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Orbit")
    FVector Down = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Orbit")
    FVector Up = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category="Orbit")
    double Magnitude = 0.0;

    /** True even for a zero-gravity field, which explicitly overrides native gravity. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit")
    bool bHasInfluence = false;

    /** False at null points where a meaningful up/down direction does not exist. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit")
    bool bHasValidDirection = false;

    UPROPERTY(BlueprintReadOnly, Category="Orbit")
    int32 DominantSourceId = INDEX_NONE;
};

/** Effective, rotation-local Volume dimensions. Max Distance caps EACH relevant
 * shape dimension rather than intersecting the Volume with a world-space sphere.
 * Capsule HalfHeight includes the rounded end caps. */
struct ORBIT_API FOrbitEffectiveVolumeBounds
{
    FVector BoxHalfExtents = FVector::ZeroVector;
    double Radius = 0.0;
    double HalfHeight = 0.0;
};

/** Pure numerical evaluator, usable by gameplay, tests and future Chaos callbacks. */
class ORBIT_API FOrbitFieldEvaluator
{
public:
    /** Same effective geometry used by gameplay sampling and editor bounds. */
    static FOrbitEffectiveVolumeBounds GetEffectiveVolumeBounds(const FOrbitFieldDefinition& Field);
    static bool Evaluate(const FOrbitFieldDefinition& Field, const FVector& Position, FVector& OutAcceleration);
    /** Velocity-aware correction for simulated rigid bodies using Spiral Capture. Other directions/modes use Evaluate. */
    static bool EvaluateForBody(const FOrbitFieldDefinition& Field, const FVector& Position,
        const FVector& Velocity, FVector& OutAcceleration);
    static FOrbitGravitySample Combine(const TArray<FOrbitFieldDefinition>& Fields, const FVector& Position, EOrbitGravityChannel Channel = EOrbitGravityChannel::General, uint8 ReceiverMask = 1u,
        EOrbitOverrideResolution Resolution = EOrbitOverrideResolution::StrongestInfluence,
        int32 PreferredSourceId = INDEX_NONE, double SwitchHysteresis = 0.0);
    /** Applies velocity-aware vortex handling while preserving field priorities, channels and falloff. */
    static FOrbitGravitySample CombineForBody(const TArray<FOrbitFieldDefinition>& Fields, const FVector& Position,
        const FVector& Velocity, uint8 ReceiverMask = 1u,
        EOrbitOverrideResolution Resolution = EOrbitOverrideResolution::StrongestInfluence);
    static FVector ClosestOnCurve(const TArray<FVector>& Curve, const FVector& Position, int32& OutSegment, double& OutAlpha);

    /** Samples a single field line through the normalized acceleration field using
     * midpoint (RK2) integration. No momentum, collisions, or elapsed time are modeled.
     * Stops on null fields, singularities and influence boundaries. Output includes seed.
     * Returns number of segments traced (OutPoints.Num() - 1).
     */
    static int32 TraceStreamline(const FOrbitFieldDefinition& Field, const FVector& Seed,
        double StepDistance, int32 MaxSteps, TArray<FVector>& OutPoints);
};
