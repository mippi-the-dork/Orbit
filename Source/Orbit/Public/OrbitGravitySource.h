#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravitySource.generated.h"

class USceneComponent;
class USplineComponent;
class UArrowComponent;

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

    /** Apply a rotated box mask around the source (in addition to MaxDistance). Always used by Volume. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence")
    bool bLimitToBox = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Influence", meta=(EditCondition="bLimitToBox", ClampMin="1.0", Units="cm"))
    FVector BoxHalfExtents = FVector(1000.0);

    /** World-space spline sampling interval. Lower values improve bend fidelity at a cost. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Spline", meta=(ClampMin="25.0", Units="cm"))
    double SplineSampleSpacing = 200.0;

    UFUNCTION(BlueprintCallable, Category="Orbit|Field")
    FOrbitGravitySample SampleSource(const FVector& WorldLocation) const;

    void BuildDefinition(FOrbitFieldDefinition& OutDefinition) const;

    /** Draws field-vector previews in level viewports and PIE. Editor visualization only; no gameplay effect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Debug")
    bool bShowGravityPreview = false;

    virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override;
#endif

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
