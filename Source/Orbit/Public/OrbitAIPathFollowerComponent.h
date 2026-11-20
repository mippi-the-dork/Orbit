#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitAISurfaceNavigation.h"
#include "OrbitAIPathFollowerComponent.generated.h"

class AActor;
class ACharacter;
struct FOrbitSphericalNavigationSettings;

/**
 * Opt-in, gravity-tangent waypoint steering for AI-controlled Orbit Characters.
 * Works with authored/generated world-space waypoints. Can import native NavMesh
 * paths where a valid native NavMesh exists. Spherical surface paths use Orbit
 * collision-sampled graph navigation, not Recast or native crowd avoidance.
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

    /** Collision-verified spherical graph route. Does not use Recast or handle dynamic crowds. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Surface")
    bool FollowSphericalSurfacePath(FVector Goal, FVector SphereCenter, float ReferenceRadius,
        const FOrbitSphericalNavigationSettings& Settings);

    /** Use the owning Character capsule when planning or checking a spherical path. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface|Agent")
    bool bUseCharacterCapsuleForSurfaceNavigation = true;

    /** Check the upcoming route leg periodically, and recalculate if it is no longer clear. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface|Dynamic")
    bool bReplanBlockedSurfaceRoutes = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface|Dynamic", meta=(ClampMin="0.2", Units="s"))
    float RouteCheckInterval = 0.75f;

    /** Editor / game debug display. Draws current route lines while active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface|Debug")
    bool bDrawSurfacePathDebug = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    int32 SurfaceReplanCount = 0;

    /** Recalculate an active spherical route immediately, for changed terrain or scripted obstacles. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Surface")
    bool ReplanSphericalSurfacePathNow();

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
    bool bHasActiveSurfaceRoute = false;
    FVector SurfaceGoal = FVector::ZeroVector;
    FVector SurfaceCenter = FVector::ZeroVector;
    float SurfaceRadius = 0.0f;
    float TimeUntilRouteCheck = 0.0f;
    FOrbitSphericalNavigationSettings ActiveSurfaceSettings;
};
