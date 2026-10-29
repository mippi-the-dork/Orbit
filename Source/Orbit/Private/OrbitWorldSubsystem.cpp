#include "OrbitWorldSubsystem.h"

#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitGravityReceiverComponent.h"
#include "OrbitPhysicsRestMath.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "OrbitSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Stats/Stats.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrbitWorld, Log, All);

bool UOrbitWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld();
}

void UOrbitWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    DiscoveryAccumulator = 0.0f;
    bNeedsInitialDiscovery = true;
}

void UOrbitWorldSubsystem::Deinitialize()
{
    RestoreBodyGravity();
    ActiveFields.Empty();
    Sources.Empty();
    SourceComponents.Empty();
    Super::Deinitialize();
}

TStatId UOrbitWorldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UOrbitWorldSubsystem, STATGROUP_Tickables);
}

void UOrbitWorldSubsystem::RefreshActiveSources()
{
    UpdateSourceSnapshot();
}

void UOrbitWorldSubsystem::RegisterSource(AOrbitGravitySource* Source)
{
    if (IsValid(Source) && Source->GetWorld() == GetWorld())
    {
        Sources.AddUnique(TWeakObjectPtr<AOrbitGravitySource>(Source));
        bNeedsInitialDiscovery = true;
        UpdateSourceSnapshot();
    }
}

void UOrbitWorldSubsystem::UnregisterSource(AOrbitGravitySource* Source)
{
    Sources.RemoveAll([Source](const TWeakObjectPtr<AOrbitGravitySource>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Source;
    });
    UpdateSourceSnapshot();
    if (ActiveFields.IsEmpty())
    {
        RestoreBodyGravity();
    }
}

void UOrbitWorldSubsystem::RegisterSourceComponent(UOrbitGravitySourceComponent* Component)
{
    if (IsValid(Component) && Component->GetWorld() == GetWorld())
    {
        SourceComponents.AddUnique(TWeakObjectPtr<UOrbitGravitySourceComponent>(Component));
        bNeedsInitialDiscovery = true;
        UpdateSourceSnapshot();
    }
}

void UOrbitWorldSubsystem::UnregisterSourceComponent(UOrbitGravitySourceComponent* Component)
{
    SourceComponents.RemoveAll([Component](const TWeakObjectPtr<UOrbitGravitySourceComponent>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Component;
    });
    UpdateSourceSnapshot();
}

void UOrbitWorldSubsystem::UpdateSourceSnapshot()
{
    ActiveFields.Reset();
    for (int32 Index = Sources.Num() - 1; Index >= 0; --Index)
    {
        AOrbitGravitySource* Source = Sources[Index].Get();
        if (!IsValid(Source))
        {
            Sources.RemoveAtSwap(Index);
        }
        else if (Source->bEnabled)
        {
            FOrbitFieldDefinition& Definition = ActiveFields.AddDefaulted_GetRef();
            Source->BuildDefinition(Definition);
        }
    }
    for (int32 Index = SourceComponents.Num() - 1; Index >= 0; --Index)
    {
        UOrbitGravitySourceComponent* Source = SourceComponents[Index].Get();
        if (!IsValid(Source) || !Source->IsRegistered()) { SourceComponents.RemoveAtSwap(Index); }
        else if (Source->bEnabled)
        {
            FOrbitFieldDefinition& Definition = ActiveFields.AddDefaulted_GetRef();
            Source->BuildDefinition(Definition);
        }
    }
}

uint8 UOrbitWorldSubsystem::GetReceiverChannelMask(const AActor* Owner) const
{
    // Like Lighting Channels, a missing receiver component means channel 0.
    const UOrbitGravityReceiverComponent* Receiver =
        Owner ? Owner->FindComponentByClass<UOrbitGravityReceiverComponent>() : nullptr;
    return Receiver ? Receiver->GravityChannels.ToMask() : 1u;
}

bool UOrbitWorldSubsystem::HasCompatiblePhysicsSource(uint8 ReceiverMask) const
{
    for (const FOrbitFieldDefinition& Field : ActiveFields)
    {
        if (Field.bPhysics && (Field.ChannelMask & ReceiverMask) != 0u)
        {
            return true;
        }
    }
    return false;
}

