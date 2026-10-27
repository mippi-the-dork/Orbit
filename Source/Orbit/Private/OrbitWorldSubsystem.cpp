#include "OrbitWorldSubsystem.h"

#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitGravityReceiverComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
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
    return Component ? SampleGravityForActor(Component->GetOwner(), Component->GetComponentLocation(), Channel)
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

bool UOrbitWorldSubsystem::IsSupportedAlongGravity(const UPrimitiveComponent* Component, const FVector& Down) const
{
    if (!Component || !GetWorld() || Down.IsNearlyZero() || !Component->GetOwner())
    {
        return false;
    }
    // Only a supporting blocking collision counts. Checking the surface normal
    // avoids sleeping against near-vertical walls or ceilings.
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OrbitGravityRest), false);
    QueryParams.AddIgnoredActor(Component->GetOwner());
    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    const FVector Start = Component->Bounds.Origin;
    const double ProbeLength = FMath::Max(10.0, static_cast<double>(Component->Bounds.SphereRadius) + 4.0);
    FHitResult Hit;
    return GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start + Down * ProbeLength, ObjectTypes, QueryParams) &&
        Hit.bBlockingHit && FVector::DotProduct(Hit.ImpactNormal, -Down) > 0.7;
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
            ActiveFields, Component->GetComponentLocation(), Component->GetPhysicsLinearVelocity(),
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

        // The game-thread force bridge can inhibit Chaos sleep. For supported
        // single-body props only, explicitly allow a slow, stable contact to
        // sleep. This never modifies physical-material restitution or damping.
        if (Settings->bStabilizeRestingRigidBodies && Cast<UStaticMeshComponent>(Component) &&
            !Component->ComponentHasTag(NoRestTag) &&
            Sample.bHasValidDirection && !Acceleration.IsNearlyZero() &&
            Component->GetPhysicsLinearVelocity().SizeSquared() <= FMath::Square(Settings->RestLinearSpeed) &&
            Component->GetPhysicsAngularVelocityInRadians().SizeSquared() <= FMath::Square(Settings->RestAngularSpeed) &&
            IsSupportedAlongGravity(Component, Sample.Down))
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
