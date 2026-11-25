#include "OrbitAIController.h"
#include "OrbitAIPathFollowerComponent.h"
#include "GameFramework/Character.h"

UOrbitAIPathFollowerComponent* AOrbitAIController::GetOrbitPathFollower() const
{
    const ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn());
    return ControlledCharacter ? ControlledCharacter->FindComponentByClass<UOrbitAIPathFollowerComponent>() : nullptr;
}

bool AOrbitAIController::MoveOrbitToLocation(FVector Destination, FVector SphereCenter, float SurfaceRadius,
    const FOrbitSphericalNavigationSettings& Settings)
{
    UOrbitAIPathFollowerComponent* Follower = GetOrbitPathFollower();
    if (!Follower) return false;
    // A replacement request must terminate the previous movement before planning.
    Follower->StopFollowing();
    return Follower->FollowSphericalSurfacePath(Destination, SphereCenter, SurfaceRadius, Settings);
}

bool AOrbitAIController::MoveOrbitToActor(AActor* Target, FVector SphereCenter, float SurfaceRadius,
    const FOrbitSphericalNavigationSettings& Settings)
{
    UOrbitAIPathFollowerComponent* Follower = GetOrbitPathFollower();
    if (!Follower || !IsValid(Target) || Target == GetPawn()) return false;
    Follower->StopFollowing();
    return Follower->FollowSphericalSurfaceActor(Target, SphereCenter, SurfaceRadius, Settings);
}

void AOrbitAIController::StopOrbitMovement()
{
    if (UOrbitAIPathFollowerComponent* Follower = GetOrbitPathFollower()) Follower->StopFollowing();
}

void AOrbitAIController::OnUnPossess()
{
    StopOrbitMovement();
    Super::OnUnPossess();
}

FOrbitAIMoveDiagnostics AOrbitAIController::GetOrbitMoveDiagnostics() const
{
    if (const UOrbitAIPathFollowerComponent* Follower = GetOrbitPathFollower())
    {
        return Follower->GetMoveDiagnostics();
    }
    return FOrbitAIMoveDiagnostics();
}
