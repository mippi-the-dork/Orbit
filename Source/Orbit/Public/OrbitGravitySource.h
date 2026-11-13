#pragma once

#include "CoreMinimal.h"
#include "OrbitSplineSampling.h"
#include "GameFramework/Actor.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravitySource.generated.h"

class USceneComponent;
class USplineComponent;
class UArrowComponent;
class UBillboardComponent;
class UOrbitBoundarySelectionComponent;
struct FPropertyChangedEvent;

/** Author one gravity field. The Spline component is editable directly in the level viewport. */
UCLASS(Blueprintable, HideCategories=(Replication, Input, Collision))
class ORBIT_API AOrbitGravitySource : public AActor
{
    GENERATED_BODY()
public:
    AOrbitGravitySource();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit", meta=(ToolTip="Root scene component of the Orbit gravity source Actor."))
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit", meta=(ToolTip="Native spline component owned by this Actor. Its geometry is used when Shape is Spline."))
    TObjectPtr<USplineComponent> GravitySpline;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit", meta=(ToolTip="Editor visualization of the source orientation."))
    TObjectPtr<UArrowComponent> DirectionArrow;

#if WITH_EDITORONLY_DATA
    /** Viewport selection icon. Uses the supplied OrbitAsset_64 artwork. */
    UPROPERTY(VisibleAnywhere, Category="Orbit", meta=(DisplayName="Orbit Billboard", ToolTip="Editor-only Orbit icon used to select the source in the viewport."))
    TObjectPtr<UBillboardComponent> OrbitBillboard;

    /** Editor-only selectable wireframes for box limits and max-distance boundaries. */
    UPROPERTY(VisibleAnywhere, Transient, Category="Orbit", meta=(ToolTip="Editor-only selectable wireframe for box limits and maximum distance."))
    TObjectPtr<UOrbitBoundarySelectionComponent> BoundarySelection;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Geometry used to define the gravity field. Spline and Volume reveal additional shape-specific settings."))
    EOrbitGravityShape Shape = EOrbitGravityShape::Point;

