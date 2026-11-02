// Copyright Mippithedork 2026, Inc. All Rights Reserved.

#include "OrbitWorldSubsystem.h"

#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitGravityReceiverComponent.h"
#include "OrbitPhysicsRestMath.h"
#include "OrbitPhysicsIntegrationMath.h"
#include "OrbitMotionHistoryMath.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "OrbitSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrbitWorld, Log, All);

namespace
{
    // Use authored simple collision, not mesh asset names or bounding spheres.
    // Compound and convex bodies intentionally retain the stricter generic gate.
    bool UsesSimpleSphereRestCollision(const UPrimitiveComponent* Component)
    {
        const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
        const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
        const UBodySetup* Setup = Mesh ? Mesh->GetBodySetup() : nullptr;
        return Setup && Setup->AggGeom.SphereElems.Num() > 0 &&
            Setup->AggGeom.BoxElems.Num() == 0 && Setup->AggGeom.ConvexElems.Num() == 0 &&
            Setup->AggGeom.SphylElems.Num() == 0 && Setup->AggGeom.TaperedCapsuleElems.Num() == 0;
    }
}


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
    bAutoMotionStartedThisWorld = false;
}

void UOrbitWorldSubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
    // Preserve an explicitly requested capture if PIE ends before the user runs Export.
    if (!MotionHistoryRows.IsEmpty() && !bMotionHistoryExported)
    {
        ExportMotionHistory(TEXT("OrbitMotion_AutoPIEStop"));
    }
#endif
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

bool UOrbitWorldSubsystem::GetPhysicsBodyDiagnostics(
    UPrimitiveComponent* Component, FOrbitPhysicsBodyDiagnostics& OutDiagnostics) const
{
    OutDiagnostics = FOrbitPhysicsBodyDiagnostics();
    if (!IsValid(Component)) { return false; }
    for (const FManagedBody& Body : ManagedBodies)
    {
        if (Body.Component.Get() == Component)
        {
            OutDiagnostics = Body.LastDiagnostics;
            return OutDiagnostics.bValid;
        }
    }
    return false;
}

bool UOrbitWorldSubsystem::HasManagedBody(const UPrimitiveComponent* Component) const
{
    for (const FManagedBody& Body : ManagedBodies)
    {
        if (Body.Component.Get() == Component) { return true; }
    }
    return false;
}

void UOrbitWorldSubsystem::HandlePhysicsComponentWake(UPrimitiveComponent* WakingComponent, FName BoneName)
{
    if (!IsValid(WakingComponent)) { return; }
    for (FManagedBody& Body : ManagedBodies)
    {
        if (Body.Component.Get() != WakingComponent) { continue; }
        ++Body.NativeWakeEventCount;
        Body.LastNativeWakePIETime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
        if (GetDefault<UOrbitSettings>()->bLogNativeWakeEvents)
        {
            UE_LOG(LogOrbitWorld, Display, TEXT("Orbit physics wake event: %s bone=%s pie=%.3f (notification, cause unknown)"),
                *WakingComponent->GetPathName(), *BoneName.ToString(), Body.LastNativeWakePIETime);
        }
        break;
    }
}

void UOrbitWorldSubsystem::HandlePhysicsComponentSleep(UPrimitiveComponent* SleepingComponent, FName BoneName)
{
    if (!IsValid(SleepingComponent)) { return; }
    for (FManagedBody& Body : ManagedBodies)
    {
        if (Body.Component.Get() != SleepingComponent) { continue; }
        ++Body.NativeSleepEventCount;
        Body.LastNativeSleepPIETime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
        break;
    }
}

void UOrbitWorldSubsystem::UnbindBodyEvents(FManagedBody& Body)
{
    if (UPrimitiveComponent* Component = Body.Component.Get())
    {
        Component->OnComponentWake.RemoveDynamic(this, &UOrbitWorldSubsystem::HandlePhysicsComponentWake);
        Component->OnComponentSleep.RemoveDynamic(this, &UOrbitWorldSubsystem::HandlePhysicsComponentSleep);
    }
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
        NewBody.OriginalLinearDamping = Component->GetLinearDamping();
        NewBody.OriginalAngularDamping = Component->GetAngularDamping();
        ManagedBodies.Add(NewBody);
        // Do not alter bGenerateWakeEvents: the user's physics settings own it.
        Component->OnComponentWake.AddUniqueDynamic(this, &UOrbitWorldSubsystem::HandlePhysicsComponentWake);
        Component->OnComponentSleep.AddUniqueDynamic(this, &UOrbitWorldSubsystem::HandlePhysicsComponentSleep);
        Component->SetEnableGravity(false);
    }
}

void UOrbitWorldSubsystem::RestoreBodyDamping(FManagedBody& Body)
{
    UPrimitiveComponent* Component = Body.Component.Get();
    if (!Body.bNativeContactDampingApplied || !IsValid(Component) || !Component->IsRegistered())
    {
        Body.bNativeContactDampingApplied = false;
        return;
    }
    // Restore the user's component configuration when contact ends or Orbit releases it.
    Component->SetLinearDamping(Body.OriginalLinearDamping);
    Component->SetAngularDamping(Body.OriginalAngularDamping);
    Body.bNativeContactDampingApplied = false;
}

