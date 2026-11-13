#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravityReceiverComponent.generated.h"

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

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintPure, Category="Orbit|Channels")
    int32 GetGravityChannelMask() const { return static_cast<int32>(GravityChannels.ToMask()); }
};