    /** For Volume, this controls direction only in Directional (Legacy) mode. Other Volume Direction Modes take precedence; the saved value is preserved. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Shape != EOrbitGravityShape::Volume || VolumeDirectionMode == EOrbitVolumeDirectionMode::Directional", ToolTip="Gravity direction for non-Volume fields and Directional (Legacy) volumes. For other Volume Direction Modes, edit the mode under Orbit | Volume; this value is retained but inactive."))
    EOrbitGravityDirection Direction = EOrbitGravityDirection::Inward;

    /** Radial vortex modes only: signed tangent-to-radial acceleration ratio.
     * +1 = clockwise at 45 degrees; -1 = counterclockwise at 45 degrees.
     * Clockwise is viewed from the positive local axis toward its negative direction.
     * Zero gives pure radial inward/outward gravity. Strength still sets the total acceleration.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward || Direction == EOrbitGravityDirection::VortexOutward", ClampMin="-10.0", ClampMax="10.0", UIMin="-3.0", UIMax="3.0", ToolTip="Signed rotation strength relative to radial gravity. Zero removes swirl; positive and negative values rotate in opposite directions.", EditConditionHides))
    double VortexSpin = 1.0;

    /** Determines behavior of physics-simulated bodies in Vortex Inward fields.
     * Spiral Capture controls speed as radius decreases, preventing continuous-force ejection.
     * The instantaneous gravity query and field-line previews remain acceleration-only.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward", ToolTip="How simulated rigid bodies respond to inward vortex gravity. Spiral Capture additionally steers their movement.", EditConditionHides))
    EOrbitVortexPhysicsMode VortexPhysicsMode = EOrbitVortexPhysicsMode::SpiralCapture;

    /** Desired rate of inward convergence (per second) for Spiral Capture. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", ClampMin="0.0", ClampMax="5.0", UIMin="0.0", UIMax="2.0", ToolTip="Target inward convergence per second during Spiral Capture. Only applies to inward vortex physics.", EditConditionHides))
    double VortexConvergenceRate = 0.5;

    /** Desired rotation rate in degrees/second, multiplied by signed Vortex Spin. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", ClampMin="0.0", ClampMax="720.0", UIMin="0.0", UIMax="180.0", Units="deg/s", ToolTip="Target spiral rotation rate in degrees per second during Spiral Capture.", EditConditionHides))
    double VortexAngularRate = 63.0;

    /** Response rate for steering simulated bodies toward the inward spiral. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", ClampMin="0.1", ClampMax="30.0", UIMin="1.0", UIMax="12.0", ToolTip="How quickly simulated bodies steer toward the Spiral Capture flow.", EditConditionHides))
    double VortexResponseRate = 6.0;

    /** Caps the desired speed as a source-independent safety control. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", ClampMin="1.0", Units="cm/s", ToolTip="Upper limit in centimeters per second for Spiral Capture target flow speed.", EditConditionHides))
    double VortexMaxFlowSpeed = 1500.0;

    /** Caps steering acceleration as a multiple of current gravitational acceleration. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Direction == EOrbitGravityDirection::VortexInward && VortexPhysicsMode == EOrbitVortexPhysicsMode::SpiralCapture", ClampMin="0.1", ClampMax="20.0", UIMin="0.5", UIMax="6.0", ToolTip="Maximum capture steering acceleration relative to the source gravity strength.", EditConditionHides))
    double VortexMaxAccelerationFactor = 3.0;

    /** In PIE, mirror direct editor-world Details edits to the corresponding
     *  runtime source when their stable actor identity matches. Editor-only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="When enabled, editing this Actor in the editor world during PIE also updates its matching PIE instance. Does not alter saved levels."))
    bool bMirrorEditorChangesToPIE = true;

    /** Last number of matching PIE actor instances updated by an editor-world
     *  Details property edit. Visible only for debugging the editing workflow. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit", meta=(ToolTip="Diagnostic count of PIE source Actors updated by the last editor-world property edit."))
    int32 LastEditorPIEMirrorCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="0.0", Units="cm/s^2", ToolTip="Acceleration applied by this source in centimeters per second squared. Standard Earth gravity is approximately 980."))
    double Strength = 980.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="How gravity strength changes with distance from the source. Constant applies the same strength throughout the field."))
    EOrbitGravityFalloff Falloff = EOrbitGravityFalloff::Constant;

    /** For Volume, caps each dimension of the selected local shape (box, sphere,
     * cylinder or capsule). Other shapes retain their existing radial metric.
     * Zero means no additional cap beyond a Volume's authored dimensions.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="0.0", Units="cm", ToolTip="Optional field reach in centimeters. Zero means no additional distance limit; Volume fields are also bounded by their authored geometry."))
    double MaxDistance = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="1.0", Units="cm", ToolTip="Distance in centimeters used to normalize inverse-square falloff. Only affects Inverse Square falloff.", EditCondition="Falloff == EOrbitGravityFalloff::InverseSquare", EditConditionHides))
    double InverseSquareReferenceDistance = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="How this field interacts with other overlapping sources: replace the current field or contribute according to the selected combining mode."))
    EOrbitGravityCombine Combine = EOrbitGravityCombine::Override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Precedence of this source when several fields overlap. Used by the field combination and competition rules."))
    int32 Priority = 0;

    /** Lighting-style mask. Sources affect receivers when any selected channel matches. Default: channel 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="A source affects receivers when their channel masks overlap. Default is channel 0."))
    FOrbitGravityChannels GravityChannels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Enables this gravity source. Disabled sources do not influence characters or physics bodies."))
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Allow this field to affect eligible physics-simulated bodies."))
    bool bAffectPhysics = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Allow this field to affect characters using Orbit character gravity."))
    bool bAffectCharacters = true;

    /** Additional rotated box mask, combined with any Volume boundary if present. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Restrict this field to an additional rotated box region. Separate from the shape of a Volume field."))
    bool bLimitToBox = false;

    /** Also configures the Box volume dimensions even when the additional box mask is disabled. */
    /** Retained as serialized migration data; existing level and Blueprint values are never discarded. */
    UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Migrated to Box Corner A/B"))
    FVector BoxHalfExtents = FVector(1000.0);

    /** Editable local-space opposite corners. Translation controls bounds; rotation and scale are not used. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(MakeEditWidget, EditCondition="bLimitToBox || (Shape == EOrbitGravityShape::Volume && VolumeShape == EOrbitVolumeShape::Box)", ToolTip="First corner of the local-space box. Only the translation is used; rotation and scale are ignored."))
    FTransform BoxCornerA = FTransform(FVector(-1000.0));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(MakeEditWidget, EditCondition="bLimitToBox || (Shape == EOrbitGravityShape::Volume && VolumeShape == EOrbitVolumeShape::Box)", ToolTip="Opposite corner of the local-space box. Only the translation is used; rotation and scale are ignored."))
    FTransform BoxCornerB = FTransform(FVector(1000.0));
    /** Saved migration marker; old actors convert on load, including instance-specific half extents. */
    UPROPERTY()
    bool bBoxCornersInitialized = false;
    void EnsureBoxCornersMigrated();

