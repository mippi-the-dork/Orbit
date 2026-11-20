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