FOrbitGravitySample UOrbitWorldSubsystem::SampleGravity(const FVector& WorldLocation, EOrbitGravityChannel Channel) const
{
    return SampleGravityWithChannels(WorldLocation, FOrbitGravityChannels(), Channel);
}

FOrbitGravitySample UOrbitWorldSubsystem::SampleGravityWithChannels(
    const FVector& WorldLocation, FOrbitGravityChannels ReceiverChannels, EOrbitGravityChannel Channel) const
{
    FOrbitGravitySample Result = FOrbitFieldEvaluator::Combine(ActiveFields, WorldLocation, Channel,
        ReceiverChannels.ToMask(), GetDefault<UOrbitSettings>()->OverrideResolution);
    if (!Result.bHasInfluence)
    {
        const UOrbitSettings* Settings = GetDefault<UOrbitSettings>();
        if (Settings->bUseWorldGravityOutsideFields && GetWorld())
        {
            Result.Acceleration = FVector(0.0, 0.0, GetWorld()->GetGravityZ());
            Result.Magnitude = Result.Acceleration.Size();
            Result.bHasValidDirection = Result.Magnitude > UE_DOUBLE_SMALL_NUMBER;
            Result.Down = Result.bHasValidDirection ? Result.Acceleration / Result.Magnitude : FVector::ZeroVector;
            Result.Up = Result.bHasValidDirection ? -Result.Down : FVector::UpVector;
        }
    }
    return Result;
}

FOrbitGravitySample UOrbitWorldSubsystem::SampleGravityForActor(
    const AActor* Actor, const FVector& WorldLocation, EOrbitGravityChannel Channel) const
{
    FOrbitGravityChannels Channels;
    const uint8 Mask = GetReceiverChannelMask(Actor);
    Channels.bChannel0 = (Mask & 1u) != 0u;
    Channels.bChannel1 = (Mask & 2u) != 0u;
    Channels.bChannel2 = (Mask & 4u) != 0u;
    return SampleGravityWithChannels(WorldLocation, Channels, Channel);
}

FOrbitGravitySample UOrbitWorldSubsystem::SampleGravityForActorStable(
    const AActor* Actor, const FVector& WorldLocation, EOrbitGravityChannel Channel,
    int32 PreferredSourceId, double Hysteresis) const
{
    FOrbitGravitySample Result = FOrbitFieldEvaluator::Combine(ActiveFields, WorldLocation, Channel,
        GetReceiverChannelMask(Actor), GetDefault<UOrbitSettings>()->OverrideResolution,
        PreferredSourceId, Hysteresis);
    if (!Result.bHasInfluence && GetDefault<UOrbitSettings>()->bUseWorldGravityOutsideFields && GetWorld())
    {
        Result.Acceleration = FVector(0.0, 0.0, GetWorld()->GetGravityZ());
        Result.Magnitude = Result.Acceleration.Size();
        Result.bHasValidDirection = Result.Magnitude > UE_DOUBLE_SMALL_NUMBER;
        Result.Down = Result.bHasValidDirection ? Result.Acceleration / Result.Magnitude : FVector::ZeroVector;
        Result.Up = Result.bHasValidDirection ? -Result.Down : FVector::UpVector;
    }
    return Result;
}

FOrbitGravitySample UOrbitWorldSubsystem::SampleIndividualSource(const UObject* Source,
    const FVector& WorldLocation, EOrbitGravityChannel Channel, uint8 ReceiverMask) const
{
    FOrbitFieldDefinition Field;
    if (const AOrbitGravitySource* ActorSource = Cast<AOrbitGravitySource>(Source))
    {
        if (ActorSource->GetWorld() != GetWorld() || !ActorSource->bEnabled) { return FOrbitGravitySample(); }
        ActorSource->BuildDefinition(Field);
    }
    else if (const UOrbitGravitySourceComponent* Component = Cast<UOrbitGravitySourceComponent>(Source))
    {
        if (Component->GetWorld() != GetWorld() || !Component->bEnabled) { return FOrbitGravitySample(); }
        Component->BuildDefinition(Field);
    }
    else { return FOrbitGravitySample(); }
    TArray<FOrbitFieldDefinition> Fields;
    Fields.Add(MoveTemp(Field));
    return FOrbitFieldEvaluator::Combine(Fields, WorldLocation, Channel, ReceiverMask,
        GetDefault<UOrbitSettings>()->OverrideResolution);
}

