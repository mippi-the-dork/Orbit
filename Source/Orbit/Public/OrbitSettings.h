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

    /** Set temporary native Chaos linear/angular damping on supported bodies.
     *  Orbit restores their original component damping on contact exit.
     *  Does NOT inject separate corrective AddForce/AddTorque calls. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(DisplayName="Damp Supported Rigid Body Motion"))
    bool bDampSupportedRigidBodyMotion = true;

    /** Native linear damping applied only while supported (acts on all velocity axes).
     *  The property name is retained so existing project settings remain readable. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(DisplayName="Supported Linear Damping", ClampMin="0.0", ClampMax="20.0", UIMin="0.0", UIMax="5.0"))
    float SupportedTangentialDamping = 1.5f;

    /** Native Chaos angular damping applied only while supported. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="20.0", UIMin="0.0", UIMax="6.0"))
    float SupportedAngularDamping = 2.0f;

    /** Distance for a short geometry-based support sweep, measured in cm. No damping away from near-contact. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.1", ClampMax="15.0", Units="cm"))
    float RestSupportProbeDistance = 3.0f;

    /** Avoid applying contact damping during fast impacts or bounce flight (cm/s along local Down). */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="500.0", Units="cm/s"))
    float RestMaxNormalContactSpeed = 75.0f;

    /** Short hysteresis for transient collision-sweep misses while a body stays near the support.
     *  Unlike friction, this does not add damping strength or keep bodies supported during lift-off. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="0.25", Units="s"))
    float RestSupportContactGraceSeconds = 0.08f;

    /** Off by default; cyan=verified, blue=brief query grace, amber=no support.
     *  Red=verified support normal significantly disagrees with gravity Up.
     *  Includes source-to-support-center offset and native gravity state diagnostics. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest|Debug")
    bool bDrawPhysicsRestContactDebug = false;

    /** Global safety toggle; disable if another system is responsible for physical gravity. */
    UPROPERTY(EditAnywhere, Config, Category="Physics")
    bool bEnablePhysicsIntegration = true;
};
