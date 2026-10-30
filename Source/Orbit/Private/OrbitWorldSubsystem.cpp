#include "OrbitWorldSubsystem.h"

#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitGravityReceiverComponent.h"
#include "OrbitPhysicsRestMath.h"
#include "OrbitMotionHistoryMath.h"
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
        ++Probe.BlockingCandidates;
        if (Alignment < 0.70)
        {
            ++Probe.RejectedNormals;
            if (Alignment > Probe.BestRejectedAlignment)
            {
                Probe.BestRejectedAlignment = Alignment;
                Probe.BestRejectedNormal = Hit.ImpactNormal.GetSafeNormal();
                Probe.BestRejectedImpact = Hit.ImpactPoint;
                Probe.bBestRejectedStartedPenetrating = Hit.bStartPenetrating;
                Probe.BestRejectedComponent = Hit.GetComponent();
            }
        }
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
            RestoreBodyDamping(Body);
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
            RestoreBodyDamping(Body);
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
        const UPrimitiveComponent* SupportComponent = bFreshSupport ? Probe.SupportComponent.Get() :
            (bGraceSupport ? Body.LastSupportComponent.Get() : nullptr);
        const FVector SupportNormal = bFreshSupport ? Probe.Normal : Body.LastSupportNormal;
        const double NormalAlignment = bSupported ? OrbitPhysicsRestMath::SupportUpAlignment(
            Sample.Down, SupportNormal) : 0.0;
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

        if (Settings->bStabilizeRestingRigidBodies && bSupported &&
            LinearVelocity.SizeSquared() <= FMath::Square(Settings->RestLinearSpeed) &&
            AngularVelocity.SizeSquared() <= FMath::Square(Settings->RestAngularSpeed))
        {
            Body.StableSeconds += FMath::Max(0.0f, DeltaTime);
            if (Body.StableSeconds >= Settings->RestTime)
            {
                Component->PutRigidBodyToSleep(NAME_None);
                Body.StableSeconds = 0.0f;
                Diagnostics.bBodyAwake = false;
                Diagnostics.SupportState = TEXT("Sleeping after stable support");
                Diagnostics.bUsingNativeContactDamping = Body.bNativeContactDampingApplied;
                Diagnostics.TangentialSpeed = OrbitMotionHistoryMath::CurrentTangentialSpeed(
                    Component->GetPhysicsLinearVelocity(), Acceleration);
                Diagnostics.AngularSpeed = Component->GetPhysicsAngularVelocityInRadians().Size();
                if (UOrbitGravityReceiverComponent* Receiver = (IsValid(Component->GetOwner()) ? Component->GetOwner()->FindComponentByClass<UOrbitGravityReceiverComponent>() : nullptr))
                {
                    Receiver->LastPhysicsBodyDiagnostics = Diagnostics;
                }
                CaptureMotionHistory(Component, Body);
                continue;
            }
        }
        else
        {
            Body.StableSeconds = 0.0f;
        }

        if (bSupported && Settings->bDampSupportedRigidBodyMotion)
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
