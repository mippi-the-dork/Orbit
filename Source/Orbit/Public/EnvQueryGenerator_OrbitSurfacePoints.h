#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "OrbitAISurfaceNavigation.h"
#include "EnvQueryGenerator_OrbitSurfacePoints.generated.h"

/** Native EQS point generator for a single collision-queryable spherical planet.
 * Candidate directions are evenly distributed by deterministic Fibonacci sampling.
 * Collision projection is evaluated afresh per query and never cached across agents.
 */
UCLASS(meta=(DisplayName="Orbit: Spherical Surface Points"))
class ORBIT_API UEnvQueryGenerator_OrbitSurfacePoints : public UEnvQueryGenerator
{
    GENERATED_BODY()
public:
    UEnvQueryGenerator_OrbitSurfacePoints(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UPROPERTY(EditDefaultsOnly, Category="Orbit|Surface")
    FVector SphereCenter = FVector::ZeroVector;

    /** Optional native EQS context for moving planets or shared authored EQS templates.
     * Uses the first location returned by the context; fails closed if no location exists.
     * Defaults off so existing SphereCenter settings continue to work.
     */
    UPROPERTY(EditDefaultsOnly, Category="Orbit|Surface|Context")
    bool bUseSphereCenterContext = false;

    UPROPERTY(EditDefaultsOnly, Category="Orbit|Surface|Context",
        meta=(EditCondition="bUseSphereCenterContext"))
    TSubclassOf<class UEnvQueryContext> SphereCenterContext;

    UPROPERTY(EditDefaultsOnly, Category="Orbit|Surface", meta=(ClampMin="1.0", Units="cm"))
    float SurfaceRadius = 1000.0f;

    /** Maximum 256 collision samples per EQS query. */
    UPROPERTY(EditDefaultsOnly, Category="Orbit|Surface", meta=(ClampMin="8", ClampMax="256"))
    int32 SampleCount = 32;

    /** When true, only actual collision-queryable, walkable points are emitted. */
    UPROPERTY(EditDefaultsOnly, Category="Orbit|Surface")
    bool bProjectToWalkableSurface = true;

    UPROPERTY(EditDefaultsOnly, Category="Orbit|Surface")
    FOrbitSphericalNavigationSettings NavigationSettings;

    /** Restrict candidates to a spherical cap around the querier instead of searching the entire planet.
     * The default remains full-sphere for backwards compatibility. The cap follows the querier as it moves.
     */
    UPROPERTY(EditDefaultsOnly, Category="Orbit|Search Region")
    bool bLimitToQuerierRegion = false;

    /** Angular search radius from the querier in degrees; 180 covers the whole sphere. */
    UPROPERTY(EditDefaultsOnly, Category="Orbit|Search Region",
        meta=(EditCondition="bLimitToQuerierRegion", ClampMin="1.0", ClampMax="180.0", Units="deg"))
    float QuerierRegionAngleDegrees = 45.0f;

    /** Deterministic equal-area spherical-cap sampling around an arbitrary unit direction. */
    static FVector SampleDirectionInCap(int32 Index, int32 Count, const FVector& CapAxis,
        float CapAngleDegrees);

    /** Pure, deterministic sphere sampling. Exposed for native automation; unit vector or zero on invalid input. */
    static FVector SampleDirection(int32 Index, int32 Count);

    virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
    virtual FText GetDescriptionTitle() const override;
    virtual FText GetDescriptionDetails() const override;
};
