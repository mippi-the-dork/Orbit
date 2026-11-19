#include "OrbitGravityReceiverComponent.h"
#include "OrbitWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

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
        if (bEnableArticulatedBodyOwnership)
        {
            TInlineComponentArray<USkeletalMeshComponent*> Meshes;
            GetOwner()->GetComponents(Meshes);
            for (USkeletalMeshComponent* Mesh : Meshes) { PrepareArticulatedPhysicsBodiesNow(Mesh); }
        }
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

bool UOrbitGravityReceiverComponent::PreparePhysicsBodyNow(UStaticMeshComponent* Body)
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!IsValid(Body) || !IsValid(Owner) || !World || !World->IsGameWorld() ||
        !IsRegistered() || Owner->GetWorld() != World || Body->GetOwner() != Owner ||
        Body->GetWorld() != World || !Body->IsRegistered() ||
        !Body->IsSimulatingPhysics())
    {
        return false;
    }
    UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Subsystem && Subsystem->PrepareSpawnedPhysicsBody(Body) &&
        Subsystem->IsCurrentExperimentalBodyOwned(Body);
}

int32 UOrbitGravityReceiverComponent::PrepareArticulatedPhysicsBodiesNow(USkeletalMeshComponent* SkeletalMesh)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!bEnableArticulatedBodyOwnership || !IsValid(SkeletalMesh) || !IsValid(Owner) ||
        SkeletalMesh->GetOwner() != Owner || !IsRegistered() || !World ||
        !World->IsGameWorld() || SkeletalMesh->GetWorld() != World ||
        !SkeletalMesh->IsRegistered()) { return 0; }
    UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Subsystem ? Subsystem->PrepareArticulatedPhysicsBodies(SkeletalMesh) : 0;
}

int32 UOrbitGravityReceiverComponent::ReleaseArticulatedPhysicsBodiesNow(USkeletalMeshComponent* SkeletalMesh)
{
    const AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!IsValid(SkeletalMesh) || !IsValid(Owner) || !World ||
        SkeletalMesh->GetOwner() != Owner || SkeletalMesh->GetWorld() != World ||
        !World->IsGameWorld()) { return 0; }
    UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Subsystem ? Subsystem->ReleaseArticulatedPhysicsBodies(SkeletalMesh) : 0;
}

int32 UOrbitGravityReceiverComponent::GetArticulatedOwnedBodyCount(USkeletalMeshComponent* SkeletalMesh) const
{
    const AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!IsValid(SkeletalMesh) || !IsValid(Owner) || !World ||
        SkeletalMesh->GetOwner() != Owner || SkeletalMesh->GetWorld() != World) { return 0; }
    const UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Subsystem ? Subsystem->GetArticulatedOwnedBodyCount(SkeletalMesh) : 0;
}

TArray<FName> UOrbitGravityReceiverComponent::GetArticulatedOwnedBoneNames(USkeletalMeshComponent* SkeletalMesh) const
{
    const AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!IsValid(SkeletalMesh) || !IsValid(Owner) || !World ||
        SkeletalMesh->GetOwner() != Owner || SkeletalMesh->GetWorld() != World)
    {
        return TArray<FName>();
    }
    const UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Subsystem ? Subsystem->GetArticulatedOwnedBoneNames(SkeletalMesh) : TArray<FName>();
}

int32 UOrbitGravityReceiverComponent::SetArticulatedOwnershipActive(bool bEnabled, bool bPrepareNow)
{
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !World || !World->IsGameWorld() ||
        Owner->GetWorld() != World || !IsRegistered()) { return 0; }
    UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Subsystem) { return 0; }

    bEnableArticulatedBodyOwnership = bEnabled;
    if (!bEnabled)
    {
        // Release before returning; caller need not wait for the next subsystem tick.
        Subsystem->ReleaseArticulatedClaimsForActor(Owner);
        return 0;
    }
    if (!bPrepareNow) { return 0; }
    int32 Owned = 0;
    TInlineComponentArray<USkeletalMeshComponent*> Meshes;
    Owner->GetComponents(Meshes);
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        if (IsValid(Mesh) && Mesh->IsRegistered())
        {
            Owned += PrepareArticulatedPhysicsBodiesNow(Mesh);
        }
    }
    return Owned;
}

void UOrbitGravityReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Subsystem->ReleaseArticulatedClaimsForActor(GetOwner());
        }
    }
    Super::EndPlay(EndPlayReason);
}

void UOrbitGravityReceiverComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Subsystem->ReleaseArticulatedClaimsForActor(GetOwner());
        }
    }
    Super::OnUnregister();
}

void UOrbitGravityReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!HasBegunPlay() || (!bAutoPrepareChangedPhysicsBodies && !bEnableArticulatedBodyOwnership)) { return; }
    UWorld* World = GetWorld();
    AActor* Owner = GetOwner();
    if (!World || !World->IsGameWorld() || !IsValid(Owner) || !IsRegistered() ||
        Owner->GetWorld() != World) { return; }
    UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Subsystem) { return; }

    // Do not perform the heavyweight snapshot/bridge preparation for bodies
    // already owned by their current Chaos particle. This also finds component
    // additions and physics-state recreation without relying on UObject identity.
    if (bEnableArticulatedBodyOwnership && bAutoPrepareChangedPhysicsBodies)
    {
        TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
        Owner->GetComponents(SkeletalMeshes);
        for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
        {
            if (IsValid(Mesh) && Mesh->IsRegistered()) { PrepareArticulatedPhysicsBodiesNow(Mesh); }
        }
    }
    if (!bAutoPrepareChangedPhysicsBodies) { return; }
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
