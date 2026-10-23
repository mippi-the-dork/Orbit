#pragma once

#include "CoreMinimal.h"
#include "OrbitGravityTypes.generated.h"

UENUM(BlueprintType)
enum class EOrbitGravityShape : uint8
{
    Point UMETA(DisplayName="Point (Spherical)"),
    Axis UMETA(DisplayName="Axis (Cylindrical)"),
    Directional UMETA(DisplayName="Directional"),
    Volume UMETA(DisplayName="Volume (Box Directional)"),
    Spline UMETA(DisplayName="Spline (Curve / Torus)")
};

UENUM(BlueprintType)
enum class EOrbitGravityDirection : uint8
{
    Inward UMETA(DisplayName="Inward / Down"),
    Outward UMETA(DisplayName="Outward / Reversed"),
    SplineUp UMETA(DisplayName="Spline Up (Oriented)")
};

UENUM(BlueprintType)
enum class EOrbitGravityFalloff : uint8
{
    Constant,
    Linear,
    InverseSquare UMETA(DisplayName="Inverse Square")
};

UENUM(BlueprintType)
enum class EOrbitGravityCombine : uint8
{
    Additive,
    Override
};

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
    EOrbitGravityCombine Combine = EOrbitGravityCombine::Override;
    FVector Center = FVector::ZeroVector;
    FVector Axis = FVector::UpVector;
    FVector DirectionVector = FVector::DownVector;
    FQuat Rotation = FQuat::Identity;
    FVector BoxExtents = FVector(1000.0);
    TArray<FVector> Curve;
    TArray<FVector> CurveUp;
    double Strength = 980.0;
    double MaxDistance = 0.0; // 0 means unlimited
    double ReferenceDistance = 100.0;
    bool bLimitToBox = false;
    bool bPhysics = true;
    bool bCharacters = true;
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

/** Pure numerical evaluator, usable by gameplay, tests and future Chaos callbacks. */
class ORBIT_API FOrbitFieldEvaluator
{
public:
    static bool Evaluate(const FOrbitFieldDefinition& Field, const FVector& Position, FVector& OutAcceleration);
    static FOrbitGravitySample Combine(const TArray<FOrbitFieldDefinition>& Fields, const FVector& Position, EOrbitGravityChannel Channel = EOrbitGravityChannel::General);
    static FVector ClosestOnCurve(const TArray<FVector>& Curve, const FVector& Position, int32& OutSegment, double& OutAlpha);
};
