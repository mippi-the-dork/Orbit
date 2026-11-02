#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravitySource.generated.h"

class USceneComponent;
class USplineComponent;
class UArrowComponent;
class UBillboardComponent;
struct FPropertyChangedEvent;

/** Author one gravity field. The Spline component is editable directly in the level viewport. */
UCLASS(Blueprintable, HideCategories=(Replication, Input, Collision))
class ORBIT_API AOrbitGravitySource : public AActor
{
    GENERATED_BODY()
public:
    AOrbitGravitySource();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Components")
    TObjectPtr<USplineComponent> GravitySpline;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Components")
    TObjectPtr<UArrowComponent> DirectionArrow;

#if WITH_EDITORONLY_DATA
    /** Viewport selection icon. Uses the supplied OrbitAsset_64 artwork. */
    UPROPERTY(VisibleAnywhere, Category="Orbit|Components", meta=(DisplayName="Orbit Billboard"))
    TObjectPtr<UBillboardComponent> OrbitBillboard;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field")
    EOrbitGravityShape Shape = EOrbitGravityShape::Point;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field")
    EOrbitGravityDirection Direction = EOrbitGravityDirection::Inward;

    /** Radial vortex modes only: signed tangent-to-radial acceleration ratio.
     * +1 = clockwise at 45 degrees; -1 = counterclockwise at 45 degrees.
     * Clockwise is viewed from the positive local axis toward its negative direction.
     * Zero gives pure radial inward/outward gravity. Strength still sets the total acceleration.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward || Direction == EOrbitGravityDirection::VortexOutward", ClampMin="-10.0", ClampMax="10.0", UIMin="-3.0", UIMax="3.0"))
    double VortexSpin = 1.0;

    /** Determines behavior of physics-simulated bodies in Vortex Inward fields.
     * Spiral Capture controls speed as radius decreases, preventing continuous-force ejection.
     * The instantaneous gravity query and field-line previews remain acceleration-only.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward"))
    EOrbitVortexPhysicsMode VortexPhysicsMode = EOrbitVortexPhysicsMode::SpiralCapture;

    /** Desired rate of inward convergence (per second) for Spiral Capture. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward", ClampMin="0.0", ClampMax="5.0", UIMin="0.0", UIMax="2.0"))
    double VortexConvergenceRate = 0.5;

    /** Desired rotation rate in degrees/second, multiplied by signed Vortex Spin. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward", ClampMin="0.0", ClampMax="720.0", UIMin="0.0", UIMax="180.0", Units="deg/s"))
    double VortexAngularRate = 63.0;

    /** Response rate for steering simulated bodies toward the inward spiral. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward", ClampMin="0.1", ClampMax="30.0", UIMin="1.0", UIMax="12.0"))
    double VortexResponseRate = 6.0;

    /** Caps the desired speed as a source-independent safety control. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward", ClampMin="1.0", Units="cm/s"))
    double VortexMaxFlowSpeed = 1500.0;

    /** Caps steering acceleration as a multiple of current gravitational acceleration. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward", ClampMin="0.1", ClampMax="20.0", UIMin="0.5", UIMax="6.0"))
    double VortexMaxAccelerationFactor = 3.0;

    /** In PIE, mirror direct editor-world Details edits to the corresponding
     *  runtime source when their stable actor identity matches. Editor-only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Runtime")
    bool bMirrorEditorChangesToPIE = true;

    /** Last number of matching PIE actor instances updated by an editor-world
     *  Details property edit. Visible only for debugging the editing workflow. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Debug|Runtime")
    int32 LastEditorPIEMirrorCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="0.0", Units="cm/s^2"))
    double Strength = 980.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field")
    EOrbitGravityFalloff Falloff = EOrbitGravityFalloff::Constant;

    /** For Volume, caps each dimension of the selected local shape (box, sphere,
     * cylinder or capsule). Other shapes retain their existing radial metric.
     * Zero means no additional cap beyond a Volume's authored dimensions.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="0.0", Units="cm"))
    double MaxDistance = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="1.0", Units="cm"))
    double InverseSquareReferenceDistance = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combining")
    EOrbitGravityCombine Combine = EOrbitGravityCombine::Override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combining")
    int32 Priority = 0;

    /** Lighting-style mask. Sources affect receivers when any selected channel matches. Default: channel 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Channels")
    FOrbitGravityChannels GravityChannels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    bool bAffectPhysics = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    bool bAffectCharacters = true;

    /** Additional rotated box mask, combined with any Volume boundary if present. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    bool bLimitToBox = false;

    /** Also configures the Box volume dimensions even when the additional box mask is disabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence", meta=(ClampMin="1.0", Units="cm"))
    FVector BoxHalfExtents = FVector(1000.0);

    /** Physical influence boundary for a Volume gravity source. Box preserves the existing BoxHalfExtents behavior. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Volume", meta=(EditCondition="Shape == EOrbitGravityShape::Volume"))
    EOrbitVolumeShape VolumeShape = EOrbitVolumeShape::Box;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Volume", meta=(EditCondition="Shape == EOrbitGravityShape::Volume", ClampMin="1.0", Units="cm"))
    double VolumeRadius = 1000.0;

    /** Cylinder half-height, or capsule total half-height including the caps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Volume", meta=(EditCondition="Shape == EOrbitGravityShape::Volume", ClampMin="1.0", Units="cm"))
    double VolumeHalfHeight = 1000.0;

    /** World-space spline sampling interval. Lower values improve bend fidelity at a cost. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Spline", meta=(ClampMin="25.0", Units="cm"))
    double SplineSampleSpacing = 200.0;

    /** Set strength on THIS world instance and publish it to gravity consumers immediately.
     *  Prefer this to an unqualified editor actor property change during PIE debugging.
     *  Does not replicate and does not change the saved editor-world actor. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Field|Runtime")
    void SetGravityStrength(double NewStrength);

    UFUNCTION(BlueprintCallable, Category="Orbit|Field")
    FOrbitGravitySample SampleSource(const FVector& WorldLocation) const;

    void BuildDefinition(FOrbitFieldDefinition& OutDefinition) const;

    /** Draws field-vector previews in level viewports and PIE. Editor visualization only; no gameplay effect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug")
    bool bShowGravityPreview = false;

    /** Viewport wireframe of the actual bounded volume, even with gravity arrows hidden. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug")
    bool bShowVolumeBounds = true;

    /** Visualizes the finite Max Distance boundary for Point, Axis, Spline and other sources.
     *  Axis boundaries are infinite along the axis; the displayed tube length is a visual slice.
     *  For Volume sources the blue boundary has the chosen Volume shape,
     *  with its effective dimensions capped by Max Distance.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug")
    bool bShowMaxDistanceBoundary = true;

    /** Expand the 3D preview to the entire finite Max Distance, instead of being capped by Preview Extent.
     *  For unbounded sources (Max Distance = 0), Preview Extent still controls the visualization.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview || bShowStreamlines"))
    bool bFitPreviewToMaxDistance = true;

    /** Relative resolution of the WHOLE displayed region, not arrows per meter.
     * 1 = sparse (3 samples per axis), 10 = detailed (12 samples per axis).
     * High values create more debug arrows and can impact editor performance.
     * Increasing Max Distance spreads samples over the new size rather than changing their count.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview", ClampMin="1", ClampMax="10", UIMin="1", UIMax="10"))
    int32 PreviewDensity = 4;

    /** Prevent excessive debug draw calls with dense visualizations. Candidate samples are
     * distributed over the full region before thinning, never clustered in a corner.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview", ClampMin="64", ClampMax="2048"))
    int32 PreviewMaxArrows = 2048;

    /** Trace field lines through space. This is a normalized acceleration visualization,
     * not a ballistic / momentum-aware prediction of rigid-body trajectories.
     * Can be enabled independently of instantaneous vector arrows.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Streamlines")
    bool bShowStreamlines = false;

    /** Seed distribution and target line count across the complete displayed field:
     * 1 sparse .. 10 dense. Line count is bounded by StreamlineMaxLines.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Streamlines", meta=(EditCondition="bShowStreamlines", ClampMin="1", ClampMax="10", UIMin="1", UIMax="10"))
    int32 StreamlineDensity = 4;

    /** Limit the number of independent streamlines; independent of arrow count. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Streamlines", meta=(EditCondition="bShowStreamlines", ClampMin="1", ClampMax="96", UIMin="1", UIMax="96"))
    int32 StreamlineMaxLines = 32;

    /** Integration steps for each streamline. Uses midpoint (RK2) spatial integration. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Streamlines", meta=(EditCondition="bShowStreamlines", ClampMin="8", ClampMax="128", UIMin="8", UIMax="128"))
    int32 StreamlineSteps = 64;

    /** Total traced distance as a multiple of the displayed field radius/extent.
     * Higher values show longer spirals; traces stop on field boundaries.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Streamlines", meta=(EditCondition="bShowStreamlines", ClampMin="0.25", ClampMax="4.0", UIMin="0.25", UIMax="4.0"))
    double StreamlineLength = 2.0;

    /** Hard cap on all traced segments per source, regardless of density/steps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Streamlines", meta=(EditCondition="bShowStreamlines", ClampMin="128", ClampMax="8192", UIMin="128", UIMax="8192"))
    int32 StreamlineMaxSegments = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Streamlines", meta=(EditCondition="bShowStreamlines"))
    bool bShowStreamlineDirectionMarkers = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug|Streamlines", meta=(EditCondition="bShowStreamlines", ClampMin="0.5", ClampMax="4.0", UIMin="0.5", UIMax="4.0"))
    float StreamlineThickness = 1.5f;

    /** Preview radius/half-extent when unlimited or when Auto Fit is off. Also controls the visible axial length for Axis sources. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview || bShowStreamlines", ClampMin="50.0", Units="cm"))
    double PreviewExtent = 900.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview", ClampMin="10.0", Units="cm"))
    double PreviewArrowLength = 120.0;

    /** Longer arrows indicate stronger local acceleration when using falloff. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview"))
    bool bScalePreviewByStrength = true;

    virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
    virtual void PostRegisterAllComponents() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override;
#endif

private:
#if WITH_EDITOR
    // Editor-only draw cache; non-persistent, never cooked or replicated.
    TArray<TPair<FVector, FVector>> CachedStreamlineSegments;
    TArray<TPair<FVector, FVector>> CachedStreamlineMarkers;
    FTransform CachedStreamlineTransform = FTransform::Identity;
    double LastStreamlineBuildSeconds = -1.0;
    bool bStreamlinesCached = false;
#endif

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