void UOrbitWorldSubsystem::RestoreBodyGravity()
{
    for (FManagedBody& Body : ManagedBodies)
    {
        UnbindBodyEvents(Body);
        UPrimitiveComponent* Component = Body.Component.Get();
        if (IsValid(Component) && Component->IsRegistered())
        {
            RestoreBodyDamping(Body);
            Component->SetEnableGravity(Body.bOriginalGravity);
        }
    }
    ManagedBodies.Empty();
}

UOrbitWorldSubsystem::FRestSupportProbe UOrbitWorldSubsystem::ProbeSupportAlongGravity(
    UPrimitiveComponent* Component, const FVector& Down, float ProbeDistance, UPrimitiveComponent* CachedSideBlocker) const
{
    FRestSupportProbe Probe;
    if (!Component || !GetWorld() || Down.IsNearlyZero() || !Component->GetOwner())
    {
        return Probe;
    }

    const FVector UnitDown = Down.GetSafeNormal();
    const double Distance = FMath::Max(0.1, static_cast<double>(ProbeDistance));
    const FVector Origin = Component->GetComponentLocation();
    const FVector Start = Origin - UnitDown * Distance;
    const FVector End = Origin + UnitDown * Distance;
    FComponentQueryParams Params;
    Params.AddIgnoredActor(Component->GetOwner());
    Params.bTraceComplex = false;
    if (IsValid(CachedSideBlocker) && CachedSideBlocker != Component)
    {
        Params.AddIgnoredComponent(CachedSideBlocker);
        Probe.bCachedSideObstructionUsed = true;
    }
    const UOrbitSettings* Settings = GetDefault<UOrbitSettings>();
    double BestAlignment = 0.70;
    bool bBestCandidateStable = false;

    // A multi-sweep may stop at the first blocking collision. If that blocker has
    // a sideways normal, rerun at most twice ignoring ONLY the rejected component.
    // Real Chaos collision is not changed, and the same short query distance applies.
    const int32 RetryLimit = Probe.bCachedSideObstructionUsed ? 0 : FMath::Clamp(Settings->RestSupportObstructionRetries, 0, 2);
    for (int32 Attempt = 0; Attempt <= RetryLimit; ++Attempt)
    {
        if (Attempt > 0) { Probe.bSecondarySweepAttempted = true; }
        TArray<FHitResult> Hits;
        GetWorld()->ComponentSweepMulti(Hits, Component, Start, End, Component->GetComponentQuat(), Params);
        TWeakObjectPtr<UPrimitiveComponent> BestRejectedThisPass;
        double RejectedAlignmentThisPass = -2.0;
        for (const FHitResult& Hit : Hits)
        {
            UPrimitiveComponent* Candidate = Hit.GetComponent();
            if (!Hit.bBlockingHit || !IsValid(Candidate) || Candidate == Component) { continue; }
            const double Alignment = OrbitPhysicsRestMath::SupportUpAlignment(Down, Hit.ImpactNormal);
            ++Probe.BlockingCandidates;
            if (Alignment < 0.70)
            {
                ++Probe.RejectedNormals;
                if (Alignment > RejectedAlignmentThisPass)
                {
                    RejectedAlignmentThisPass = Alignment;
                    BestRejectedThisPass = Candidate;
                }
                if (Alignment > Probe.BestRejectedAlignment)
                {
                    Probe.BestRejectedAlignment = Alignment;
                    Probe.BestRejectedNormal = Hit.ImpactNormal.GetSafeNormal();
                    Probe.BestRejectedImpact = Hit.ImpactPoint;
                    Probe.bBestRejectedStartedPenetrating = Hit.bStartPenetrating;
                    Probe.BestRejectedComponent = Candidate;
                }
                continue;
            }
            const bool bDynamic = Candidate->IsSimulatingPhysics();
            const bool bCandidateStable = OrbitPhysicsRestMath::IsStableDynamicSupport(
                bDynamic,
                bDynamic ? Candidate->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint) : FVector::ZeroVector,
                bDynamic ? Candidate->GetPhysicsAngularVelocityInRadians() : FVector::ZeroVector,
                Settings->RestMaxMovingSupportSpeed, Settings->RestMaxMovingSupportAngularSpeed);
            if (!OrbitPhysicsRestMath::PreferSupportCandidate(Alignment, bCandidateStable,
                BestAlignment, bBestCandidateStable, Probe.bHit)) { continue; }
            bBestCandidateStable = bCandidateStable;
            BestAlignment = Alignment;
            Probe.bHit = true;
            Probe.bStartedPenetrating = Hit.bStartPenetrating;
            Probe.Normal = Hit.ImpactNormal.GetSafeNormal();
            Probe.ImpactPoint = Hit.ImpactPoint;
            Probe.SupportComponent = Candidate;
        }
        if (Probe.bHit)
        {
            if (Attempt > 0) { Probe.bSecondarySweepFoundSupport = true; }
            break;
        }
        if (!OrbitPhysicsRestMath::ShouldRetrySupportProbe(Probe.bHit,
            BestRejectedThisPass.IsValid(), Attempt, RetryLimit)) { break; }
        if (Attempt == 0) { Probe.RecoveredSideObstruction = BestRejectedThisPass; }
        Params.AddIgnoredComponent(BestRejectedThisPass.Get());
    }
    if (Probe.bCachedSideObstructionUsed && !Probe.bHit)
    {
        // A stale cache must never hide the true support. One unfiltered bounded
        // retry falls back to the original path, then the cache is invalidated.
        FRestSupportProbe Unfiltered = ProbeSupportAlongGravity(Component, Down, ProbeDistance, nullptr);
        Unfiltered.bCachedSideObstructionUsed = true;
        Unfiltered.bCachedSideObstructionFallback = true;
        return Unfiltered;
    }
    return Probe;
}