FOrbitGravitySample UOrbitWorldSubsystem::SampleGravityForComponent(
    const UPrimitiveComponent* Component, EOrbitGravityChannel Channel) const
{
    // For simulating bodies, the force is applied at the physics center of mass,
    // which can differ from the Actor/Component pivot on imported meshes.
    return Component ? SampleGravityForActor(Component->GetOwner(),
        Component->IsSimulatingPhysics() ? Component->GetCenterOfMass(NAME_None) : Component->GetComponentLocation(), Channel)
        : FOrbitGravitySample();
}

FVector UOrbitWorldSubsystem::GetGravityAcceleration(const FVector& WorldLocation, EOrbitGravityChannel Channel) const
{
    return SampleGravity(WorldLocation, Channel).Acceleration;
}

bool UOrbitWorldSubsystem::HasManagedBody(const UPrimitiveComponent* Component) const
{
    for (const FManagedBody& Body : ManagedBodies)
    {
        if (Body.Component.Get() == Component) { return true; }
    }
    return false;
}

void UOrbitWorldSubsystem::DiscoverBodies()
{
    const UOrbitSettings* Settings = GetDefault<UOrbitSettings>();
    static const FName IgnoreTag(TEXT("Orbit.Ignore"));
    static const FName ForceTag(TEXT("Orbit.Force"));
    for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
    {
        UPrimitiveComponent* Component = *It;
        if (!IsValid(Component) || Component->GetWorld() != GetWorld() ||
            !Component->IsRegistered() || !Component->IsSimulatingPhysics() ||
            Component->ComponentHasTag(IgnoreTag) || HasManagedBody(Component))
        {
            continue;
        }
        const bool bNativeGravity = Component->IsGravityEnabled();
        if (!bNativeGravity && !Settings->bIncludeInitiallyGravityDisabledBodies && !Component->ComponentHasTag(ForceTag))
        {
            continue;
        }
        // Crucial: unmatched channels and outside-field bodies retain native Chaos
        // gravity, and therefore its usual collision settling and sleeping behavior.
        const FOrbitGravitySample Sample = SampleGravityForComponent(Component, EOrbitGravityChannel::Physics);
        if (!Sample.bHasInfluence &&
            (Settings->bUseWorldGravityOutsideFields ||
             !HasCompatiblePhysicsSource(GetReceiverChannelMask(Component->GetOwner()))))
        {
            // In particular, channel-mismatched objects must keep native
            // gravity, even when Orbit zero-g fallback is configured.
            continue;
        }
        FManagedBody NewBody;
        NewBody.Component = Component;
        NewBody.bOriginalGravity = bNativeGravity;
        ManagedBodies.Add(NewBody);
        Component->SetEnableGravity(false);
    }
}

void UOrbitWorldSubsystem::RestoreBodyGravity()
{
    for (FManagedBody& Body : ManagedBodies)
    {
        UPrimitiveComponent* Component = Body.Component.Get();
        if (IsValid(Component) && Component->IsRegistered())
        {
            Component->SetEnableGravity(Body.bOriginalGravity);
        }
    }
    ManagedBodies.Empty();
}

