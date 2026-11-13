#include "OrbitGravityReceiverComponent.h"
#include "OrbitWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"

UOrbitGravityReceiverComponent::UOrbitGravityReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UOrbitGravityReceiverComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bPreparePhysicsBodiesOnBeginPlay)
    {
        PreparePhysicsBodiesNow();
    }
}

int32 UOrbitGravityReceiverComponent::PreparePhysicsBodiesNow()
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !World || !World->IsGameWorld() ||
        Owner->GetWorld() != World || !IsRegistered())
    {
        return 0;
    }
    UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Subsystem ? Subsystem->PrepareSpawnedPhysicsActor(Owner) : 0;
}

void UOrbitGravityReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bAutoPrepareChangedPhysicsBodies || !HasBegunPlay()) { return; }
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !World->IsGameWorld() || !IsValid(Owner) || !IsRegistered() ||
        Owner->GetWorld() != World) { return; }
    UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Subsystem) { return; }

    // Do not perform the heavyweight snapshot/bridge preparation for bodies
    // already owned by their current Chaos particle. This also finds component
    // additions and physics-state recreation without relying on UObject identity.
    TInlineComponentArray<UStaticMeshComponent*> Components;
    Owner->GetComponents(Components);
    for (UStaticMeshComponent* Mesh : Components)
    {
        if (IsValid(Mesh) && Mesh->IsRegistered() && Mesh->IsSimulatingPhysics() &&
            Mesh->IsGravityEnabled() &&
            !Mesh->ComponentHasTag(FName(TEXT("Orbit.Ignore"))) &&
            !Subsystem->IsCurrentExperimentalBodyOwned(Mesh))
        {
            Subsystem->PrepareSpawnedPhysicsBody(Mesh);
        }
    }
}
