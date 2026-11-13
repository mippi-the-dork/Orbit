#pragma once

#include "CoreMinimal.h"
#include "OrbitSplineSampling.h"
#include "Components/SceneComponent.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravitySourceComponent.generated.h"

class USplineComponent;
struct FPropertyChangedEvent;

/** Embeds a complete Orbit gravity field inside any Actor. The field follows this
 * component's WORLD transform, independently of its owner Actor's pivot. */
UCLASS(ClassGroup=(Orbit), BlueprintType, meta=(BlueprintSpawnableComponent))
class ORBIT_API UOrbitGravitySourceComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UOrbitGravitySourceComponent();
    virtual void PostLoad() override;
    virtual void OnComponentCreated() override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Enables this gravity source. Disabled sources do not influence characters or physics bodies."))
    bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Geometry used to define the gravity field. Spline and Volume reveal additional shape-specific settings."))
    EOrbitGravityShape Shape = EOrbitGravityShape::Point;
    /** For Volume, this controls direction only in Directional (Legacy) mode. Other Volume Direction Modes take precedence; the saved value is preserved. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Shape != EOrbitGravityShape::Volume || VolumeDirectionMode == EOrbitVolumeDirectionMode::Directional", ToolTip="Gravity direction for non-Volume fields and Directional (Legacy) volumes. For other Volume Direction Modes, edit the mode under Orbit | Volume; this value is retained but inactive."))
    EOrbitGravityDirection Direction = EOrbitGravityDirection::Inward;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="0.0", Units="cm/s^2", ToolTip="Acceleration applied by this source in centimeters per second squared. Standard Earth gravity is approximately 980."))
    double Strength = 980.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="How gravity strength changes with distance from the source. Constant applies the same strength throughout the field."))
    EOrbitGravityFalloff Falloff = EOrbitGravityFalloff::Constant;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="0.0", Units="cm", ToolTip="Optional field reach in centimeters. Zero means no additional distance limit; Volume fields are also bounded by their authored geometry."))
    /** For Volume, caps selected shape dimensions, not a spherical intersection. */
    double MaxDistance = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="1.0", Units="cm", ToolTip="Distance in centimeters used to normalize inverse-square falloff. Only affects Inverse Square falloff.", EditCondition="Falloff == EOrbitGravityFalloff::InverseSquare", EditConditionHides))
    double InverseSquareReferenceDistance = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="How this field interacts with other overlapping sources: replace the current field or contribute according to the selected combining mode."))
    EOrbitGravityCombine Combine = EOrbitGravityCombine::Override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Precedence of this source when several fields overlap. Used by the field combination and competition rules."))
    int32 Priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="A source affects receivers when their channel masks overlap. Default is channel 0."))
    FOrbitGravityChannels GravityChannels;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Allow this field to affect eligible physics-simulated bodies."))
    bool bAffectPhysics = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Allow this field to affect characters using Orbit character gravity."))
    bool bAffectCharacters = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Restrict this field to an additional rotated box region. Separate from the shape of a Volume field."))
    bool bLimitToBox = false;
    UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Migrated to Box Corner A/B"))
    FVector BoxHalfExtents = FVector(1000.0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(MakeEditWidget, EditCondition="bLimitToBox || (Shape == EOrbitGravityShape::Volume && VolumeShape == EOrbitVolumeShape::Box)", ToolTip="First corner of the local-space box. Only the translation is used; rotation and scale are ignored."))
    FTransform BoxCornerA = FTransform(FVector(-1000.0));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(MakeEditWidget, EditCondition="bLimitToBox || (Shape == EOrbitGravityShape::Volume && VolumeShape == EOrbitVolumeShape::Box)", ToolTip="Opposite corner of the local-space box. Only the translation is used; rotation and scale are ignored."))
    FTransform BoxCornerB = FTransform(FVector(1000.0));
    UPROPERTY()
    bool bBoxCornersInitialized = false;
    void EnsureBoxCornersMigrated();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Boundary geometry for a Volume field. Does not change non-Volume shapes.", EditCondition="Shape == EOrbitGravityShape::Volume", EditConditionHides))
    EOrbitVolumeShape VolumeShape = EOrbitVolumeShape::Box;
    /** Governs gravity direction for Volumes. In nonlegacy modes, the Field Direction setting is inactive and retains its existing value. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="How a Volume determines gravity direction. Directional (Legacy) uses the regular Gravity Direction setting.", EditCondition="Shape == EOrbitGravityShape::Volume", EditConditionHides))
    EOrbitVolumeDirectionMode VolumeDirectionMode = EOrbitVolumeDirectionMode::Directional;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="1.0", Units="cm", ToolTip="Sphere, cylinder, or capsule radius in centimeters. Only applies to a Volume of a compatible shape.", EditCondition="Shape == EOrbitGravityShape::Volume && VolumeShape != EOrbitVolumeShape::Box", EditConditionHides))
    double VolumeRadius = 1000.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="1.0", Units="cm", ToolTip="Cylinder half-height or capsule half-height including caps, in centimeters. Only applies to those Volume shapes.", EditCondition="Shape == EOrbitGravityShape::Volume && (VolumeShape == EOrbitVolumeShape::Cylinder || VolumeShape == EOrbitVolumeShape::Capsule)", EditConditionHides))
    double VolumeHalfHeight = 1000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="-10.0", ClampMax="10.0", ToolTip="Signed rotation strength relative to radial gravity. Zero removes swirl; positive and negative values rotate in opposite directions.", EditCondition="Direction == EOrbitGravityDirection::VortexInward || Direction == EOrbitGravityDirection::VortexOutward", EditConditionHides))
    double VortexSpin = 1.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="How simulated rigid bodies respond to inward vortex gravity. Spiral Capture additionally steers their movement.", EditCondition="Direction == EOrbitGravityDirection::VortexInward", EditConditionHides))
    EOrbitVortexPhysicsMode VortexPhysicsMode = EOrbitVortexPhysicsMode::SpiralCapture;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Target inward convergence per second during Spiral Capture. Only applies to inward vortex physics.", EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", EditConditionHides))
    double VortexConvergenceRate = 0.5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Target spiral rotation rate in degrees per second during Spiral Capture.", EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", EditConditionHides))
    double VortexAngularRate = 63.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="How quickly simulated bodies steer toward the Spiral Capture flow.", EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", EditConditionHides))
    double VortexResponseRate = 6.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Upper limit in centimeters per second for Spiral Capture target flow speed.", EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", EditConditionHides))
    double VortexMaxFlowSpeed = 1500.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Maximum capture steering acceleration relative to the source gravity strength.", EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", EditConditionHides))
    double VortexMaxAccelerationFactor = 3.0;

    /** Optional spline on the same actor. Leave empty to use the first spline
     * found on the actor. Prefer an explicit reference with several splines. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Spline used to evaluate this field. When unassigned on a Source Component, Orbit finds a spline on the owning Actor.", EditCondition="Shape == EOrbitGravityShape::Spline", EditConditionHides))
    TObjectPtr<USplineComponent> GravitySpline = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="25.0", Units="cm", ToolTip="Desired maximum distance between world-space spline samples. Smaller values improve bend fidelity but cost more to build and query.", EditCondition="Shape == EOrbitGravityShape::Spline", EditConditionHides))
    double SplineSampleSpacing = 200.0;

    /** Upper bound for adaptive world-space spline segments. Existing fields keep the same spacing setting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="256", ClampMax="32768", UIMin="256", UIMax="32768", ToolTip="Maximum number of adaptive spline segments. Raising this improves complex spline fidelity but increases memory and query cost.", EditCondition="Shape == EOrbitGravityShape::Spline", EditConditionHides))
    int32 SplineMaxSegments = 8192;

    /** Legacy default preserves live procedural spline updates in previously saved levels.
     * New Actor/Component instances opt in to Automatic during creation.
     * Automatic polls every 0.1 s as a safety fallback for external spline edits;
     * Interval uses the configured duration; Manual requires explicit refresh.
     * The source transform and sampling settings are checked every frame in every mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Shape == EOrbitGravityShape::Spline", EditConditionHides, ToolTip="How often Orbit checks authored spline points for changes. Automatic checks at most 10 times/sec, Interval uses Check Interval, Manual only when refreshed; Every Frame preserves previous versions' behavior. Gravity itself still evaluates each frame."))
    EOrbitSplineUpdateMode SplineUpdateMode = EOrbitSplineUpdateMode::EveryFrame;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Shape == EOrbitGravityShape::Spline && SplineUpdateMode == EOrbitSplineUpdateMode::Interval", EditConditionHides, ClampMin="0.05", UIMin="0.05", UIMax="2.0", Units="s", ToolTip="Seconds between authored spline geometry checks. World transform and sample settings are still checked every frame."))
    double SplineCheckInterval = 0.25;

    /** Explicitly invalidate spline geometry after procedural edits, especially in Manual mode. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Spline", meta=(ToolTip="Force Orbit to rebuild the spline gravity geometry after Blueprint or C++ spline edits. In Manual mode, call this after editing spline points."))
    void RefreshSplineGravity();

    /** Transient sampling cache; no serialized state or gameplay configuration. */
    mutable FOrbitSplineSampleCache SplineGeometryCache;

    /** Rebuild a UObject-free field snapshot using this component's transform. */
    void BuildDefinition(FOrbitFieldDefinition& OutDefinition, bool bReuseSplineArrays = false) const;

    UFUNCTION(BlueprintCallable, Category="Orbit")
    void SetGravityStrength(double NewStrength);

    /** Single-component evaluation. Honors this source's channels and integration flags. */
    UFUNCTION(BlueprintPure, Category="Orbit")
    FOrbitGravitySample SampleSourceComponent(FVector WorldLocation) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