    /** Physical influence boundary for a Volume gravity source. Box preserves the existing BoxHalfExtents behavior. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Shape == EOrbitGravityShape::Volume", ToolTip="Boundary geometry for a Volume field. Does not change non-Volume shapes.", EditConditionHides))
    EOrbitVolumeShape VolumeShape = EOrbitVolumeShape::Box;

    /** Additional volume acceleration directions; Directional retains the original behavior. */
    /** Governs gravity direction for Volumes. In nonlegacy modes, the Field Direction setting is inactive and retains its existing value. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Shape == EOrbitGravityShape::Volume", ToolTip="How a Volume determines gravity direction. Directional (Legacy) uses the regular Gravity Direction setting.", EditConditionHides))
    EOrbitVolumeDirectionMode VolumeDirectionMode = EOrbitVolumeDirectionMode::Directional;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Shape == EOrbitGravityShape::Volume && VolumeShape != EOrbitVolumeShape::Box", ClampMin="1.0", Units="cm", ToolTip="Sphere, cylinder, or capsule radius in centimeters. Only applies to a Volume of a compatible shape.", EditConditionHides))
    double VolumeRadius = 1000.0;

    /** Cylinder half-height, or capsule total half-height including the caps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="Shape == EOrbitGravityShape::Volume && (VolumeShape == EOrbitVolumeShape::Cylinder || VolumeShape == EOrbitVolumeShape::Capsule)", ClampMin="1.0", Units="cm", ToolTip="Cylinder half-height or capsule half-height including caps, in centimeters. Only applies to those Volume shapes.", EditConditionHides))
    double VolumeHalfHeight = 1000.0;

    /** World-space spline sampling interval. Lower values improve bend fidelity at a cost. */
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

    /** Set strength on THIS world instance and publish it to gravity consumers immediately.
     *  Prefer this to an unqualified editor actor property change during PIE debugging.
     *  Does not replicate and does not change the saved editor-world actor. */
    UFUNCTION(BlueprintCallable, Category="Orbit")
    void SetGravityStrength(double NewStrength);

    UFUNCTION(BlueprintCallable, Category="Orbit")
    FOrbitGravitySample SampleSource(const FVector& WorldLocation) const;

    void BuildDefinition(FOrbitFieldDefinition& OutDefinition, bool bReuseSplineArrays = false) const;

