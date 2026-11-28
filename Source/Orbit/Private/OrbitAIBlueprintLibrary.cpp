#include "OrbitAIBlueprintLibrary.h"
#include "AIController.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"
#include "GameFramework/Character.h"

UOrbitAIPathFollowerComponent* UOrbitAIBlueprintLibrary::GetOrbitFollower(AAIController* Controller)
{
    if (!IsValid(Controller)) return nullptr;
    ACharacter* Character = Cast<ACharacter>(Controller->GetPawn());
    return IsValid(Character) ? Character->FindComponentByClass<UOrbitAIPathFollowerComponent>() : nullptr;
}

bool UOrbitAIBlueprintLibrary::MoveToLocation(AAIController* Controller, FVector Destination,
    FVector SphereCenter, float SurfaceRadius, const FOrbitSphericalNavigationSettings& Settings, int32& OutRequestId)
{
    OutRequestId = 0;
    UOrbitAIPathFollowerComponent* Follower = GetOrbitFollower(Controller);
    if (!IsValid(Follower) || !(!Destination.ContainsNaN()) || !(!SphereCenter.ContainsNaN()) ||
        !FMath::IsFinite(SurfaceRadius) || SurfaceRadius <= 0.0f) return false;
    Follower->StopFollowing();
    if (!Follower->FollowSphericalSurfacePath(Destination, SphereCenter, SurfaceRadius, Settings)) return false;
    OutRequestId = Follower->MoveRequestId;
    return OutRequestId > 0;
}

bool UOrbitAIBlueprintLibrary::MoveToActor(AAIController* Controller, AActor* Target,
    FVector SphereCenter, float SurfaceRadius, const FOrbitSphericalNavigationSettings& Settings, int32& OutRequestId)
{
    OutRequestId = 0;
    UOrbitAIPathFollowerComponent* Follower = GetOrbitFollower(Controller);
    if (!IsValid(Follower) || !IsValid(Target) || Target == Controller->GetPawn() ||
        !(!SphereCenter.ContainsNaN()) || !FMath::IsFinite(SurfaceRadius) || SurfaceRadius <= 0.0f) return false;
    Follower->StopFollowing();
    if (!Follower->FollowSphericalSurfaceActor(Target, SphereCenter, SurfaceRadius, Settings)) return false;
    OutRequestId = Follower->MoveRequestId;
    return OutRequestId > 0;
}

bool UOrbitAIBlueprintLibrary::AbortRequest(AAIController* Controller, int32 RequestId)
{
    UOrbitAIPathFollowerComponent* Follower = GetOrbitFollower(Controller);
    return IsValid(Follower) && RequestId > 0 && Follower->AbortMoveRequest(RequestId);
}

FOrbitAIMoveDiagnostics UOrbitAIBlueprintLibrary::GetMoveStatus(AAIController* Controller)
{
    if (const UOrbitAIPathFollowerComponent* Follower = GetOrbitFollower(Controller))
        return Follower->GetMoveDiagnostics();
    return FOrbitAIMoveDiagnostics();
}

bool UOrbitAIBlueprintLibrary::IsRequestCurrent(AAIController* Controller, int32 RequestId)
{
    const UOrbitAIPathFollowerComponent* Follower = GetOrbitFollower(Controller);
    return IsValid(Follower) && RequestId > 0 && Follower->MoveRequestId == RequestId;
}

bool UOrbitAIBlueprintLibrary::GetRequestStatus(AAIController* Controller, int32 RequestId,
    FOrbitAIMoveDiagnostics& OutDiagnostics)
{
    OutDiagnostics = FOrbitAIMoveDiagnostics();
    if (!IsRequestCurrent(Controller, RequestId)) return false;
    OutDiagnostics = GetOrbitFollower(Controller)->GetMoveDiagnostics();
    return true;
}

// Standard native EQS is responsible for query execution and scoring. Orbit only
// consumes a completed location result, then uses its existing collision-verified
// spherical planner. Invalid or unfinished queries must never cancel an active move.
bool UOrbitAIBlueprintLibrary::MoveToBestEQSLocation(AAIController* Controller,
    UEnvQueryInstanceBlueprintWrapper* CompletedQuery, FVector SphereCenter,
    float SurfaceRadius, const FOrbitSphericalNavigationSettings& Settings, int32& OutRequestId)
{
    OutRequestId = 0;
    if (!IsValid(Controller) || !IsValid(CompletedQuery) ||
        !IsValid(GetOrbitFollower(Controller)))
    {
        return false;
    }
    TArray<FVector> Locations;
    if (!CompletedQuery->GetQueryResultsAsLocations(Locations) || Locations.IsEmpty() ||
        Locations[0].ContainsNaN())
    {
        return false;
    }
    // Native EQS SingleResult and AllMatching both put the best result first.
    return MoveToLocation(Controller, Locations[0], SphereCenter, SurfaceRadius,
        Settings, OutRequestId);
}