bool UOrbitWorldSubsystem::IsOrbitManagedPhysicsBody(const UPrimitiveComponent* Component) const
{
    return IsValid(Component) && Component->GetWorld() == GetWorld() &&
        HasManagedBody(Component);
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
            UnbindBodyEvents(Body);
            ManagedBodies.RemoveAtSwap(Index);
            continue;
        }
        if (!Component->IsSimulatingPhysics() || Component->ComponentHasTag(IgnoreTag))
        {
            RestoreBodyDamping(Body);
            Component->SetEnableGravity(Body.bOriginalGravity);
            UnbindBodyEvents(Body);
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
            RestoreBodyDamping(Body);
            Component->SetEnableGravity(Body.bOriginalGravity);
            UnbindBodyEvents(Body);
            ManagedBodies.RemoveAtSwap(Index);
            continue;
        }
        const FVector Acceleration = Sample.Acceleration;
        // A/B mode is restricted to original native-gravity bodies in a FLAT,
        // world-down field with the same magnitude. Orbit remains authoritative
        // for everything else; the two paths must never apply gravity together.
        const bool bNativeComparison = OrbitPhysicsIntegrationMath::CanCompareWithNativeWorldGravity(
            Settings->bCompareNativeWorldGravityWhenEquivalent, Body.bOriginalGravity,
            Sample.bHasInfluence, Acceleration, GetWorld()->GetGravityZ());
        if (Component->IsGravityEnabled() != bNativeComparison)
        {
            Component->SetEnableGravity(bNativeComparison);
        }
        const double PieNow = GetWorld()->GetTimeSeconds();
        Body.bLastWokeAfterManualSleep = false;
        Body.bLastFieldChangeWake = false;
        Body.bLastManualSleepRequested = false;
        if (!Component->IsAnyRigidBodyAwake())
        {
            // Never re-awaken a resting body with the same gravity every tick.
            // Wake if the local field changed substantially while it slept.
            if (FVector::DistSquared(Acceleration, Body.LastAcceleration) > FMath::Square(20.0))
            {
                Component->WakeAllRigidBodies();
                // A source change is a new rest episode, not a failed sleep request.
                Body.ManualSleepFailureCount = 0;
                Body.bManualSleepSuppressed = false;
                Body.UnstableRestSeconds = 0.0f;
                Body.bHasRestAnchor = false;
                Body.SuppressedStableSeconds = 0.0f;
                Body.bLastFieldChangeWake = true;
                Body.bAwaitingManualSleepWake = false;
            }
            else
            {
                // Refresh read-only snapshots while sleeping; old values reflected the last
                // awake frame even when Chaos had already zeroed the rigid body's velocities.
                Body.bLastProbeAttempted = false;
                Body.bLastFreshSupport = false;
                Body.bLastGraceSupport = false;
                Body.bLastRestCandidate = false;
                Body.bLastGentleContact = false;
                Body.LastProbe = FRestSupportProbe();
                Body.LastNormalSpeedCmS = 0.0;
                Body.LastProbeGraceSeconds = Body.SupportGraceRemaining;
                Body.LastDiagnostics.bValid = true;
                Body.LastDiagnostics.bBodyAwake = false;
                Body.LastDiagnostics.bNativeGravityEnabled = Component->IsGravityEnabled();
                Body.LastDiagnostics.bOriginalNativeGravityEnabled = Body.bOriginalGravity;
                Body.LastDiagnostics.bUsingNativeContactDamping = Body.bNativeContactDampingApplied;
                Body.LastDiagnostics.DominantSourceId = Sample.DominantSourceId;
                Body.LastDiagnostics.GravityAcceleration = Acceleration;
                Body.LastDiagnostics.TangentialSpeed = OrbitMotionHistoryMath::CurrentTangentialSpeed(
                    Component->GetPhysicsLinearVelocity(), Acceleration);
                Body.LastDiagnostics.AngularSpeed = Component->GetPhysicsAngularVelocityInRadians().Size();
                Body.LastDiagnostics.SupportState = TEXT("Sleeping (previous support sample)");
                if (UOrbitGravityReceiverComponent* Receiver = (IsValid(Component->GetOwner()) ? Component->GetOwner()->FindComponentByClass<UOrbitGravityReceiverComponent>() : nullptr))
                {
                    Receiver->LastPhysicsBodyDiagnostics = Body.LastDiagnostics;
                }
                CaptureMotionHistory(Component, Body);
                continue;
            }
        }
        if (Body.bAwaitingManualSleepWake && Component->IsAnyRigidBodyAwake())
        {
            Body.bLastWokeAfterManualSleep = OrbitPhysicsRestMath::IsImmediateManualSleepWake(
                PieNow, Body.LastManualSleepPIETime);
            if (Body.bLastWokeAfterManualSleep && !Body.bLastFieldChangeWake)
            {
                Body.ManualSleepRetryAfter = PieNow + FMath::Clamp(
                    Settings->RestWakeRetryCooldown, 0.0f, 10.0f);
                Body.ManualSleepFailureCount = OrbitPhysicsRestMath::NextManualSleepFailureCount(
                    Body.ManualSleepFailureCount, true, Settings->RestMaxManualSleepFailures);
                Body.bManualSleepSuppressed = OrbitPhysicsRestMath::SuppressManualSleep(
                    Body.ManualSleepFailureCount, Settings->RestMaxManualSleepFailures);
                if (Body.bManualSleepSuppressed)
                {
                    Body.SuppressedStableSeconds = 0.0f;
                    Body.ManualSleepRetryAfter = FMath::Max(Body.ManualSleepRetryAfter,
                        PieNow + FMath::Max(0.0f, Settings->RestSuppressedStableRetrySeconds));
                }
            }
            Body.bAwaitingManualSleepWake = false;
        }
        // A materially changed field starts a new potential rest episode even
        // if Chaos had the body awake already; don't report an explicit wake
        // unless Orbit actually called WakeAllRigidBodies above.
        if (Body.bManualSleepSuppressed &&
            FVector::DistSquared(Acceleration, Body.LastAcceleration) > FMath::Square(20.0))
        {
            Body.ManualSleepFailureCount = 0;
            Body.bManualSleepSuppressed = false;
            Body.UnstableRestSeconds = 0.0f;
            Body.bHasRestAnchor = false;
            Body.StableSeconds = 0.0f;
            Body.SuppressedStableSeconds = 0.0f;
            Body.ManualSleepRetryAfter = PieNow;
        }
        Body.LastManualSleepCooldownLeft = static_cast<float>(FMath::Max(0.0,
            Body.ManualSleepRetryAfter - PieNow));
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
        UPrimitiveComponent* CachedBlocker = Body.CachedSideObstruction.Get();
        const bool bUseCachedSideBlocker = bGentle &&
            OrbitPhysicsRestMath::MayUseCachedSideObstruction(
                IsValid(CachedBlocker), Body.CachedRecoveredSupport.IsValid(), PieNow,
                Body.CachedSideObstructionExpiresAt, Component->GetComponentLocation(),
                Body.CachedBodyPosition,
                IsValid(CachedBlocker) ? CachedBlocker->GetComponentLocation() : FVector::ZeroVector,
                Body.CachedBlockerPosition,
                IsValid(CachedBlocker) ? CachedBlocker->GetComponentQuat() : FQuat::Identity,
                Body.CachedBlockerRotation, Sample.Down, Body.CachedDown);
        const FRestSupportProbe Probe = bGentle
            ? ProbeSupportAlongGravity(Component, Sample.Down, Settings->RestSupportProbeDistance,
                bUseCachedSideBlocker ? CachedBlocker : nullptr)
            : FRestSupportProbe();
        if (Probe.bCachedSideObstructionFallback || !Probe.bHit ||
            (bUseCachedSideBlocker && Probe.SupportComponent != Body.CachedRecoveredSupport))
        {
            Body.CachedSideObstruction.Reset();
            Body.CachedRecoveredSupport.Reset();
            Body.CachedSideObstructionExpiresAt = -1.0;
        }
        // Cache only a confirmed *unfiltered* blocked query that found actual
        // supporting geometry after excluding a sideways obstruction. Expiry is
        // never extended by using the cache, so normal sweeps regularly resume.
        if (Probe.bSecondarySweepFoundSupport && Probe.RecoveredSideObstruction.IsValid() &&
            Probe.SupportComponent.IsValid() && Settings->RestObstructionCacheSeconds > 0.0f)
        {
            Body.CachedSideObstruction = Probe.RecoveredSideObstruction;
            Body.CachedRecoveredSupport = Probe.SupportComponent;
            Body.CachedBodyPosition = Component->GetComponentLocation();
            Body.CachedBlockerPosition = Probe.RecoveredSideObstruction->GetComponentLocation();
            Body.CachedBlockerRotation = Probe.RecoveredSideObstruction->GetComponentQuat();
            Body.CachedDown = Sample.Down;
            Body.CachedSideObstructionExpiresAt = PieNow + FMath::Clamp(Settings->RestObstructionCacheSeconds, 0.0f, 0.25f);
        }
        const bool bFreshSupport = bGentle && Probe.bHit;
        // Store the *existing* sweep's evidence so recording it performs no extra query.
        Body.LastProbe = Probe;
        Body.bLastProbeAttempted = bGentle;
        Body.bLastRestCandidate = bRestCandidate;
        Body.bLastGentleContact = bGentle;
        Body.bLastFreshSupport = bFreshSupport;
        Body.LastNormalSpeedCmS = FVector::DotProduct(LinearVelocity, Sample.Down.GetSafeNormal());
        bool bGraceSupport = false;
        const FVector BodyPosition = Component->GetCenterOfMass(NAME_None);
        if (bFreshSupport)
        {
            Body.SupportGraceRemaining = FMath::Clamp(Settings->RestSupportContactGraceSeconds, 0.0f, 0.25f);
            Body.LastSupportedLocation = BodyPosition;
            Body.LastSupportedUp = Sample.Up;
            Body.LastSupportNormal = Probe.Normal;
            Body.LastSupportImpactPoint = Probe.ImpactPoint;
            Body.bLastSupportStartedPenetrating = Probe.bStartedPenetrating;
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
        Body.bLastGraceSupport = bGraceSupport;
        Body.LastProbeGraceSeconds = Body.SupportGraceRemaining;

        const FOrbitFieldDefinition* DominantField = ActiveFields.FindByPredicate(
            [&Sample](const FOrbitFieldDefinition& Field) { return Field.SourceId == Sample.DominantSourceId; });
        const FVector GravityUp = -Sample.Down.GetSafeNormal();
        UPrimitiveComponent* SupportComponent = bFreshSupport ? Probe.SupportComponent.Get() :
            (bGraceSupport ? Body.LastSupportComponent.Get() : nullptr);
        const FVector SupportNormal = bFreshSupport ? Probe.Normal : Body.LastSupportNormal;
        const double NormalAlignment = bSupported ? OrbitPhysicsRestMath::SupportUpAlignment(
            Sample.Down, SupportNormal) : 0.0;
        // A geometry hit is still support for collision, but a rapidly moving simulating
        // neighbor must not be treated as stationary ground for artificial rest/damping.
        const bool bDynamicSupport = IsValid(SupportComponent) && SupportComponent->IsSimulatingPhysics();
        const FVector SupportVelocityAtContact = bDynamicSupport ?
            SupportComponent->GetPhysicsLinearVelocityAtPoint(
                bFreshSupport ? Probe.ImpactPoint : Body.LastSupportImpactPoint) : FVector::ZeroVector;
        const FVector SupportAngularVelocity = bDynamicSupport ?
            SupportComponent->GetPhysicsAngularVelocityInRadians() : FVector::ZeroVector;
        const bool bStableSupport = bSupported && OrbitPhysicsRestMath::IsStableDynamicSupport(
            bDynamicSupport, SupportVelocityAtContact, SupportAngularVelocity,
            Settings->RestMaxMovingSupportSpeed, Settings->RestMaxMovingSupportAngularSpeed);
        Body.bLastDynamicSupport = bDynamicSupport;
        Body.bLastStableSupportForRest = bStableSupport;
        Body.LastSupportContactVelocity = SupportVelocityAtContact.Size();
        Body.LastSupportAngularVelocity = SupportAngularVelocity.Size();
        Body.LastRelativeSupportVelocity = (LinearVelocity - SupportVelocityAtContact).Size();
        const bool bMisaligned = bFreshSupport && NormalAlignment < 0.90;
        const FColor SupportColor = bMisaligned ? FColor::Red :
            (bFreshSupport ? FColor::Cyan : (bGraceSupport ? FColor::Blue : FColor(255, 165, 0)));

        FOrbitPhysicsBodyDiagnostics& Diagnostics = Body.LastDiagnostics;
        Diagnostics.bValid = true;
        Diagnostics.SupportState = !bRestCandidate ? TEXT("Ineligible (check field and Orbit.NoRest)") :
            (bMisaligned ? TEXT("Normal mismatch") : (bFreshSupport ? TEXT("Verified") :
             (bGraceSupport ? TEXT("Query grace") : TEXT("Unsupported"))));
        Diagnostics.bBodyAwake = true;
        Diagnostics.bNativeGravityEnabled = Component->IsGravityEnabled();
        Diagnostics.bOriginalNativeGravityEnabled = Body.bOriginalGravity;
        Diagnostics.DominantSourceId = Sample.DominantSourceId;
        Diagnostics.GravityAcceleration = Acceleration;
        Diagnostics.TangentialSpeed = OrbitPhysicsRestMath::TangentialVelocity(LinearVelocity, Sample.Down).Size();
        Diagnostics.AngularSpeed = AngularVelocity.Size();
        Diagnostics.bHasSupportNormal = bSupported;
        Diagnostics.SupportUpNormalAlignment = NormalAlignment;
        Diagnostics.bHasPointRadialAlignment = DominantField && DominantField->Shape == EOrbitGravityShape::Point;
        Diagnostics.PointRadialAlignment = Diagnostics.bHasPointRadialAlignment ?
            FVector::DotProduct(GravityUp, (BodyPosition - DominantField->Center).GetSafeNormal()) : 0.0;
        Diagnostics.bHasSourceToSupportBoundsCenter = Diagnostics.bHasPointRadialAlignment && SupportComponent;
        Diagnostics.SourceToSupportBoundsCenterCm = Diagnostics.bHasSourceToSupportBoundsCenter ?
            FVector::Dist(DominantField->Center, SupportComponent->Bounds.Origin) : 0.0;

        if (Settings->bDrawPhysicsRestContactDebug && bRestCandidate)
        {
            DrawDebugLine(GetWorld(), BodyPosition, BodyPosition + Sample.Down * 60.0,
                SupportColor, false, 0.12f, 0, 2.0f);
            // Useful where the viewport draws labels, but not the sole diagnostic channel.
            const FString Label = FString::Printf(
                TEXT("Orbit %s | radial=%s%.3f | up-normal=%s%.3f | center=%s%.1fcm | nativeG=%s"),
                *Diagnostics.SupportState,
                Diagnostics.bHasPointRadialAlignment ? TEXT("") : TEXT("n/a "), Diagnostics.PointRadialAlignment,
                Diagnostics.bHasSupportNormal ? TEXT("") : TEXT("n/a "), Diagnostics.SupportUpNormalAlignment,
                Diagnostics.bHasSourceToSupportBoundsCenter ? TEXT("") : TEXT("n/a "),
                Diagnostics.SourceToSupportBoundsCenterCm, Diagnostics.bNativeGravityEnabled ? TEXT("ON") : TEXT("OFF"));
            DrawDebugString(GetWorld(), BodyPosition + GravityUp * 85.0,
                Label, nullptr, SupportColor, 0.15f, true, 1.0f);
        }

        // A full rest interval requires fresh, consistent contact with the same support.
        // Query grace is sufficient for damping but not for forcibly sleeping a body.
        const bool bSameSupport = bFreshSupport && IsValid(SupportComponent) &&
            Body.PreviousStableRestSupport.Get() == SupportComponent;
        Body.LastRestPositionBudget = OrbitPhysicsRestMath::RestDisplacementBudget(
            UsesSimpleSphereRestCollision(Component), Settings->RestMaxStationaryDisplacement,
            Settings->RestSphereStationaryDisplacement);
        if (!bSameSupport || !bStableSupport || !Body.bHasRestAnchor)
        {
            Body.RestAnchorPosition = BodyPosition;
            Body.bHasRestAnchor = bFreshSupport && bStableSupport;
        }
        Body.RestAnchorDisplacement = Body.bHasRestAnchor ?
            static_cast<float>(FVector::Dist(BodyPosition, Body.RestAnchorPosition)) : 0.0f;
        // Re-anchor after genuine movement: accumulated drift cannot pass as a
        // stationary contact merely because instantaneous velocity is small.
        if (Body.bHasRestAnchor &&
            Body.RestAnchorDisplacement > Body.LastRestPositionBudget)
        {
            Body.RestAnchorPosition = BodyPosition;
            Body.StableSeconds = 0.0f;
        }
        const bool bSlowBody = Body.bHasRestAnchor &&
            OrbitPhysicsRestMath::IsStationaryRestCandidate(
                LinearVelocity, AngularVelocity, Sample.Down, BodyPosition, Body.RestAnchorPosition,
                Settings->RestLinearSpeed, Settings->RestAngularSpeed,
                Settings->RestMaxStationaryNormalSpeed, Body.LastRestPositionBudget);
        // Rearm after a genuine departure or meaningful movement, not a one-frame
        // support-query dropout. Native Chaos can sleep freely while suppressed.
        const bool bMeaningfullyUnstable = !bFreshSupport || !bStableSupport ||
            LinearVelocity.SizeSquared() > FMath::Square(Settings->RestRearmLinearSpeed) ||
            AngularVelocity.SizeSquared() > FMath::Square(0.25);
        Body.UnstableRestSeconds = bMeaningfullyUnstable ?
            Body.UnstableRestSeconds + FMath::Max(0.0f, DeltaTime) : 0.0f;
        if (OrbitPhysicsRestMath::ShouldRearmManualSleep(Body.bManualSleepSuppressed,
            Body.UnstableRestSeconds, Settings->RestRearmUnstableSeconds))
        {
            Body.ManualSleepFailureCount = 0;
            Body.bManualSleepSuppressed = false;
            Body.UnstableRestSeconds = 0.0f;
            Body.ManualSleepRetryAfter = PieNow;
            Body.StableSeconds = 0.0f;
            Body.SuppressedStableSeconds = 0.0f;
            Body.bHasRestAnchor = false;
        }
        // Once suppressed, earn ONE recovery attempt by remaining in genuinely
        // supported low-motion contact for a sustained interval. Each failed
        // recovery returns to suppression immediately instead of retrying twice.
        const bool bStableWhileSuppressed = bFreshSupport && bStableSupport && bSameSupport && bSlowBody;
        Body.SuppressedStableSeconds = Body.bManualSleepSuppressed && bStableWhileSuppressed ?
            Body.SuppressedStableSeconds + FMath::Max(0.0f, DeltaTime) : 0.0f;
        if (OrbitPhysicsRestMath::ShouldRecoverSuppressedSleep(
            Body.bManualSleepSuppressed, bStableWhileSuppressed, Body.SuppressedStableSeconds,
            Settings->RestSuppressedStableRetrySeconds, PieNow, Body.ManualSleepRetryAfter))
        {
            Body.bManualSleepSuppressed = false;
            Body.ManualSleepFailureCount = OrbitPhysicsRestMath::ProbationaryFailureCount(Settings->RestMaxManualSleepFailures);
            Body.SuppressedStableSeconds = 0.0f;
            Body.StableSeconds = 0.0f;
            ++Body.SuppressedRecoveryCount;
        }
        const bool bRestEligible = !Body.bManualSleepSuppressed &&
            OrbitPhysicsRestMath::MayRequestManualSleep(
                bFreshSupport, bStableSupport, bSameSupport, bSlowBody,
                PieNow, Body.ManualSleepRetryAfter);
        if (!bFreshSupport || !bStableSupport)
        {
            Body.PreviousStableRestSupport.Reset();
            Body.bHasRestAnchor = false;
        }
        else
        {
            Body.PreviousStableRestSupport = SupportComponent;
        }
        if (Settings->bStabilizeRestingRigidBodies && bRestEligible)
        {
            Body.StableSeconds += FMath::Max(0.0f, DeltaTime);
            if (Body.StableSeconds >= Settings->RestTime)
            {
                Body.bLastManualSleepRequested = true;
                Component->PutRigidBodyToSleep(NAME_None);
                Body.StableSeconds = 0.0f;
                const bool bActuallySleeping = !Component->IsAnyRigidBodyAwake();
                if (bActuallySleeping)
                {
                    Body.bAwaitingManualSleepWake = true;
                    Body.LastManualSleepPIETime = PieNow;
                }
                else
                {
                    Body.ManualSleepRetryAfter = PieNow + FMath::Clamp(
                        Settings->RestWakeRetryCooldown, 0.0f, 10.0f);
                    Body.ManualSleepFailureCount = OrbitPhysicsRestMath::NextManualSleepFailureCount(
                        Body.ManualSleepFailureCount, true, Settings->RestMaxManualSleepFailures);
                    Body.bManualSleepSuppressed = OrbitPhysicsRestMath::SuppressManualSleep(
                        Body.ManualSleepFailureCount, Settings->RestMaxManualSleepFailures);
                    if (Body.bManualSleepSuppressed)
                    {
                        Body.SuppressedStableSeconds = 0.0f;
                        Body.ManualSleepRetryAfter = FMath::Max(Body.ManualSleepRetryAfter,
                            PieNow + FMath::Max(0.0f, Settings->RestSuppressedStableRetrySeconds));
                    }
                }
                Body.LastManualSleepCooldownLeft = static_cast<float>(FMath::Max(0.0,
                    Body.ManualSleepRetryAfter - PieNow));
                Diagnostics.bBodyAwake = !bActuallySleeping;
                Diagnostics.SupportState = bActuallySleeping ? TEXT("Sleeping after stable support") :
                    TEXT("Manual sleep request did not persist");
                Diagnostics.bUsingNativeContactDamping = Body.bNativeContactDampingApplied;
                Diagnostics.TangentialSpeed = OrbitMotionHistoryMath::CurrentTangentialSpeed(
                    Component->GetPhysicsLinearVelocity(), Acceleration);
                Diagnostics.AngularSpeed = Component->GetPhysicsAngularVelocityInRadians().Size();
                if (UOrbitGravityReceiverComponent* Receiver = (IsValid(Component->GetOwner()) ? Component->GetOwner()->FindComponentByClass<UOrbitGravityReceiverComponent>() : nullptr))
                {
                    Receiver->LastPhysicsBodyDiagnostics = Diagnostics;
                }
                CaptureMotionHistory(Component, Body);
                if (bActuallySleeping) { continue; }
            }
        }
        else
        {
            Body.StableSeconds = 0.0f;
        }

        if (Body.bManualSleepSuppressed && bFreshSupport)
        {
            Diagnostics.SupportState = TEXT("Verified (manual sleep suppressed after wake failures)");
        }
        if (bSupported && bStableSupport && Settings->bDampSupportedRigidBodyMotion)
        {
            // The former game-thread AddForce/AddTorque rest corrections switched
            // on/off with support queries and could fight Chaos's contact solver.
            // Set native damping only when support is established. Chaos integrates
            // it with its physics updates; the original values are restored on exit.
            // NOTE: native linear damping affects all velocity components, not only
            // tangential motion. It is never enabled during unsupported flight.
            const float DesiredLinear = OrbitPhysicsRestMath::NativeSupportedDamping(Body.OriginalLinearDamping, Settings->SupportedTangentialDamping);
            const float DesiredAngular = OrbitPhysicsRestMath::NativeSupportedDamping(Body.OriginalAngularDamping, Settings->SupportedAngularDamping);
            if (!Body.bNativeContactDampingApplied ||
                !FMath::IsNearlyEqual(Component->GetLinearDamping(), DesiredLinear, 0.01f) ||
                !FMath::IsNearlyEqual(Component->GetAngularDamping(), DesiredAngular, 0.01f))
            {
                Component->SetLinearDamping(DesiredLinear);
                Component->SetAngularDamping(DesiredAngular);
            }
            Body.bNativeContactDampingApplied = true;
        }
        else
        {
            RestoreBodyDamping(Body);
        }
        Diagnostics.bUsingNativeContactDamping = Body.bNativeContactDampingApplied;
        if (UOrbitGravityReceiverComponent* Receiver = (IsValid(Component->GetOwner()) ? Component->GetOwner()->FindComponentByClass<UOrbitGravityReceiverComponent>() : nullptr))
        {
            Receiver->LastPhysicsBodyDiagnostics = Diagnostics;
        }

        CaptureMotionHistory(Component, Body);

        if (!bNativeComparison && !Acceleration.IsNearlyZero())
        {
            // UE's regular AddForce maintains a frame's force over internal
            // physics substeps. It is deliberately NOT also added in a custom
            // substep callback (that would double-apply gravity). The native
            // comparison path above is mutually exclusive, and lets us isolate
            // Character Movement load vs custom gravity in controlled tests.
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
        if (bMotionHistoryRecording) { FinishMotionHistory(TEXT("Orbit physics integration disabled or no eligible sources")); }
        return;
    }

    DiscoveryAccumulator += FMath::Max(0.0f, DeltaTime);
    if (bNeedsInitialDiscovery || DiscoveryAccumulator >= Settings->PhysicsDiscoveryInterval)
    {
        DiscoverBodies();
        DiscoveryAccumulator = 0.0f;
        bNeedsInitialDiscovery = false;
    }
#if !UE_BUILD_SHIPPING
    // Editor console variables can be set before entering PIE. Arm once per world,
    // after first body discovery, so the initial impacts are not missed.
    if (!bAutoMotionStartedThisWorld && !bMotionHistoryRecording && !ManagedBodies.IsEmpty())
    {
        IConsoleVariable* AutoAll = IConsoleManager::Get().FindConsoleVariable(TEXT("Orbit.Motion.AutoStartAll"));
        if (AutoAll && AutoAll->GetInt() != 0)
        {
            IConsoleVariable* DurationVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Orbit.Motion.AutoDuration"));
            IConsoleVariable* RateVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Orbit.Motion.AutoRate"));
            bAutoMotionStartedThisWorld = StartAllMotionHistory(
                DurationVar ? DurationVar->GetFloat() : 20.0f,
                RateVar ? RateVar->GetInt() : 60);
        }
    }
#endif
    ApplyPhysicsForces(DeltaTime);
    if (bMotionHistoryRecording)
    {
        // A captured roster is fixed for this session. Removal of an individual body in
        // all-body mode must not end recording of the other bodies.
        if (!bMotionHistoryAllBodies &&
            (MotionHistoryBodies.IsEmpty() || !MotionHistoryBodies[0].Component.IsValid() ||
             !HasManagedBody(MotionHistoryBodies[0].Component.Get())))
        {
            FinishMotionHistory(TEXT("Target no longer managed by Orbit"));
        }
        else if (GetWorld()->GetTimeSeconds() - MotionHistoryStartTime >= MotionHistoryDuration)
        {
            FinishMotionHistory(TEXT("Duration reached"));
        }
        else if (bMotionHistoryAllBodies)
        {
            // Iterate only managed bodies; no O(captured x managed) lookups per tick.
            bool bAnyStillManaged = false;
            for (const FManagedBody& Body : ManagedBodies)
            {
                if (const int32* CaptureIndex = MotionHistoryBodyLookup.Find(Body.Component.Get()))
                {
                    if (MotionHistoryBodies.IsValidIndex(*CaptureIndex) &&
                        MotionHistoryBodies[*CaptureIndex].Component.IsValid())
                    {
                        bAnyStillManaged = true;
                        break;
                    }
                }
            }
            if (!bAnyStillManaged)
            {
                FinishMotionHistory(TEXT("No captured bodies remain managed by Orbit"));
            }
        }
    }
}
