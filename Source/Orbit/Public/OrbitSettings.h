#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OrbitGravityTypes.h"
#include "OrbitSettings.generated.h"

/** Only affects automatically managed simulating rigid bodies. Existing projects remain Standard. */
UENUM(BlueprintType)
enum class EOrbitRigidBodyGravityMethod : uint8
{
    Standard UMETA(DisplayName="Standard (Existing AddForce)"),
    ExperimentalChaosUniform UMETA(DisplayName="Experimental Chaos Gravity (Directional + Point)")
};

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

    /**
     * Standard preserves the exact existing Orbit force path. Experimental uses an
     * otherwise unused Chaos gravity group for ONE unrestricted constant
     * Directional source OR one Point source (per-particle substep evaluation).
     * Multiple ordinary Point sources require a second explicit opt-in.
     * Axis, Spline, Volume, and mixed sources fall back by default; mixed Point +
     * Directional requires its own independent, default-off opt-in.
     * Experimental is not validated for production and requires an exclusive group.
     * The C++ enum identifier remains unchanged to preserve serialized settings.
     */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Gravity Integration", meta=(DisplayName="Rigid Body Gravity Application"))
    EOrbitRigidBodyGravityMethod RigidBodyGravityMethod = EOrbitRigidBodyGravityMethod::Standard;

    /**
     * Opt-in startup experiment. Assign eligible existing static-mesh rigid bodies
     * to the Chaos experimental gravity group at WorldSubsystem::OnWorldBeginPlay,
     * before the first Actor BeginPlay and regular subsystem tick. This aims to
     * avoid one world-Z gravity step at PIE startup. Off by default even when
     * Experimental Chaos Gravity was previously selected. Runtime-spawned bodies
     * still use normal discovery, and unsupported fields/bodies are unchanged.
     * Experimental; not a guarantee of first-step ownership in every world mode.
     */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Gravity Integration",
        meta=(DisplayName="Experimental: Claim Gravity Before BeginPlay",
              EditCondition="RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform"))
    bool bExperimentalPreBeginPlayGravityOwnership = false;

    /**
     * Separate, default-off Iteration 037 experiment: use the existing Chaos
     * solver group for two or more ordinary Point gravity sources. Gravity is
     * evaluated per rigid body from a copied, immutable physics-thread field
     * snapshot, using Orbit's existing Additive / Override / Priority rules.
     * All active physics sources must be ordinary Point sources on channel 0;
     * only channel-0-only receivers may use this experimental multi-source path.
     * Other sources and receivers keep the Standard integration. The main
     * Experimental setting alone never enables multi-source integration.
     */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Gravity Integration",
        meta=(DisplayName="Experimental: Multiple Point Sources",
              EditCondition="RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform"))
    bool bExperimentalMultiplePointSources = false;

    /**
     * Iteration 038: additional default-off experiment for at least one ordinary
     * Point and one unlimited, constant Directional source. Requires the main
     * Experimental method. Kept separate from Multiple Point so projects that
     * opted in to Iteration 037 retain the same field eligibility after update.
     * Both sources and receivers must use Channel 0 only; no vortex, box mask,
     * custom gravity groups, or velocity-sensitive fields are supported.
     */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Gravity Integration",
        meta=(DisplayName="Experimental: Mixed Point + Directional Sources",
              EditCondition="RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform"))
    bool bExperimentalMixedPointDirectionalSources = false;

    /**
     * Iteration 040 compatibility experiment: allow previously unsupported, position-based
     * field shapes through the same per-particle Chaos integration. Uses Orbit's existing
     * FOrbitFieldEvaluator::CombineForBody, including rotated box bounds, Axis, Spline,
     * Directional and Volume source geometry, priority, falloff and blending.
     * Off by default even in projects that opted into earlier experimental modes.
     * Velocity-steered Vortex fields, non-default receiver channels, and non-static-mesh
     * simulated bodies stay on Standard pending separate validation. Expensive Spline
     * fields scale with sampled curve length per physics body per physics substep.
     */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Gravity Integration",
        meta=(DisplayName="Experimental: Expanded Field Compatibility",
              EditCondition="RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform"))
    bool bExperimentalExpandedFieldCompatibility = false;

    /**
     * Iteration 042: route Channel 0/1/2 source and receiver masks through an
     * identity-keyed Chaos particle snapshot, rather than assuming all bodies
     * in gravity group 7 share Channel 0. When enabled, a valid position-based
     * field configuration uses the channel-aware solver even if a single-source
     * mode would otherwise take precedence. Off unless explicitly enabled.
     * Unmapped particles receive ZERO custom acceleration, never another
     * receiver's gravity. The source/receiver mask rules remain unchanged.
     */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Gravity Integration",
        meta=(DisplayName="Experimental: Receiver Channel Compatibility",
              EditCondition="RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform"))
    bool bExperimentalReceiverChannelCompatibility = false;

    /**
     * Iteration 042: evaluate Continuous Force and Spiral Capture Vortex modes
     * inside the Chaos physics substep using each particle's current velocity.
     * Off by default. This does not alter old Standard Vortex gameplay or
     * enable nondefault channels unless Receiver Channel Compatibility is on.
     */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Gravity Integration",
        meta=(DisplayName="Experimental: Vortex Compatibility",
              EditCondition="RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform"))
    bool bExperimentalVortexCompatibility = false;

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

    /** After a manual sleep is reversed within a short interval, stop reissuing sleep
     *  requests for this duration. Native Chaos damping remains active on safe support. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="10.0", Units="s"))
    float RestWakeRetryCooldown = 2.0f;

    /** Suppress additional Orbit manual-sleep attempts after this many immediate
     *  sleep/wake failures in one resting episode. Chaos may still sleep on its own. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="1", ClampMax="10"))
    int32 RestMaxManualSleepFailures = 2;

    /** After two failed manual-sleep attempts, retry at most once per sustained
     *  stable period. This replaces permanent suppression but prevents a rapid
     *  sleep/wake loop. Not a timer for native Chaos sleeping. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="1.0", ClampMax="30.0", Units="s"))
    float RestSuppressedStableRetrySeconds = 6.0f;

    /** Continuous loss of support or meaningful motion required before another
     *  manual sleep episode may start after repeated immediate wake failures. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.1", ClampMax="5.0", Units="s"))
    float RestRearmUnstableSeconds = 0.5f;

    /** Meaningful linear speed to rearm after repeated manual sleep failures. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="5.0", ClampMax="200.0", Units="cm/s"))
    float RestRearmLinearSpeed = 20.0f;

    /** Maximum displacement during one continuous support episode before
     *  Orbit rest timer resets, including with high gravity solver velocities. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.01", ClampMax="5.0", Units="cm"))
    float RestMaxStationaryDisplacement = 0.25f;

    /** Larger rest position envelope for simple sphere collision, measured
     *  across the same stable support episode. Cubes/other shapes retain the
     *  tighter generic displacement threshold. Avoids perpetual re-anchoring
     *  from very slow supported rolling. Other speed/angular gates still apply. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.05", ClampMax="5.0", Units="cm"))
    float RestSphereStationaryDisplacement = 1.0f;

    /** Allow a bounded normal-direction solver velocity only while actual
     *  position and tangential/angular motion stay stationary on verified support. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="75.0", Units="cm/s"))
    float RestMaxStationaryNormalSpeed = 15.0f;

    /** One extra support sweep can skip a sideways obstruction to find the
     *  floor behind it; geometry collisions remain unchanged. 0 disables. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0", ClampMax="2"))
    int32 RestSupportObstructionRetries = 1;

    /** Briefly reuse a confirmed sideways obstruction to avoid a second sweep
     *  every game tick. Cache is bypassed on significant body/blocker movement,
     *  changed gravity, or support loss and always expires quickly. Zero disables. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="0.25", Units="s"))
    float RestObstructionCacheSeconds = 0.12f;

    /** In the Editor, enable Generate Wake Events on the mesh in its Details
     *  for native callbacks to appear in Motion History (not all force wakes
     *  dispatch native callbacks). Orbit never overrides that mesh setting. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest|Debug")
    bool bLogNativeWakeEvents = false;

    /** Manual sleep and contact damping will not use fast-moving simulating supports.
     *  Static supports are unaffected. This does not disable physical collision/stacking. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="100.0", Units="cm/s"))
    float RestMaxMovingSupportSpeed = 5.0f;

    /** Angular speed threshold for treating a simulating support as effectively stationary. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest", meta=(ClampMin="0.0", ClampMax="5.0", Units="rad/s"))
    float RestMaxMovingSupportAngularSpeed = 0.08f;

    /** Off by default; cyan=verified, blue=brief query grace, amber=no support.
     *  Red=verified support normal significantly disagrees with gravity Up.
     *  Includes source-to-support-center offset and native gravity state diagnostics. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Rest|Debug")
    bool bDrawPhysicsRestContactDebug = false;

    /** A/B diagnostic for FLAT test scenes only: when a body's Orbit acceleration
     *  exactly matches WorldSettings gravity (-Z and magnitude), let Chaos integrate
     *  native world gravity instead of the normal Orbit AddForce call. Never used
     *  on curved/sideways field samples. Disabled by default; do not use as a
     *  production physics integration mode until the A/B captures are evaluated. */
    UPROPERTY(EditAnywhere, Config, Category="Physics|Diagnostics",
        meta=(DisplayName="Compare Native World Gravity on Matching Fields"))
    bool bCompareNativeWorldGravityWhenEquivalent = false;

    /** Global safety toggle; disable if another system is responsible for physical gravity. */
    UPROPERTY(EditAnywhere, Config, Category="Physics")
    bool bEnablePhysicsIntegration = true;
};