    /** Permit source debug geometry during Play In Editor and Simulate In Editor.
     *  Editor authoring previews remain available independently of this option.
     *  Visualization only: never changes gravity evaluation or receiver behavior.
     *  Defaults off for newly created sources and for older saved sources that
     *  do not contain this property. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(DisplayName="Show In Game", ToolTip="Also draw this source's enabled debug arrows, streamlines, Volume bounds, and Max Distance boundary while playing or simulating in the editor. Off by default to avoid gameplay debug rendering cost. Editor authoring previews still appear when their individual toggles are enabled. Does not affect gravity."))
    bool bShowDebugInGame = false;

    /** Draws field-vector previews in level viewports and PIE. Editor visualization only; no gameplay effect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Draw gravity direction arrows in the level viewport. Editor visualization only; high densities cost rendering time."))
    bool bShowGravityPreview = false;

    /** Viewport wireframe of the actual bounded volume, even with gravity arrows hidden. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Draw the authored Volume boundary in the viewport, even when gravity arrows are hidden."))
    bool bShowVolumeBounds = true;

    /** Visualizes the finite Max Distance boundary for Point, Axis, Spline and other sources.
     *  Axis boundaries are infinite along the axis; the displayed tube length is a visual slice.
     *  For Volume sources the blue boundary has the chosen Volume shape,
     *  with its effective dimensions capped by Max Distance.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Draw the effective Max Distance boundary in the viewport. This is not another gameplay field limit."))
    bool bShowMaxDistanceBoundary = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Color of the additional box-limit wireframe. Visualization only."))
    FLinearColor LimitBoxColor = FLinearColor(1.0f, 0.82f, 0.10f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="0.1", UIMin="0.5", UIMax="10.0", ToolTip="Line thickness of the box-limit preview in the editor viewport."))
    float LimitBoxThickness = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Color of the Max Distance boundary preview. Does not change gravity."))
    FLinearColor MaxDistanceColor = FLinearColor(0.27f, 0.7f, 0.96f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ClampMin="0.1", UIMin="0.5", UIMax="10.0", ToolTip="Line thickness of the Max Distance boundary preview."))
    float MaxDistanceThickness = 1.3f;

    /** Color of instantaneous field-vector arrows. Visualization only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowGravityPreview", ToolTip="Color of sampled gravity direction arrows. Visualization only."))
    FLinearColor PreviewArrowColor = FLinearColor::FromSRGBColor(FColor::Cyan);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowGravityPreview", ClampMin="0.1", UIMin="0.5", UIMax="10.0", ToolTip="Line thickness of preview arrows. Visualization only."))
    float PreviewArrowThickness = 1.2f;

    /** Color for field-line segments; does not affect gravity or receiver channels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowStreamlines", ToolTip="Color of debug streamlines. Visualization only."))
    FLinearColor StreamlineColor = FLinearColor::FromSRGBColor(FColor(194, 116, 250));

    /** Expand the 3D preview to the entire finite Max Distance, instead of being capped by Preview Extent.
     *  For unbounded sources (Max Distance = 0), Preview Extent still controls the visualization.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowGravityPreview || bShowStreamlines", ToolTip="Expand the preview to a finite Max Distance when available; otherwise use Preview Extent."))
    bool bFitPreviewToMaxDistance = true;

    /** Relative resolution of the WHOLE displayed region, not arrows per meter.
     * 1 = sparse (3 samples per axis), 10 = detailed (12 samples per axis).
     * High values create more debug arrows and can impact editor performance.
     * Increasing Max Distance spreads samples over the new size rather than changing their count.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowGravityPreview", ClampMin="1", ClampMax="10", UIMin="1", UIMax="10", ToolTip="Sample density for gravity arrows over the whole preview region, not per meter. Higher values cost more."))
    int32 PreviewDensity = 4;

    /** Prevent excessive debug draw calls with dense visualizations. Candidate samples are
     * distributed over the full region before thinning, never clustered in a corner.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowGravityPreview", ClampMin="64", ClampMax="2048", ToolTip="Hard cap on the number of gravity arrows drawn for this source."))
    int32 PreviewMaxArrows = 2048;

    /** Trace field lines through space. This is a normalized acceleration visualization,
     * not a ballistic / momentum-aware prediction of rigid-body trajectories.
     * Can be enabled independently of instantaneous vector arrows.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(ToolTip="Trace approximate gravity field lines for visualization. These are not predicted paths with inertia."))
    bool bShowStreamlines = false;

    /** Seed distribution and target line count across the complete displayed field:
     * 1 sparse .. 10 dense. Line count is bounded by StreamlineMaxLines.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowStreamlines", ClampMin="1", ClampMax="10", UIMin="1", UIMax="10", ToolTip="Amount of streamline seeds across the displayed field."))
    int32 StreamlineDensity = 4;

    /** Limit the number of independent streamlines; independent of arrow count. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowStreamlines", ClampMin="1", ClampMax="96", UIMin="1", UIMax="96", ToolTip="Maximum number of independent debug streamlines."))
    int32 StreamlineMaxLines = 32;

    /** Integration steps for each streamline. Uses midpoint (RK2) spatial integration. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowStreamlines", ClampMin="8", ClampMax="128", UIMin="8", UIMax="128", ToolTip="Integration steps per debug streamline. Higher values cost more editor time."))
    int32 StreamlineSteps = 64;

    /** Total traced distance as a multiple of the displayed field radius/extent.
     * Higher values show longer spirals; traces stop on field boundaries.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowStreamlines", ClampMin="0.25", ClampMax="4.0", UIMin="0.25", UIMax="4.0", ToolTip="Length of traced streamline relative to the preview region. Does not limit actual gravity."))
    double StreamlineLength = 2.0;

    /** Hard cap on all traced segments per source, regardless of density/steps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowStreamlines", ClampMin="128", ClampMax="8192", UIMin="128", UIMax="8192", ToolTip="Hard cap on debug streamline segments across this source."))
    int32 StreamlineMaxSegments = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowStreamlines", ToolTip="Draw small direction markers along debug streamlines."))
    bool bShowStreamlineDirectionMarkers = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowStreamlines", ClampMin="0.5", ClampMax="4.0", UIMin="0.5", UIMax="4.0", ToolTip="Line thickness of debug streamlines."))
    float StreamlineThickness = 1.5f;

    /** Preview radius/half-extent when unlimited or when Auto Fit is off. Also controls the visible axial length for Axis sources. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowGravityPreview || bShowStreamlines", ClampMin="50.0", Units="cm", ToolTip="Preview size when no finite Max Distance is used or automatic fit is disabled. Visualization only."))
    double PreviewExtent = 900.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowGravityPreview", ClampMin="10.0", Units="cm", ToolTip="Nominal length of debug arrows, in centimeters. Visualization only."))
    double PreviewArrowLength = 120.0;

    /** Longer arrows indicate stronger local acceleration when using falloff. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit", meta=(EditCondition="bShowGravityPreview", ToolTip="Scale preview arrow lengths to indicate local acceleration strength. Visualization only."))
    bool bScalePreviewByStrength = true;

    virtual void PostLoad() override;
    virtual void PostActorCreated() override;
    virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
    virtual void PostRegisterAllComponents() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override;
    void UpdateSelectableBounds();
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