UOrbitWorldSubsystem::FRestSupportProbe UOrbitWorldSubsystem::ProbeSupportAlongGravity(
    UPrimitiveComponent* Component, const FVector& Down, float ProbeDistance) const
{
    FRestSupportProbe Probe;
    if (!Component || !GetWorld() || Down.IsNearlyZero() || !Component->GetOwner())
    {
        return Probe;
    }

    // This is still a near-contact query, NOT a Chaos manifold read.
    // A real hit is required before the short contact grace can be used.
    const FVector UnitDown = Down.GetSafeNormal();
    const double Distance = FMath::Max(0.1, static_cast<double>(ProbeDistance));
    const FVector Origin = Component->GetComponentLocation();
    const FVector Start = Origin - UnitDown * Distance;
    const FVector End = Origin + UnitDown * Distance;
    FComponentQueryParams Params;
    Params.AddIgnoredActor(Component->GetOwner());
    Params.bTraceComplex = false;

    TArray<FHitResult> Hits;
    GetWorld()->ComponentSweepMulti(Hits, Component, Start, End, Component->GetComponentQuat(), Params);
    double BestAlignment = 0.7;
    for (const FHitResult& Hit : Hits)
    {
        if (!Hit.bBlockingHit || !Hit.GetComponent() || Hit.GetComponent() == Component)
        {
            continue;
        }
        const double Alignment = OrbitPhysicsRestMath::SupportUpAlignment(Down, Hit.ImpactNormal);
        if (Alignment < BestAlignment) { continue; }
        BestAlignment = Alignment;
        Probe.bHit = true;
        Probe.bStartedPenetrating = Hit.bStartPenetrating;
        Probe.Normal = Hit.ImpactNormal.GetSafeNormal();
        Probe.ImpactPoint = Hit.ImpactPoint;
        Probe.SupportComponent = Hit.GetComponent();
    }
    return Probe;
}

