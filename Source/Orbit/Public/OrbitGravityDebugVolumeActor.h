#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravityDebugVolumeActor.generated.h"

class USceneComponent;
class UBoxComponent;
class UOrbitGravityDebugDrawComponent;

/** Controls when the Debug Volume evaluates the gravity field, independently of rendering. */
UENUM(BlueprintType)
enum class EOrbitDebugVolumeEvaluationMode : uint8
{
    Automatic UMETA(DisplayName="Automatic (Interval)"),
    Manual UMETA(DisplayName="Manual (Refresh Only)")
};

/** A single field snapshot stored in world coordinates for appearance-only redraws. */
struct FOrbitDebugVolumeCachedSample
{
    FVector Position = FVector::ZeroVector;
    FVector Acceleration = FVector::ZeroVector;
    bool bHasValidDirection = false;
};

/** Non-physical probe volume that visualizes the actual combined Orbit gravity field. */
UCLASS(Blueprintable, meta=(DisplayName="Orbit Debug Volume Actor"))
class ORBIT_API AOrbitGravityDebugVolumeActor : public AActor
{
    GENERATED_BODY()
public:
    AOrbitGravityDebugVolumeActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Debug Volume")
    TObjectPtr<USceneComponent> SceneRoot;

    /** Native editable collision-free box outline; visibility is controlled by Unreal. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Debug Volume")
    TObjectPtr<UBoxComponent> BoundsBox;

    /** Render-only arrows. Disable Visible or Hidden In Game using standard component controls. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Orbit|Debug Volume")
    TObjectPtr<UOrbitGravityDebugDrawComponent> GravityArrows;

    /** Two independently editable local-space corners. Native 3D viewport vector widgets included. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Bounds", meta=(MakeEditWidget, ToolTip="First local-space corner; move with the viewport widget."))
    FVector CornerA = FVector(-500.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Bounds", meta=(MakeEditWidget, ToolTip="Second local-space corner; move with the viewport widget."))
    FVector CornerB = FVector(500.0);

    /** Native Orbit receiver channel mask: sources match when any channel overlaps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Sampling")
    FOrbitGravityChannels GravityChannels;

    /** Physics excludes character-only sources; Character excludes physics-only sources. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Sampling")
    EOrbitGravityChannel SampleMode = EOrbitGravityChannel::General;

    /** Uniform grid samples per meter on each local axis. Higher values produce more arrows. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Sampling", meta=(ClampMin="0.05", ClampMax="10.0", UIMin="0.1", UIMax="3.0"))
    float Density = 0.35f;

    /** Automatic grid budget. The grid remains uniform when density is reduced to respect this limit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Sampling", meta=(ClampMin="1", ClampMax="8192", UIMin="64", UIMax="2048"))
    int32 MaxArrows = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Appearance")
    FLinearColor ArrowColor = FLinearColor(0.05f, 0.8f, 1.0f);

    /** Length multiplier in centimeters at 980 cm/s^2. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Appearance", meta=(ClampMin="0.01", ClampMax="20.0", UIMin="0.1", UIMax="4.0"))
    float ArrowScale = 1.0f;

    /** Line width in screen pixels; applies to arrow shafts, heads, and zero-gravity markers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Appearance", meta=(ClampMin="0.1", ClampMax="20.0", UIMin="0.5", UIMax="8.0"))
    float ArrowThickness = 1.8f;

    /** Arrow lengths communicate magnitude when enabled; strength 980 corresponds to the base length. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Appearance")
    bool bLengthRepresentsMagnitude = true;

    /** Preserve zero-gravity sample points as small crosses, so a missing vector is visible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Appearance")
    bool bShowZeroGravitySamples = false;

    /** Automatic continuously evaluates the field; Manual keeps the last snapshot until Refresh Gravity Preview is invoked. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Sampling")
    EOrbitDebugVolumeEvaluationMode EvaluationMode = EOrbitDebugVolumeEvaluationMode::Automatic;

    /** Interval in both the editor and runtime, including live updates when gravity sources move. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug Volume|Sampling", meta=(ClampMin="0.05", ClampMax="5.0", UIMin="0.1", UIMax="1.0"))
    float RefreshInterval = 0.25f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Debug Volume|Diagnostics")
    int32 LastSampleCount = 0;

    /** Explicitly evaluate the current gravity field. Available as a button in the Actor Details panel, and from Blueprint. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Orbit|Debug Volume", meta=(DisplayName="Refresh Gravity Preview"))
    void RefreshGravityPreview();

    /** Redraw the current snapshot without querying gravity again. Useful after appearance changes. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Orbit|Debug Volume", meta=(DisplayName="Redraw Cached Preview"))
    void RedrawCachedPreview();

    /** Number of field evaluations, excluding redraws. Resets with the actor instance. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Debug Volume|Diagnostics")
    int32 EvaluationCount = 0;

    /** Number of nonzero gravity arrows in the latest captured field. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Debug Volume|Diagnostics")
    int32 LastArrowCount = 0;

    /** Total line segments produced by the most recent redraw (three per arrow). */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Debug Volume|Diagnostics")
    int32 LastLineCount = 0;

    /** Milliseconds spent evaluating the gravity field in the latest snapshot (not redraw time). */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Debug Volume|Diagnostics")
    float LastEvaluationMilliseconds = 0.0f;

    /** Exact deterministic grid counts with a hard maximum; no partially filled slabs. */
    static FIntVector ComputeGridCounts(const FVector& LocalSize, float SamplesPerMeter, int32 Budget);

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
private:
    float TimeUntilRefresh = 0.0f;
    TArray<FOrbitDebugVolumeCachedSample> CachedSamples;
    void UpdateBoundsBox();
};
