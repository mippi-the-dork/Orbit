#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OrbitAISurfaceNavigation.h"
#include "OrbitAIPathFollowerComponent.h"
#include "OrbitAIBlueprintLibrary.generated.h"

class AAIController;
class AActor;
class UEnvQueryInstanceBlueprintWrapper;

/** Gravity-aware Move To entry points that work with ANY native or Blueprint AIController.
 * Add Orbit AI Path Follower to the possessed Character. No special controller subclass required.
 * These methods do not replace ordinary Unreal MoveTo / Recast queries.
 */
UCLASS()
class ORBIT_API UOrbitAIBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Returns the component on the controller's possessed Character, or null when unsupported. */
    UFUNCTION(BlueprintPure, Category="Orbit|AI|Move")
    static UOrbitAIPathFollowerComponent* GetOrbitFollower(AAIController* Controller);

    /** Starts a gravity-aware move, returning a request ID for safe later cancellation. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Move", meta=(DisplayName="Orbit Move To Location"))
    static bool MoveToLocation(AAIController* Controller, FVector Destination, FVector SphereCenter,
        float SurfaceRadius, const FOrbitSphericalNavigationSettings& Settings, int32& OutRequestId);

    /** Follows a moving Actor using Orbit's existing replanning and terminal failure policy. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Move", meta=(DisplayName="Orbit Move To Actor"))
    static bool MoveToActor(AAIController* Controller, AActor* Target, FVector SphereCenter,
        float SurfaceRadius, const FOrbitSphericalNavigationSettings& Settings, int32& OutRequestId);

    /** Use the best completed native EQS location to start an Orbit spherical move.
     * Call after the normal EQS On Query Finished event, not while the query is running.
     * This does not execute the query or substitute straight-line Recast movement.
     * A failed/empty result leaves the currently active Orbit move untouched.
     */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|EQS", meta=(DisplayName="Orbit Move To Best EQS Location"))
    static bool MoveToBestEQSLocation(AAIController* Controller,
        UEnvQueryInstanceBlueprintWrapper* CompletedQuery, FVector SphereCenter,
        float SurfaceRadius, const FOrbitSphericalNavigationSettings& Settings,
        int32& OutRequestId);

    /** Aborts ONLY the supplied request; does not interfere with replacement Blueprint/BT/StateTree moves. */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Move", meta=(DisplayName="Abort Orbit Move Request"))
    static bool AbortRequest(AAIController* Controller, int32 RequestId);

    /** True only when this exact movement request is still the follower's current request.
     * Useful in asynchronous Blueprint sequences and when BT / StateTree may replace movement.
     */
    UFUNCTION(BlueprintPure, Category="Orbit|AI|Move", meta=(DisplayName="Is Orbit Move Request Current"))
    static bool IsRequestCurrent(AAIController* Controller, int32 RequestId);

    /** Request-specific status. Returns false when the ID is stale, rather than attributing
     * a replacement request's status to the original Blueprint / StateTree caller.
     */
    UFUNCTION(BlueprintCallable, Category="Orbit|AI|Move", meta=(DisplayName="Get Orbit Move Request Status"))
    static bool GetRequestStatus(AAIController* Controller, int32 RequestId,
        FOrbitAIMoveDiagnostics& OutDiagnostics);

    /** Reads the current or last-completed Orbit move without updating/interrupting it. */
    UFUNCTION(BlueprintPure, Category="Orbit|AI|Move", meta=(DisplayName="Get Orbit Move Status"))
    static FOrbitAIMoveDiagnostics GetMoveStatus(AAIController* Controller);
};
