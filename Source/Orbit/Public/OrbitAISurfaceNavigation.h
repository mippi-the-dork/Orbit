#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "OrbitAISurfaceNavigation.generated.h"

/** Bounded, spherical-surface navigation settings. Distances are Unreal centimeters. */
USTRUCT(BlueprintType)
struct ORBIT_API FOrbitSphericalNavigationSettings
{
    GENERATED_BODY()

    /** Longitude sample count; 8..64. A higher value finds narrower detours but costs more traces. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface", meta=(ClampMin="8", ClampMax="64"))
    int32 LongitudeSamples = 32;

    /** Latitude sample count, including polar rings; 5..33. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface", meta=(ClampMin="5", ClampMax="33"))
    int32 LatitudeSamples = 17;

    /** Outward trace distance from the expected spherical surface. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface", meta=(ClampMin="1.0", Units="cm"))
    float TraceOutward = 150.0f;

    /** Inward trace distance from the expected spherical surface. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface", meta=(ClampMin="1.0", Units="cm"))
    float TraceInward = 200.0f;

    /** Maximum permitted surface height difference from the supplied reference radius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface", meta=(ClampMin="1.0", Units="cm"))
    float MaxSurfaceHeightError = 80.0f;

    /** Minimum dot product of surface normal and radial up. 1 means perfectly radial. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface", meta=(ClampMin="0.0", ClampMax="1.0"))
    float MinimumWalkableNormalDot = 0.65f;

    /** Collision channel for surface/obstacle queries. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_WorldStatic;

    /** Require intermediate samples on each graph edge to be traversable (slower, safer). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface")
    bool bValidateEdges = true;

    /** Horizontal collision radius of the navigating pawn. Zero retains point-only legacy queries. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface|Agent", meta=(ClampMin="0.0", Units="cm"))
    float AgentRadius = 0.0f;

    /** Half-height of the navigating pawn capsule. Requires AgentRadius > 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface|Agent", meta=(ClampMin="0.0", Units="cm"))
    float AgentHalfHeight = 0.0f;

    /** Clearance above the walkable surface to avoid counting its own contact as an obstruction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|AI|Surface|Agent", meta=(ClampMin="0.0", Units="cm"))
    float SurfaceSkin = 3.0f;
};

/** Per-request measurements for native profiling and automation. No persistent shared collision state. */
struct ORBIT_API FOrbitSurfaceNavigationProfile
{
    double EndpointMilliseconds = 0.0;
    double GraphSamplingMilliseconds = 0.0;
    double EdgeValidationMilliseconds = 0.0;
    double SearchMilliseconds = 0.0;
    double ConnectorMilliseconds = 0.0;
    /** Number of actual (successful or rejected) grid surface samples made this request. */
    int32 AttemptedNodeSamples = 0;
    /** Repeated node requests answered from the per-request sampling state. */
    int32 ReusedNodeSamples = 0;
    int32 SampledNodes = 0;
    int32 ExpandedNodes = 0;
    int32 EdgeChecks = 0;
    int32 ReusedEdgeChecks = 0;
    int32 BlockedEdges = 0;
};

/** Spherical surface path generation independent of world-Z Recast navigation. */
UCLASS()
class ORBIT_API UOrbitAISurfaceNavigation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Produces collision-verified world-space waypoints on a spherical surface.
     * Both endpoints must lie near a queryable surface. No result is produced on failure.
     * This is a static spherical graph, not dynamic crowd avoidance or arbitrary topology navigation.
     */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Surface", meta=(WorldContext="WorldContextObject"))
    static bool FindSphericalSurfacePath(const UObject* WorldContextObject, FVector Start, FVector Goal,
        FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings,
        TArray<FVector>& OutWaypoints, FString& OutFailureReason);

    /** Same route and collision semantics as the Blueprint API, with optional per-call diagnostics.
     * Diagnostics are native-only and never retain world or collision data across requests.
     */
    static bool FindSphericalSurfacePathProfiled(const UObject* WorldContextObject, FVector Start, FVector Goal,
        FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings,
        TArray<FVector>& OutWaypoints, FString& OutFailureReason, FOrbitSurfaceNavigationProfile* OutProfile);

    /** Project a candidate position onto the current collision-verified spherical surface.
     * Useful for Blueprint EQS generators/tests. Does not modify Unreal's EQS framework.
     */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Query", meta=(WorldContext="WorldContextObject"))
    static bool ProjectToSphericalSurface(const UObject* WorldContextObject, FVector Candidate,
        FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings,
        FVector& OutSurfacePoint);

    /** Shortest great-circle distance on a reference sphere; not obstacle path distance.
     * Returns -1 for invalid geometry. Suitable as a gravity-relative scoring input.
     */
    UFUNCTION(BlueprintPure, Category="Orbit|AI|Query")
    static double SphericalArcDistance(FVector A, FVector B, FVector SphereCenter, float ReferenceRadius);

    /** Deterministic A* on a seam-wrapped latitude/longitude grid, for isolated automation and tooling.
     * BlockedMask and nodes use indices Lat * LongitudeSamples + Lon. Returned path includes both endpoints.
     */
    /** Verify an existing route leg against the current world collision, including pawn capsule clearance. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Surface", meta=(WorldContext="WorldContextObject"))
    static bool IsSphericalSurfaceLegTraversable(const UObject* WorldContextObject, FVector From, FVector To,
        FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings);

    static bool FindGridRoute(int32 LongitudeSamples, int32 LatitudeSamples, const TArray<uint8>& BlockedMask,
        int32 StartIndex, int32 GoalIndex, TArray<int32>& OutIndices);
};
