#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "OrbitAIPathFollowerComponent.h"
#include "OrbitAIController.generated.h"

class UOrbitAIPathFollowerComponent;
struct FOrbitAIMoveDiagnostics;

/** Optional native AIController adapter for gravity-relative spherical movement.
 * Orbit moves deliberately do not call native MoveTo or rely on a world-Z NavMesh.
 * Give possessed Characters an Orbit AI Path Follower component.
 */
UCLASS(Blueprintable)
class ORBIT_API AOrbitAIController : public AAIController
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Controller")
    bool MoveOrbitToLocation(FVector Destination, FVector SphereCenter, float SurfaceRadius,
        const FOrbitSphericalNavigationSettings& Settings);

    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Controller")
    bool MoveOrbitToActor(AActor* Target, FVector SphereCenter, float SurfaceRadius,
        const FOrbitSphericalNavigationSettings& Settings);

    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Controller")
    void StopOrbitMovement();

    UFUNCTION(BlueprintPure, Category="Orbit|AI|Controller")
    UOrbitAIPathFollowerComponent* GetOrbitPathFollower() const;

    /** Snapshot for debugging the controlled Character's current Orbit move. */
    UFUNCTION(BlueprintPure, Category="Orbit|AI|Controller")
    FOrbitAIMoveDiagnostics GetOrbitMoveDiagnostics() const;

protected:
    virtual void OnUnPossess() override;
};
