#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravitySourceComponent.generated.h"

class USplineComponent;

/** Embeds a complete Orbit gravity field inside any Actor. The field follows this
 * component's WORLD transform, independently of its owner Actor's pivot. */
UCLASS(ClassGroup=(Orbit), BlueprintType, meta=(BlueprintSpawnableComponent))
class ORBIT_API UOrbitGravitySourceComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UOrbitGravitySourceComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field")
    bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field")
    EOrbitGravityShape Shape = EOrbitGravityShape::Point;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field")
    EOrbitGravityDirection Direction = EOrbitGravityDirection::Inward;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="0.0", Units="cm/s^2"))
    double Strength = 980.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field")
    EOrbitGravityFalloff Falloff = EOrbitGravityFalloff::Constant;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="0.0", Units="cm"))
    /** For Volume, caps selected shape dimensions, not a spherical intersection. */
    double MaxDistance = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Field", meta=(ClampMin="1.0", Units="cm"))
    double InverseSquareReferenceDistance = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combining")
    EOrbitGravityCombine Combine = EOrbitGravityCombine::Override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Combining")
    int32 Priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Channels")
    FOrbitGravityChannels GravityChannels;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    bool bAffectPhysics = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    bool bAffectCharacters = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    bool bLimitToBox = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    FVector BoxHalfExtents = FVector(1000.0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Volume")
    EOrbitVolumeShape VolumeShape = EOrbitVolumeShape::Box;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Volume", meta=(ClampMin="1.0", Units="cm"))
    double VolumeRadius = 1000.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Volume", meta=(ClampMin="1.0", Units="cm"))
    double VolumeHalfHeight = 1000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex", meta=(ClampMin="-10.0", ClampMax="10.0"))
    double VortexSpin = 1.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex")
    EOrbitVortexPhysicsMode VortexPhysicsMode = EOrbitVortexPhysicsMode::SpiralCapture;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture")
    double VortexConvergenceRate = 0.5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture")
    double VortexAngularRate = 63.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture")
    double VortexResponseRate = 6.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture")
    double VortexMaxFlowSpeed = 1500.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Vortex|Spiral Capture")
    double VortexMaxAccelerationFactor = 3.0;

    /** Optional spline on the same actor. Leave empty to use the first spline
     * found on the actor. Prefer an explicit reference with several splines. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Spline")
    TObjectPtr<USplineComponent> GravitySpline = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Spline", meta=(ClampMin="25.0", Units="cm"))
    double SplineSampleSpacing = 200.0;

    /** Rebuild a UObject-free field snapshot using this component's transform. */
    void BuildDefinition(FOrbitFieldDefinition& OutDefinition) const;

    UFUNCTION(BlueprintCallable, Category="Orbit|Field|Runtime")
    void SetGravityStrength(double NewStrength);

    /** Single-component evaluation. Honors this source's channels and integration flags. */
    UFUNCTION(BlueprintPure, Category="Orbit|Field")
    FOrbitGravitySample SampleSourceComponent(FVector WorldLocation) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
