#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_OrbitSphericalDistance.generated.h"

/** Native EQS float test: great-circle distance from item to a standard EQS context.
 * Drop into ordinary EQS assets alongside native generators/tests.
 * World-space positions are projected radially onto the configured sphere for scoring.
 * Does not pretend to compute obstacle-aware travel cost.
 */
UCLASS(meta=(DisplayName="Orbit: Spherical Distance"))
class ORBIT_API UEnvQueryTest_OrbitSphericalDistance : public UEnvQueryTest
{
    GENERATED_BODY()
public:
    UEnvQueryTest_OrbitSphericalDistance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UPROPERTY(EditDefaultsOnly, Category="Orbit")
    FVector SphereCenter = FVector::ZeroVector;

    /** Resolve the planet center from a normal EQS context, allowing moving planets and reusable assets.
     * Off by default: existing SphereCenter values remain compatible.
     */
    UPROPERTY(EditDefaultsOnly, Category="Orbit|Context")
    bool bUseSphereCenterContext = false;

    UPROPERTY(EditDefaultsOnly, Category="Orbit|Context", meta=(EditCondition="bUseSphereCenterContext"))
    TSubclassOf<class UEnvQueryContext> SphereCenterContext;

    UPROPERTY(EditDefaultsOnly, Category="Orbit", meta=(ClampMin="1.0", Units="cm"))
    float SurfaceRadius = 1000.0f;

    /** Standard EQS context (Querier by default). */
    UPROPERTY(EditDefaultsOnly, Category="Orbit")
    TSubclassOf<class UEnvQueryContext> DistanceTo;

    static float CalculateDistance(const FVector& A, const FVector& B,
        const FVector& Center, float Radius);

    virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
    virtual FText GetDescriptionTitle() const override;
    virtual FText GetDescriptionDetails() const override;
};
