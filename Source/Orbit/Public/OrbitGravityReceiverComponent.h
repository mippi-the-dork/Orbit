#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravityReceiverComponent.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;

/**
 * Optional lighting-style gravity channel selection for an Actor.
 * With no Receiver component, ALL actors and physics components use channel 0.
 * A Receiver affects all primitive components owned by its actor and the
 * Orbit Character Gravity adapter; it does not change native Enable Gravity.
 */
UCLASS(ClassGroup=(Orbit), meta=(BlueprintSpawnableComponent), BlueprintType)
class ORBIT_API UOrbitGravityReceiverComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOrbitGravityReceiverComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Channels")
    FOrbitGravityChannels GravityChannels;

    /** Runtime status for the last simulated primitive processed on this actor.
     *  For actors with multiple simulated primitives, query each one through the
     *  Orbit World Subsystem Get Physics Body Diagnostics function instead. */
    UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category="Orbit|Physics|Diagnostics")
    FOrbitPhysicsBodyDiagnostics LastPhysicsBodyDiagnostics;

    /** Opt-in for runtime-spawned Actors using Experimental Chaos gravity.
     *  At this receiver's BeginPlay, prepare every eligible already-registered
     *  simulating Static Mesh on its owner before the next physics step.
     *  Configure sources, receiver channels, and simulated meshes beforehand.
     *  This does not watch components added or enabled AFTER BeginPlay, and
     *  never changes Standard gravity or unsupported-body fallback.
     *  Requires the project setting Experimental: Claim Gravity Before BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Physics|Experimental",
        meta=(DisplayName="Prepare Owner Physics Bodies On BeginPlay"))
    bool bPreparePhysicsBodiesOnBeginPlay = false;

    /** Opt-in lifecycle watch for runtime Static Mesh bodies added, re-enabled, or
     *  recreated after BeginPlay. Scans this Actor on its Pre-Physics component
     *  tick, preparing only bodies without a current Experimental Chaos claim.
     *  This is NOT an instant registration callback: a body added after the
     *  receiver tick may simulate one step before Orbit sees it. Enable the
     *  BeginPlay option or use Prepare Physics Bodies Now when an immediate
     *  before-first-step claim is required. Default OFF for existing projects. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Physics|Experimental",
        meta=(DisplayName="Auto Prepare Changed Physics Bodies"))
    bool bAutoPrepareChangedPhysicsBodies = false;

    /** Re-run ownership preparation after adding a new physics component,
     *  changing channels, or enabling simulation later in the Actor's life.
     *  Returns the count of eligible bodies; zero when Experimental is disabled. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Physics|Experimental")
    int32 PreparePhysicsBodiesNow();

    /** Explicit, synchronous preparation of one already-registered simulated Static
     *  Mesh belonging to this receiver's Actor. Call after enabling simulation or
     *  recreating physics state, but before the next physics step. Returns true
     *  only if the body's current Chaos particle is owned by Orbit.
     *  Never claims a component on another Actor, or affects Standard mode.
     *  Requires Experimental pre-BeginPlay ownership opt-in in Project Settings. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Physics|Experimental",
        meta=(DisplayName="Prepare Physics Body Now"))
    bool PreparePhysicsBodyNow(UStaticMeshComponent* Body);

    /** Experimental Chaos only: opt-in to per-bone skeletal gravity ownership.
     *  Off for all existing receivers. Does not affect Standard physics, skeletal
     *  animation, or any non-simulating bones. Requires pre-BeginPlay ownership
     *  enabled in Orbit Project Settings and the Experimental Chaos method. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Physics|Experimental",
        meta=(DisplayName="Enable Articulated Body Ownership"))
    bool bEnableArticulatedBodyOwnership = false;

    /** Explicitly claim eligible simulated bodies of a skeletal mesh owned by
     *  this receiver's Actor. Call after Set Simulate Physics/Set All Bodies Below
     *  Simulate Physics and before the next physics step. Repeated calls are safe.
     *  Returns number of currently owned bones, zero in Standard mode. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Physics|Experimental")
    int32 PrepareArticulatedPhysicsBodiesNow(USkeletalMeshComponent* SkeletalMesh);

    /** Immediately return all currently claimed physics bodies on this Skeletal
     *  Mesh to their prior Chaos gravity groups. Call before turning ragdoll off
     *  for deterministic cleanup (automatic cleanup also occurs on Tick).
     *  Does not turn off skeletal simulation itself. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Physics|Experimental")
    int32 ReleaseArticulatedPhysicsBodiesNow(USkeletalMeshComponent* SkeletalMesh);

    /** Side-effect-free diagnostic: count currently live bodies claimed from a
     *  Skeletal Mesh belonging to this receiver's Actor. */
    UFUNCTION(BlueprintPure, Category="Orbit|Physics|Experimental")
    int32 GetArticulatedOwnedBodyCount(USkeletalMeshComponent* SkeletalMesh) const;

    /** Gameplay-safe articulated ownership toggle. Disabling immediately restores
     *  all claimed skeletal bodies on this actor to their previous Chaos groups.
     *  Enabling can immediately claim currently eligible simulated bones.
     *  Returns the number of owned bodies after the operation. Project Experimental
     *  settings still gate all new claims; this does not change physics simulation. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Physics|Experimental",
        meta=(DisplayName="Set Articulated Ownership Active"))
    int32 SetArticulatedOwnershipActive(bool bEnabled, bool bPrepareNow = true);

    /** Lists the bone names that currently map to live, identity-matched Chaos
     *  particles owned by Orbit, useful for partial ragdolls and hit reactions.
     *  Does not update ownership or scan dormant bones. */
    UFUNCTION(BlueprintPure, Category="Orbit|Physics|Experimental",
        meta=(DisplayName="Get Articulated Owned Bone Names"))
    TArray<FName> GetArticulatedOwnedBoneNames(USkeletalMeshComponent* SkeletalMesh) const;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnUnregister() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintPure, Category="Orbit|Channels")
    int32 GetGravityChannelMask() const { return static_cast<int32>(GravityChannels.ToMask()); }
};
