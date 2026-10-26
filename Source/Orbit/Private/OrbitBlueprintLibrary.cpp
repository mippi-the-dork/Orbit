#include "OrbitBlueprintLibrary.h"
#include "OrbitWorldSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

FOrbitGravitySample UOrbitBlueprintLibrary::SampleOrbitGravity(const UObject* WorldContextObject, FVector WorldPosition, EOrbitGravityChannel Channel)
{
    FOrbitGravitySample Empty;
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) { return Empty; }
    const UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Gravity ? Gravity->SampleGravity(WorldPosition, Channel) : Empty;
}

FOrbitGravitySample UOrbitBlueprintLibrary::SampleOrbitGravityForActor(const UObject* WorldContextObject, const AActor* Actor, FVector WorldPosition, EOrbitGravityChannel Channel)
{
    FOrbitGravitySample Empty;
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || !Actor) { return Empty; }
    const UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Gravity ? Gravity->SampleGravityForActor(Actor, WorldPosition, Channel) : Empty;
}

FVector UOrbitBlueprintLibrary::ProjectOntoGravityPlane(FVector Vector, FVector Up)
{
    if (Up.IsNearlyZero()) { return Vector; }
    return FVector::VectorPlaneProject(Vector, Up.GetSafeNormal());
}

bool UOrbitBlueprintLibrary::GravityUpFromAcceleration(FVector Acceleration, FVector& Up)
{
    Up = FVector::UpVector;
    if (Acceleration.ContainsNaN() || Acceleration.IsNearlyZero()) { return false; }
    Up = -Acceleration.GetSafeNormal();
    return true;
}
