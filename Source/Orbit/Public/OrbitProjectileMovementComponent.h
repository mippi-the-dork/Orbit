#pragma once

#include "CoreMinimal.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "OrbitGravityTypes.h"
#include "OrbitProjectileMovementComponent.generated.h"

/** What a non-physics projectile does when no Orbit gravity field influences it. */
UENUM(BlueprintType)
enum class EOrbitProjectileOutsideField : uint8
{
    ProjectSettings UMETA(DisplayName="Use Orbit Project Settings"),
    NativeGravity UMETA(DisplayName="Native Projectile Gravity"),
    ZeroGravity UMETA(DisplayName="Zero Gravity")
};

/**
 * Drop-in UProjectileMovementComponent alternative for gravity-driven, non-simulated projectiles.
 * Retains Epic's native projectile movement, bouncing, homing, velocity interpolation,
 * sub-stepping and forces; only replaces the gravity acceleration inside ComputeAcceleration.
 * Like native ProjectileMovement, it does not own a component simulating Chaos physics.
 * When physics simulation is enabled on the updated primitive, Orbit's rigid-body integration
 * is responsible instead. Do not add both movement controllers to the same projectile.
 */
UCLASS(ClassGroup=(Orbit), meta=(BlueprintSpawnableComponent), BlueprintType)
class ORBIT_API UOrbitProjectileMovementComponent : public UProjectileMovementComponent
{
    GENERATED_BODY()
public:
    UOrbitProjectileMovementComponent();

    /** Off retains exact native projectile acceleration, with no gravity field sampling. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Projectile")
    bool bUseOrbitGravity = true;

    /** Whether a projectile outside an eligible Orbit field uses native or zero gravity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Projectile")
    EOrbitProjectileOutsideField OutsideFieldBehavior = EOrbitProjectileOutsideField::ProjectSettings;

    /** Physics sources are the default; General includes gravity sources without type filtering. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Projectile")
    EOrbitGravityChannel GravityChannel = EOrbitGravityChannel::Physics;

    /** Effective gravity acceleration this projectile would receive at its current location.
     * Includes ProjectileGravityScale, Receiver channels and OutsideFieldBehavior.
     * Does not include homing acceleration, pending force or collisions. */
    UFUNCTION(BlueprintPure, Category="Orbit|Projectile|Diagnostics", meta=(DisplayName="Get Effective Orbit Projectile Gravity"))
    FVector GetEffectiveOrbitGravityAcceleration() const;

    /** No world/object access: replaces only native Z gravity, retaining homing and queued forces.
     * NativeGravityZ is the *already scaled* GetGravityZ() value from ProjectileMovement.
     * Field acceleration is multiplied by ProjectileGravityScale, matching native semantics. */
    /** Native gravity Z after ProjectileGravityScale and physics volume rules; for preview parity. */
    UFUNCTION(BlueprintPure, Category="Orbit|Projectile|Diagnostics")
    float GetNativeProjectileGravityZForPreview() const { return GetGravityZ(); }

    /** Approximate impact response for a simple preview bounce, independent of world gravity axes.
     * Tangential friction and normal restitution are clamped to 0..1. */
    static FVector PredictBounceVelocity(const FVector& IncomingVelocity, const FVector& ImpactNormal,
        float Restitution, float TangentialFriction);

    static FVector ReplaceGravityTerm(const FVector& NativeAcceleration, double NativeGravityZ,
        const FOrbitGravitySample& Sample, double GravityScale, bool bUseOrbit,
        bool bAllowNativeGravityOutsideFields);

protected:
    virtual FVector ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const override;
};
