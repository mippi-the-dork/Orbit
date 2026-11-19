#include "OrbitProjectileMovementComponent.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitSettings.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UOrbitProjectileMovementComponent::UOrbitProjectileMovementComponent()
{
    // All native projectile properties, including ProjectileGravityScale, remain inherited.
}

FVector UOrbitProjectileMovementComponent::ReplaceGravityTerm(const FVector& NativeAcceleration,
    double NativeGravityZ, const FOrbitGravitySample& Sample, double GravityScale,
    bool bUseOrbit, bool bAllowNativeGravityOutsideFields)
{
    if (!bUseOrbit || NativeAcceleration.ContainsNaN() || !FMath::IsFinite(NativeGravityZ) ||
        !FMath::IsFinite(GravityScale))
    {
        return NativeAcceleration;
    }
    if (!Sample.bHasInfluence && bAllowNativeGravityOutsideFields)
    {
        return NativeAcceleration;
    }
    const FVector Replacement = Sample.bHasInfluence && !Sample.Acceleration.ContainsNaN()
        ? Sample.Acceleration * GravityScale : FVector::ZeroVector;
    return NativeAcceleration - FVector(0.0, 0.0, NativeGravityZ) + Replacement;
}

FVector UOrbitProjectileMovementComponent::PredictBounceVelocity(const FVector& IncomingVelocity,
    const FVector& ImpactNormal, float Restitution, float TangentialFriction)
{
    if (IncomingVelocity.ContainsNaN() || ImpactNormal.ContainsNaN()) { return FVector::ZeroVector; }
    const FVector Normal = ImpactNormal.GetSafeNormal();
    if (Normal.IsNearlyZero()) { return IncomingVelocity; }
    const double Along = FVector::DotProduct(IncomingVelocity, Normal);
    if (Along >= 0.0) { return IncomingVelocity; }
    const FVector Tangent = IncomingVelocity - Along * Normal;
    return Tangent * (1.0 - FMath::Clamp(static_cast<double>(TangentialFriction), 0.0, 1.0)) -
        Along * FMath::Clamp(static_cast<double>(Restitution), 0.0, 1.0) * Normal;
}

FVector UOrbitProjectileMovementComponent::GetEffectiveOrbitGravityAcceleration() const
{
    const FVector NativeGravity(0.0, 0.0, GetGravityZ());
    if (!bUseOrbitGravity || !GetWorld() || !GetOwner())
    {
        return NativeGravity;
    }
    const UOrbitWorldSubsystem* OrbitWorld = GetWorld()->GetSubsystem<UOrbitWorldSubsystem>();
    if (!OrbitWorld)
    {
        return NativeGravity;
    }
    const FVector Location = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : GetOwner()->GetActorLocation();
    const FOrbitGravitySample Sample = OrbitWorld->SampleGravityForActor(GetOwner(), Location, GravityChannel);
    const bool bNativeFallback = OutsideFieldBehavior == EOrbitProjectileOutsideField::NativeGravity ||
        (OutsideFieldBehavior == EOrbitProjectileOutsideField::ProjectSettings &&
         GetDefault<UOrbitSettings>()->bUseWorldGravityOutsideFields);
    return ReplaceGravityTerm(NativeGravity, GetGravityZ(), Sample,
        ProjectileGravityScale, true, bNativeFallback);
}

FVector UOrbitProjectileMovementComponent::ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const
{
    const FVector NativeAcceleration = Super::ComputeAcceleration(InVelocity, DeltaTime);
    if (!bUseOrbitGravity)
    {
        return NativeAcceleration;
    }
    // Let native ProjectileMovement compute homing and queued forces first; replace only
    // its native (world-Z) gravity term, so native movement features stay intact.
    return NativeAcceleration - FVector(0.0, 0.0, GetGravityZ()) +
        GetEffectiveOrbitGravityAcceleration();
}