void UOrbitWorldSubsystem::ApplyPhysicsForces(float DeltaTime)
{
    const UOrbitSettings* Settings = GetDefault<UOrbitSettings>();
    static const FName IgnoreTag(TEXT("Orbit.Ignore"));
    static const FName NoRestTag(TEXT("Orbit.NoRest"));
    for (int32 Index = ManagedBodies.Num() - 1; Index >= 0; --Index)
    {
        FManagedBody& Body = ManagedBodies[Index];
        UPrimitiveComponent* Component = Body.Component.Get();
        if (!IsValid(Component) || Component->GetWorld() != GetWorld() || !Component->IsRegistered())
        {
            ManagedBodies.RemoveAtSwap(Index);
            continue;
        }
        if (!Component->IsSimulatingPhysics() || Component->ComponentHasTag(IgnoreTag))
        {
            Component->SetEnableGravity(Body.bOriginalGravity);
            ManagedBodies.RemoveAtSwap(Index);
            continue;
        }

        const FOrbitGravitySample Sample = FOrbitFieldEvaluator::CombineForBody(
            ActiveFields, Component->GetCenterOfMass(NAME_None), Component->GetPhysicsLinearVelocity(),
            GetReceiverChannelMask(Component->GetOwner()), GetDefault<UOrbitSettings>()->OverrideResolution);
        if (!Sample.bHasInfluence &&
            (Settings->bUseWorldGravityOutsideFields ||
             !HasCompatiblePhysicsSource(GetReceiverChannelMask(Component->GetOwner()))))
        {
            // An unmatched channel is never taken over by Orbit. Outside-field
            // objects also revert to native gravity when fallback is enabled.
            Component->SetEnableGravity(Body.bOriginalGravity);
            ManagedBodies.RemoveAtSwap(Index);
            continue;
        }
        if (Component->IsGravityEnabled())
        {
            Component->SetEnableGravity(false);
        }

        const FVector Acceleration = Sample.Acceleration;
        if (!Component->IsAnyRigidBodyAwake())
        {
            // Never re-awaken a resting body with the same gravity every tick.
            // Wake if the local field changed substantially while it slept.
            if (FVector::DistSquared(Acceleration, Body.LastAcceleration) > FMath::Square(20.0))
            {
                Component->WakeAllRigidBodies();
            }
            else
            {
                continue;
            }
        }
        Body.LastAcceleration = Acceleration;

        // Both rest damping and the existing sleep check share an actual
        // geometry-based near-contact test. Neither applies in free flight,
        // on a near-vertical obstruction, or during a fast normal impact.
        const bool bRestCandidate =
            Cast<UStaticMeshComponent>(Component) != nullptr &&
            !Component->ComponentHasTag(NoRestTag) &&
            Sample.bHasValidDirection && !Acceleration.IsNearlyZero() &&
            (Settings->bStabilizeRestingRigidBodies || Settings->bDampSupportedRigidBodyMotion);
        const FVector LinearVelocity = Component->GetPhysicsLinearVelocity();
        const FVector AngularVelocity = Component->GetPhysicsAngularVelocityInRadians();
        const bool bGentle = bRestCandidate && OrbitPhysicsRestMath::IsGentleContact(
            LinearVelocity, Sample.Down, Settings->RestMaxNormalContactSpeed);
        const FRestSupportProbe Probe = bGentle
            ? ProbeSupportAlongGravity(Component, Sample.Down, Settings->RestSupportProbeDistance)
            : FRestSupportProbe();
        const bool bFreshSupport = bGentle && Probe.bHit;
        bool bGraceSupport = false;
        const FVector BodyPosition = Component->GetCenterOfMass(NAME_None);
        if (bFreshSupport)
        {
            Body.SupportGraceRemaining = FMath::Clamp(Settings->RestSupportContactGraceSeconds, 0.0f, 0.25f);
            Body.LastSupportedLocation = BodyPosition;
            Body.LastSupportedUp = Sample.Up;
            Body.LastSupportNormal = Probe.Normal;
            Body.LastSupportComponent = Probe.SupportComponent;
        }
        else
        {
            Body.SupportGraceRemaining = FMath::Max(0.0f, Body.SupportGraceRemaining - FMath::Max(0.0f, DeltaTime));
            bGraceSupport = bGentle && Body.SupportGraceRemaining > 0.0f &&
                Body.LastSupportComponent.IsValid() &&
                OrbitPhysicsRestMath::MayRetainSupport(BodyPosition, Body.LastSupportedLocation,
                    LinearVelocity, Sample.Down, Body.LastSupportedUp,
                    Settings->RestSupportProbeDistance + 0.5f, 5.0);
            if (!bGraceSupport)
            {
                Body.SupportGraceRemaining = 0.0f;
            }
        }
        const bool bSupported = bFreshSupport || bGraceSupport;

        if (Settings->bDrawPhysicsRestContactDebug && bRestCandidate)
        {
            const FVector Start = BodyPosition;
            const double NormalAlignment = OrbitPhysicsRestMath::SupportUpAlignment(
                Sample.Down, bFreshSupport ? Probe.Normal : Body.LastSupportNormal);
            // RED is reserved for a *verified* surface normal/gravity disagreement.
            // AMBER means the support query failed; BLUE is a short remembered hit.
            const bool bMisaligned = bFreshSupport && NormalAlignment < 0.90;
            const FColor Color = bMisaligned ? FColor::Red :
                (bFreshSupport ? FColor::Cyan :
                 (bGraceSupport ? FColor::Blue : FColor(255, 165, 0)));
            DrawDebugLine(GetWorld(), Start, Start + Sample.Down * 60.0,
                Color, false, 0.0f, 0, 2.0f);
            const FOrbitFieldDefinition* DominantField = ActiveFields.FindByPredicate(
                [&Sample](const FOrbitFieldDefinition& Field) { return Field.SourceId == Sample.DominantSourceId; });
            const FVector GravityUp = -Sample.Down.GetSafeNormal();
            const double TangentSpeed = OrbitPhysicsRestMath::TangentialVelocity(
                LinearVelocity, Sample.Down).Size();
            FString CenterOffset = TEXT("n/a");
            FString RadialAlignment = TEXT("n/a");
            if (DominantField && DominantField->Shape == EOrbitGravityShape::Point)
            {
                // A single inward Point field should remain aligned to the planet
                // radial normal at every latitude, including the equator.
                const FVector RadialUp = (BodyPosition - DominantField->Center).GetSafeNormal();
                RadialAlignment = FString::Printf(TEXT("%.3f"),
                    FVector::DotProduct(GravityUp, RadialUp));
                const UPrimitiveComponent* Support = bFreshSupport ? Probe.SupportComponent.Get() :
                    Body.LastSupportComponent.Get();
                if (Support)
                {
                    // Geometry bounds origin is more useful than an imported mesh's pivot.
                    CenterOffset = FString::Printf(TEXT("%.1f cm"),
                        FVector::Dist(DominantField->Center, Support->Bounds.Origin));
                }
            }
            const FString Diagnostics = FString::Printf(
                TEXT("Orbit rest: %s | source=%d | a=%.0f | tangent=%.1f | spin=%.2f | up-normal=%.2f | radial= %s | nativeG=%s (was %s) | center offset=%s"),
                bMisaligned ? TEXT("NORMAL MISMATCH") :
                    (bFreshSupport ? TEXT("verified") : (bGraceSupport ? TEXT("query grace") : TEXT("no support"))),
                Sample.DominantSourceId, Sample.Magnitude, TangentSpeed, AngularVelocity.Size(),
                NormalAlignment, *RadialAlignment,
                Component->IsGravityEnabled() ? TEXT("ON") : TEXT("OFF"),
                Body.bOriginalGravity ? TEXT("ON") : TEXT("OFF"), *CenterOffset);
            DrawDebugString(GetWorld(), Start + GravityUp * 75.0, Diagnostics,
                nullptr, Color, 0.0f, false, 0.85f);
        }

        if (Settings->bStabilizeRestingRigidBodies && bSupported &&
            LinearVelocity.SizeSquared() <= FMath::Square(Settings->RestLinearSpeed) &&
            AngularVelocity.SizeSquared() <= FMath::Square(Settings->RestAngularSpeed))
        {
            Body.StableSeconds += FMath::Max(0.0f, DeltaTime);
            if (Body.StableSeconds >= Settings->RestTime)
            {
                Component->PutRigidBodyToSleep(NAME_None);
                Body.StableSeconds = 0.0f;
                continue;
            }
        }
        else
        {
            Body.StableSeconds = 0.0f;
        }

        if (bSupported && Settings->bDampSupportedRigidBodyMotion)
        {
            // These are accelerations (not impulses). Do not scale by DeltaTime;
            // Chaos integrates forces at its own simulation step. The tangential
            // force cannot counteract inward gravity or erase vertical jump speed.
            const FVector LateralDeceleration = OrbitPhysicsRestMath::TangentialDampingAcceleration(
                LinearVelocity, Sample.Down, Settings->SupportedTangentialDamping);
            if (!LateralDeceleration.IsNearlyZero())
            {
                Component->AddForce(LateralDeceleration, NAME_None, true);
            }
            const FVector SpinDeceleration = OrbitPhysicsRestMath::AngularDampingAcceleration(
                AngularVelocity, Settings->SupportedAngularDamping);
            if (!SpinDeceleration.IsNearlyZero())
            {
                Component->AddTorqueInRadians(SpinDeceleration, NAME_None, true);
            }
        }

        if (!Acceleration.IsNearlyZero())
        {
            // bAccelChange=true makes acceleration mass-independent. Remaining
            // bounce differences require a solver/substep-level gravity callback.
            Component->AddForce(Acceleration, NAME_None, true);
        }
    }
}

void UOrbitWorldSubsystem::Tick(float DeltaTime)
{
    if (!GetWorld()) { return; }
    UpdateSourceSnapshot(); // Rebuilds per frame: moving sources are supported, but large splines need future caching.

    const UOrbitSettings* Settings = GetDefault<UOrbitSettings>();
    const bool bHasPhysicsField = ActiveFields.ContainsByPredicate([](const FOrbitFieldDefinition& Field) { return (Field.bPhysics && Field.ChannelMask != 0u); });
    if (!Settings->bEnablePhysicsIntegration || !Settings->bAutomaticRigidBodyGravity || !bHasPhysicsField)
    {
        if (!ManagedBodies.IsEmpty()) { RestoreBodyGravity(); }
        bNeedsInitialDiscovery = true;
        return;
    }

    DiscoveryAccumulator += FMath::Max(0.0f, DeltaTime);
    if (bNeedsInitialDiscovery || DiscoveryAccumulator >= Settings->PhysicsDiscoveryInterval)
    {
        DiscoverBodies();
        DiscoveryAccumulator = 0.0f;
        bNeedsInitialDiscovery = false;
    }
    ApplyPhysicsForces(DeltaTime);
}
