#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OrbitGravityTypes.h"
#include "OrbitSettings.generated.h"

/** Project Settings > Plugins > Orbit. Settings apply to new and existing play worlds. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Orbit"))
class ORBIT_API UOrbitSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

    /** Enables experimental automatic game-thread gravity on ordinary simulating components. */
    UPROPERTY(EditAnywhere, Config, Category="Physics", meta=(DisplayName="Automatically Apply to Physics Bodies"))
    bool bAutomaticRigidBodyGravity = true;

    /** Do not silently turn objects intentionally configured for zero gravity into normal gravity objects. */
    UPROPERTY(EditAnywhere, Config, Category="Physics")
    bool bIncludeInitiallyGravityDisabledBodies = false;

    /** Zero uses a new component discovery pass every frame; larger intervals reduce scans but delay new object pickup. */
    UPROPERTY(EditAnywhere, Config, Category="Physics", meta=(ClampMin="0.0", ClampMax="5.0", Units="s"))
    float PhysicsDiscoveryInterval = 0.20f;

    /** Use normal WorldSettings gravity outside all Orbit sources; otherwise unused regions are zero-g. */
    UPROPERTY(EditAnywhere, Config, Category="Field")
    bool bUseWorldGravityOutsideFields = true;

    /** Equal-priority Override field resolution. Higher Priority always wins first. */
    UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Field|Competition")
    EOrbitOverrideResolution OverrideResolution = EOrbitOverrideResolution::StrongestInfluence;

    /** A character keeps its previous strongest source until its challenger exceeds it by this fraction. */
    UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Field|Competition",
        meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="0.5"))
    double CharacterSwitchHysteresis = 0.15;

    /**
     * Experimental single-rigid-body rest stabilization. Once a slow-moving
     * object is supported by collision geometry, it may sleep, instead of
     * continuously receiving force and rotating forever. Does not alter damping
     * or restitution. Opt out per physics component with Orbit.NoRest.
     */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(DisplayName="Stabilize Resting Rigid Bodies"))
    bool bStabilizeRestingRigidBodies = true;

    /** Required continuous supported time before sleeping a low-speed single-body object. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.1", ClampMax="5.0", Units="s"))
    float RestTime = 0.65f;

    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="50.0", Units="cm/s"))
    float RestLinearSpeed = 5.0f;

    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="1.0", Units="rad/s"))
    float RestAngularSpeed = 0.08f;

    /** Global safety toggle; disable if another system is responsible for physical gravity. */
    UPROPERTY(EditAnywhere, Config, Category="Physics")
    bool bEnablePhysicsIntegration = true;
};
