#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravitySource.generated.h"

class USceneComponent;
class USplineComponent;
class UArrowComponent;
class UBillboardComponent;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="0.0", Units="cm/s^2"))
    double Strength = 980.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field")
    EOrbitGravityFalloff Falloff = EOrbitGravityFalloff::Constant;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="0.0", Units="cm"))
    double MaxDistance = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="1.0", Units="cm"))
    double InverseSquareReferenceDistance = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combining")
    EOrbitGravityCombine Combine = EOrbitGravityCombine::Override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combining")
    int32 Priority = 0;

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

    UFUNCTION(BlueprintCallable, Category="Orbit|Field")
    FOrbitGravitySample SampleSource(const FVector& WorldLocation) const;

    void BuildDefinition(FOrbitFieldDefinition& OutDefinition) const;

    /** Draws field-vector previews in level viewports and PIE. Editor visualization only; no gameplay effect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug")
    bool bShowGravityPreview = false;

    /** Viewport wireframe of the actual bounded volume, even with gravity arrows hidden. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug")
    bool bShowVolumeBounds = true;

    /** Number of sample positions per axis. 5 = up to 125 arrows before shape masking. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview", ClampMin="2", ClampMax="7"))
    int32 PreviewDensity = 5;

    /** Preview half-extent for unbounded point, axis and directional gravity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview", ClampMin="50.0", Units="cm"))
    double PreviewExtent = 900.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview", ClampMin="10.0", Units="cm"))
    double PreviewArrowLength = 120.0;

    /** Longer arrows indicate stronger local acceleration when using falloff. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug", meta=(EditCondition="bShowGravityPreview"))
    bool bScalePreviewByStrength = true;

    virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
    virtual void PostRegisterAllComponents() override;
#endif
#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override;
#endif

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
