#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitAIPathFollowerComponent.generated.h"

class AActor;
class ACharacter;

/**
 * Opt-in, gravity-tangent waypoint steering for AI-controlled Orbit Characters.
 * Works with authored/generated world-space waypoints. Can import native NavMesh
 * paths where a valid native NavMesh exists, but DOES NOT generate curved surface
 * navigation or replace Recast/PathFollowingComponent obstacle avoidance.
 * Do not run this and AIController::MoveTo concurrently on the same pawn.
 */
UCLASS(ClassGroup=(Orbit), BlueprintType, meta=(BlueprintSpawnableComponent))
class ORBIT_API UOrbitAIPathFollowerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOrbitAIPathFollowerComponent();

    /** Target acceptance in world units, including vertical offset. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Path", meta=(ClampMin="1.0", Units="cm"))
    float AcceptanceRadius = 75.0f;

    /** Maximum distance from the surface before waypoint arrival is rejected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Path", meta=(ClampMin="0.0", Units="cm"))
    float MaxNormalArrivalError = 150.0f;

    /** When true, the next waypoint can be accepted by its tangent distance if its normal separation is within MaxNormalArrivalError. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Path")
    bool bUseTangentArrival = true;

    /** Ignore motion input while native CharacterMovement is not in walking/nav-walking mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Path")
    bool bRequireGrounded = true;

    /** When enabled, a supplied target Actor replaces the last waypoint location each tick. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Path")
    bool bTrackMovingTarget = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    bool bFollowingPath = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    int32 CurrentWaypointIndex = INDEX_NONE;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    TArray<FVector> Waypoints;

    /** Empty until a destination is reached or movement is cancelled. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    FString LastPathStatus = TEXT("Idle");

    /** Supply ordered world-space waypoints. Does not perform pathfinding or collision tests. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Path")
    bool FollowWaypoints(const TArray<FVector>& NewWaypoints);

    /** Follow a straight-line goal in the local gravity tangent plane; no obstacle avoidance. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Path")
    bool FollowLocation(FVector Goal);

    /** Generate a surface-following great-circle route around a spherical gravity center.
     * No obstacle avoidance; use authored routes when traversability is not guaranteed. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Path")
    bool FollowSphericalArc(FVector Goal, FVector SphereCenter, float MaxSegmentLength = 150.0f);

    /** Track a moving destination; does not perform pathfinding. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Path")
    bool FollowActor(AActor* Goal);

    /** Import a native Recast path, if available, and steer along its path points.
     * Native Recast does not build a spherical world NavMesh. Returns false when
     * no valid, non-partial native path is produced. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Path")
    bool FollowNativeNavigationPath(FVector Goal);

    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Path")
    void StopFollowing();

    UFUNCTION(BlueprintPure, Category="Orbit|AI|Path")
    FVector GetCurrentWaypoint() const;

    /** Projects movement direction into a plane orthogonal to the actual local gravity. */
    static FVector CalculateTangentSteering(const FVector& CurrentLocation, const FVector& TargetLocation, const FVector& GravityDown);

    /** Generate uniformly spaced great-circle waypoints, limited to 256 points.
     * Fails on degenerate input and very different start/end radii. */
    static bool BuildSphericalArcWaypoints(const FVector& Start, const FVector& Goal, const FVector& Center,
        float MaxSegmentLength, TArray<FVector>& OutPoints);

    /** Tangent acceptance is only valid while the normal error remains bounded. */
    static bool IsWaypointReached(const FVector& CurrentLocation, const FVector& TargetLocation, const FVector& GravityDown,
        float Radius, float MaxNormalError, bool bTangentArrival);

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<AActor> MovingTarget;
};
