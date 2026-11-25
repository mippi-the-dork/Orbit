#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitAISurfaceNavigation.h"
#include "OrbitAIPathFollowerComponent.generated.h"

class AActor;
class ACharacter;

/** Terminal result of the last Orbit AI move. Independent of native Recast path following. */
UENUM(BlueprintType)
enum class EOrbitAIMoveResult : uint8
{
    None,
    InProgress,
    Succeeded,
    Failed,
    Aborted
};
struct FOrbitSphericalNavigationSettings;

/** Single snapshot of an Orbit move, suitable for PIE diagnostics and Blueprint debugging.
 * No pointers or mutable arrays: callers can safely retain the result between frames.
 */
USTRUCT(BlueprintType)
struct ORBIT_API FOrbitAIMoveDiagnostics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") int32 RequestId = 0;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") EOrbitAIMoveResult Result = EOrbitAIMoveResult::None;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") bool bActive = false;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") bool bSphericalRoute = false;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") bool bGrounded = false;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") bool bInsideOrbitGravityField = false;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") int32 GravitySourceId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") int32 WaypointIndex = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") int32 WaypointCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") int32 SuccessfulReplans = 0;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") int32 StallReplans = 0;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") float MovementSpeed = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") float SecondsStalled = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") float TangentWaypointDistance = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Orbit|AI|Diagnostics") FString Status;
};

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

    /** Monotonically advancing identity for the active or most recently completed move.
     * Task cancellation must check this to avoid aborting a replacement request. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    int32 MoveRequestId = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    EOrbitAIMoveResult LastMoveResult = EOrbitAIMoveResult::None;

    /** Read one coherent status snapshot; does not replan, tick or alter the active request. */
    UFUNCTION(BlueprintPure, Category="Orbit|AI|Diagnostics")
    FOrbitAIMoveDiagnostics GetMoveDiagnostics() const;

    /** Stop this request only if it still owns the follower. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Path")
    bool AbortMoveRequest(int32 ExpectedRequestId);

    /** Spherical obstacle-aware Actor chase; replans when the target moves significantly. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Surface")
    bool FollowSphericalSurfaceActor(AActor* Goal, FVector SphereCenter, float ReferenceRadius,
        const FOrbitSphericalNavigationSettings& Settings);

    /** Distance the target must move from the last planned goal before recalculating. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface|Dynamic", meta=(ClampMin="1.0", Units="cm"))
    float MovingTargetReplanDistance = 175.0f;

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

    /** Detect lack of actual Character displacement while movement input is being requested. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Movement|Recovery")
    bool bDetectMovementStalls = true;

    /** Seconds of insufficient physical displacement before a stalled route is reported. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Movement|Recovery", meta=(ClampMin="0.2", Units="s"))
    float MovementStallTimeout = 3.0f;

    /** Minimum actual movement speed used by the stall watchdog (cm/s). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Movement|Recovery", meta=(ClampMin="0.0", Units="cm/s"))
    float MinimumMovementSpeed = 3.0f;

    /** On a stalled spherical route, attempt at most MaxStallReplans fresh obstacle-checked paths. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Movement|Recovery")
    bool bReplanStalledSurfaceRoutes = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Movement|Recovery", meta=(ClampMin="0", ClampMax="10"))
    int32 MaxStallReplans = 2;

    /** Last sampled actual world displacement (cm), independent of path-planning success. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    float LastMovementSpeed = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    float SecondsWithoutMovement = 0.0f;

    /** Distance to the current waypoint measured along the local gravity tangent plane. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    float TangentDistanceToWaypoint = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    int32 StallReplanAttempts = 0;

    /** True if CharacterMovement reports a walking or navwalking ground mode. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|AI|Status")
    bool bLastMovementGrounded = false;

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

    // Public native component tick override. The traversal automation fixture explicitly
    // advances this component and CharacterMovement in a deterministic order.
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<AActor> MovingTarget;
    bool bSurfaceActorChase = false;
    bool bHasActiveSurfaceRoute = false;
    FVector SurfaceGoal = FVector::ZeroVector;
    FVector SurfaceCenter = FVector::ZeroVector;
    float SurfaceRadius = 0.0f;
    float TimeUntilRouteCheck = 0.0f;
    FOrbitSphericalNavigationSettings ActiveSurfaceSettings;
    bool bHasMovementSample = false;
    FVector LastMovementSampleLocation = FVector::ZeroVector;
};
