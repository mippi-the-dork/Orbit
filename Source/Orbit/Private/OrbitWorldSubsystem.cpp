#include "OrbitWorldSubsystem.h"

#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitGravityReceiverComponent.h"
#include "OrbitPhysicsRestMath.h"
#include "OrbitPhysicsIntegrationMath.h"
#include "OrbitChaosGravityBridge.h"
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

void UOrbitWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    PreclaimExperimentalGravityAtBeginPlay(InWorld);
}

void UOrbitWorldSubsystem::PreclaimExperimentalGravityAtBeginPlay(UWorld& InWorld)
{
    if (&InWorld != GetWorld() || !InWorld.IsGameWorld()) { return; }
    const UOrbitSettings* Settings = GetDefault<UOrbitSettings>();
    // Cheap default-off gate: the world-wide scans and Chaos callback below do
    // not run for Standard or pre-existing Experimental installations.
    if (!OrbitPhysicsIntegrationMath::ShouldPreclaimExperimentalGravity(
        Settings->bEnablePhysicsIntegration, Settings->bAutomaticRigidBodyGravity,
        Settings->RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform,
        Settings->bExperimentalPreBeginPlayGravityOwnership, true) ||
        Settings->bCompareNativeWorldGravityWhenEquivalent) { return; }

    // Gravity Source Actors/Components normally register in BeginPlay. They have
    // not yet done so at this lifecycle hook, so take a one-time snapshot of
    // already registered world objects. Their existing BeginPlay registration
    // remains authoritative after this. Never scan an editor or other PIE world.
    for (TObjectIterator<AOrbitGravitySource> It; It; ++It)
    {
        AOrbitGravitySource* Source = *It;
        if (IsValid(Source) && !Source->IsTemplate() && Source->GetWorld() == &InWorld)
        {
            Sources.AddUnique(TWeakObjectPtr<AOrbitGravitySource>(Source));
        }
    }
    for (TObjectIterator<UOrbitGravitySourceComponent> It; It; ++It)
    {
        UOrbitGravitySourceComponent* Source = *It;
        if (IsValid(Source) && !Source->IsTemplate() && Source->GetWorld() == &InWorld && Source->IsRegistered())
        {
            SourceComponents.AddUnique(TWeakObjectPtr<UOrbitGravitySourceComponent>(Source));
        }
    }
    UpdateSourceSnapshot();

    FOrbitFieldDefinition Field;
    FVector UniformAcceleration;
    bool bPoint = false;
    const bool bEligibleSingle = FOrbitChaosGravityBridge::TryGetSolverField(
        ActiveFields, Field, UniformAcceleration, bPoint);
    TArray<FOrbitFieldDefinition> MultiFields;
    const bool bEligibleMulti = !bEligibleSingle && Settings->bExperimentalMultiplePointSources &&
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(ActiveFields, MultiFields);
    TArray<FOrbitFieldDefinition> MixedFields;
    const bool bEligibleMixed = !bEligibleSingle && !bEligibleMulti &&
        Settings->bExperimentalMixedPointDirectionalSources &&
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(ActiveFields, MixedFields);
    TArray<FOrbitFieldDefinition> ExpandedFields;
    const bool bEligibleExpanded = !bEligibleSingle && !bEligibleMulti && !bEligibleMixed &&
        Settings->bExperimentalExpandedFieldCompatibility &&
        FOrbitChaosGravityBridge::TryGetExpandedFields(ActiveFields, ExpandedFields);
    TArray<FOrbitFieldDefinition> ChannelFields;
    const bool bEligibleChannels = Settings->bExperimentalReceiverChannelCompatibility &&
        FOrbitChaosGravityBridge::TryGetExpandedFields(ActiveFields, ChannelFields,
            true, Settings->bExperimentalVortexCompatibility);
    TArray<FOrbitFieldDefinition> VortexFields;
    const bool bEligibleVortex = Settings->bExperimentalVortexCompatibility &&
        FOrbitChaosGravityBridge::TryGetVortexFields(ActiveFields, VortexFields);
    const bool bEligible = OrbitPhysicsIntegrationMath::SelectExperimentalSolver(
        true, bEligibleSingle, Settings->bExperimentalMultiplePointSources, bEligibleMulti,
        Settings->bExperimentalMixedPointDirectionalSources, bEligibleMixed,
        Settings->bExperimentalExpandedFieldCompatibility, bEligibleExpanded,
        Settings->bExperimentalReceiverChannelCompatibility, bEligibleChannels,
        Settings->bExperimentalVortexCompatibility, bEligibleVortex) !=
        OrbitPhysicsIntegrationMath::EExperimentalSolverSelection::None;
    if (!OrbitPhysicsIntegrationMath::ShouldPreclaimExperimentalGravity(
        Settings->bEnablePhysicsIntegration, Settings->bAutomaticRigidBodyGravity,
        true, Settings->bExperimentalPreBeginPlayGravityOwnership, bEligible))
    {
        // Unsupported fields must be picked up by the existing tick path.
        return;
    }
    UpdateExperimentalGravityBridge(Settings);
    if (!ChaosGravityBridge || !ChaosGravityBridge->IsActive()) { return; }

    int32 ClaimedCount = 0;
    // Only ordinary single-particle static-mesh bodies are supported here. Each
    // component is claimed independently, including multiple components on one Actor.
    for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
    {
        UStaticMeshComponent* Component = *It;
        if (IsValid(Component) && !Component->IsTemplate() && Component->GetWorld() == &InWorld &&
            TryClaimExperimentalBody(Component, Settings))
        {
            ++ClaimedCount;
        }
    }
    // Make the pre-BeginPlay receiver registry visible to the physics thread
    // before the first simulation step, not a tick later.
    ChaosGravityBridge->CommitReceiverMasks();
    UE_LOG(LogOrbitWorld, Display,
        TEXT("Experimental early gravity ownership: %d existing static mesh bodies preclaimed before Actor BeginPlay (%s). Runtime-spawned bodies retain tick discovery."),
        ClaimedCount, bEligibleChannels ? TEXT("Receiver Channels") :
            (bEligibleVortex ? TEXT("Vortex") : (bEligibleExpanded ? TEXT("Expanded Fields") :
            (bEligibleMixed ? TEXT("Mixed Point + Directional") :
            (bEligibleMulti ? TEXT("Multiple Point") : (bPoint ? TEXT("Point") : TEXT("Directional")))))));
    // The normal first tick still discovers new/ineligible bodies and refreshes
    // dynamic source snapshots. No special late cleanup is required.
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
    StopExperimentalGravityBridge();
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
    TArray<FOrbitFieldDefinition> PreviousFields = MoveTemp(ActiveFields);
    // Reuse array allocations only for the same live source ID. Moving retains
    // immutable current snapshots while avoiding three large spline buffer copies/frame.
    auto AppendSource = [this, &PreviousFields](auto* Source)
    {
        const int32 PreviousIndex = PreviousFields.IndexOfByPredicate(
            [Source](const FOrbitFieldDefinition& Field) { return Field.SourceId == static_cast<int32>(Source->GetUniqueID()); });
        FOrbitFieldDefinition& Definition = ActiveFields.AddDefaulted_GetRef();
        const bool bHadPrevious = PreviousIndex != INDEX_NONE;
        if (bHadPrevious)
        {
            Definition = MoveTemp(PreviousFields[PreviousIndex]);
            PreviousFields.RemoveAtSwap(PreviousIndex);
        }
        Source->BuildDefinition(Definition, bHadPrevious);
    };
    for (int32 Index = Sources.Num() - 1; Index >= 0; --Index)
    {
        AOrbitGravitySource* Source = Sources[Index].Get();
        if (!IsValid(Source))
        {
            Sources.RemoveAtSwap(Index);
        }
        else if (Source->bEnabled)
        {
            AppendSource(Source);
        }
    }
    for (int32 Index = SourceComponents.Num() - 1; Index >= 0; --Index)
    {
        UOrbitGravitySourceComponent* Source = SourceComponents[Index].Get();
        if (!IsValid(Source) || !Source->IsRegistered()) { SourceComponents.RemoveAtSwap(Index); }
        else if (Source->bEnabled)
        {
            AppendSource(Source);
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

bool UOrbitWorldSubsystem::TryClaimExperimentalBody(UPrimitiveComponent* Component,
    const UOrbitSettings* Settings)
{
    static const FName IgnoreTag(TEXT("Orbit.Ignore"));
    if (!Settings || !ChaosGravityBridge || !ChaosGravityBridge->IsActive() ||
        !IsValid(Component) || Component->IsTemplate() || Component->GetWorld() != GetWorld() ||
        !Component->IsA<UStaticMeshComponent>() || HasManagedBody(Component))
    {
        return false;
    }
    const bool bRegistered = Component->IsRegistered();
    const bool bSimulating = bRegistered && Component->IsSimulatingPhysics();
    const bool bGravityEnabled = bSimulating && Component->IsGravityEnabled();
    const bool bIgnored = Component->ComponentHasTag(IgnoreTag);
    const uint8 Mask = GetReceiverChannelMask(Component->GetOwner());
    if (!bRegistered || !bSimulating || !bGravityEnabled || bIgnored ||
        !FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(
            ChaosGravityBridge->IsMultiSourceMode(), Mask,
            ChaosGravityBridge->IsChannelAwareMode()))
    {
        return false;
    }
    const FOrbitGravitySample Sample = FOrbitFieldEvaluator::CombineForBody(
        ActiveFields, Component->GetCenterOfMass(NAME_None), Component->GetPhysicsLinearVelocity(),
        Mask, Settings->OverrideResolution);
    if (!OrbitPhysicsIntegrationMath::CanPreclaimExperimentalBody(
        bRegistered, bSimulating, bGravityEnabled, bIgnored, Sample.bHasInfluence))
    {
        return false;
    }
    int32 PreviousGroup = 0;
    int32 ParticleId = INDEX_NONE;
    if (!ChaosGravityBridge->AssignBody(Component, PreviousGroup, ParticleId, Mask))
    {
        return false;
    }
    FManagedBody& Body = ManagedBodies.AddDefaulted_GetRef();
    Body.Component = Component;
    Body.bOriginalGravity = bGravityEnabled;
    Body.OriginalLinearDamping = Component->GetLinearDamping();
    Body.OriginalAngularDamping = Component->GetAngularDamping();
    Body.OriginalChaosGravityGroup = PreviousGroup;
    Body.ExperimentalChaosParticleId = ParticleId;
    Body.bExperimentalChaosGroupAssigned = true;
    Component->OnComponentWake.AddUniqueDynamic(this, &UOrbitWorldSubsystem::HandlePhysicsComponentWake);
    Component->OnComponentSleep.AddUniqueDynamic(this, &UOrbitWorldSubsystem::HandlePhysicsComponentSleep);
    // No SetEnableGravity(false) or AddForce: the solver-owned group applies the
    // acceleration in Chaos and prevents a game-thread/physics-thread double force.
    return true;
}

bool UOrbitWorldSubsystem::PrepareSpawnedPhysicsBody(UPrimitiveComponent* Component)
{
    const UOrbitSettings* Settings = GetDefault<UOrbitSettings>();
    if (!Settings || !GetWorld() || !GetWorld()->IsGameWorld() || !IsValid(Component) ||
        Component->GetWorld() != GetWorld() ||
        !OrbitPhysicsIntegrationMath::ShouldPreclaimExperimentalGravity(
            Settings->bEnablePhysicsIntegration, Settings->bAutomaticRigidBodyGravity,
            Settings->RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform,
            Settings->bExperimentalPreBeginPlayGravityOwnership, true) ||
        Settings->bCompareNativeWorldGravityWhenEquivalent)
    {
        return false;
    }
    // Idempotence is valid only while the *same* Chaos particle is still owned.
    // RecreatePhysicsState may replace the particle without changing the UObject,
    // and gameplay may call Prepare again before Orbit's next world tick.
    for (int32 Index = ManagedBodies.Num() - 1; Index >= 0; --Index)
    {
        FManagedBody& Body = ManagedBodies[Index];
        if (Body.Component.Get() != Component) { continue; }
        // Standard owns native gravity through SetEnableGravity(false). Never
        // silently discard that record or claim it as Experimental ownership.
        if (!Body.bExperimentalChaosGroupAssigned) { return false; }
        const bool bEligible = Component->IsRegistered() && Component->IsSimulatingPhysics() &&
            Component->IsGravityEnabled() && !Component->ComponentHasTag(FName(TEXT("Orbit.Ignore")));
        const bool bSameParticle = Body.bExperimentalChaosGroupAssigned &&
            Body.ExperimentalChaosParticleId != INDEX_NONE &&
            FOrbitChaosGravityBridge::IsCurrentParticle(Component, Body.ExperimentalChaosParticleId);
        if (bEligible && bSameParticle) { return true; }

        // Drop old ownership before trying to claim the replacement. Restoration
        // checks identity, so an obsolete group cannot be written to a new particle.
        RestoreBodyDamping(Body);
        RestoreExperimentalBodyGroup(Body);
        UnbindBodyEvents(Body);
        ManagedBodies.RemoveAtSwap(Index);
        if (!bEligible) { return false; }
        break;
    }
    // A just-spawned source may have registered or changed since the previous tick.
    UpdateSourceSnapshot();
    UpdateExperimentalGravityBridge(Settings);
    if (!TryClaimExperimentalBody(Component, Settings)) { return false; }
    ChaosGravityBridge->CommitReceiverMasks();
    return true;
}

int32 UOrbitWorldSubsystem::PrepareSpawnedPhysicsActor(AActor* Actor)
{
    if (!IsValid(Actor) || Actor->GetWorld() != GetWorld() || !GetWorld() ||
        !GetWorld()->IsGameWorld()) { return 0; }

    // GetComponents includes independently simulated child components. Never
    // infer that preparing one rigid body also prepares the Actor's other bodies.
    TInlineComponentArray<UStaticMeshComponent*> Components;
    Actor->GetComponents(Components);
    int32 Claimed = 0;
    for (UStaticMeshComponent* Component : Components)
    {
        if (IsValid(Component) && PrepareSpawnedPhysicsBody(Component))
        {
            ++Claimed;
        }
    }
    return Claimed;
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

void UOrbitWorldSubsystem::RestoreExperimentalBodyGroup(FManagedBody& Body)
{
    if (!Body.bExperimentalChaosGroupAssigned) { return; }
    // A reused particle identifier must never inherit another actor's mask.
    if (ChaosGravityBridge)
    {
        ChaosGravityBridge->RemoveBodyReceiverMask(Body.ExperimentalChaosParticleId);
    }
    if (UPrimitiveComponent* Component = Body.Component.Get())
    {
        // A recreated particle belongs to a new physics state. Do not change
        // its gravity flag based on an ownership record from its predecessor.
        const bool bSameParticle = Body.ExperimentalChaosParticleId == INDEX_NONE ||
            FOrbitChaosGravityBridge::IsCurrentParticle(Component, Body.ExperimentalChaosParticleId);
        if (bSameParticle)
        {
            // Avoid one native-gravity step while switching back to Standard.
            if (Component->IsRegistered()) { Component->SetEnableGravity(false); }
            FOrbitChaosGravityBridge::RestoreBody(Component, Body.OriginalChaosGravityGroup, Body.ExperimentalChaosParticleId);
        }
    }
    Body.bExperimentalChaosGroupAssigned = false;
    Body.ExperimentalChaosParticleId = INDEX_NONE;
}

void UOrbitWorldSubsystem::StopExperimentalGravityBridge()
{
    if (!ChaosGravityBridge) { return; }
    // Bodies are reverted before the solver callback is released, including PIE teardown.
    for (FManagedBody& Body : ManagedBodies) { RestoreExperimentalBodyGroup(Body); }
    delete ChaosGravityBridge;
    ChaosGravityBridge = nullptr;
}

void UOrbitWorldSubsystem::UpdateExperimentalGravityBridge(const UOrbitSettings* Settings)
{
    FVector UniformGravity;
    FOrbitFieldDefinition SolverField;
    bool bPerParticlePoint = false;
    const bool bOptedIn = Settings &&
        Settings->RigidBodyGravityMethod == EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform;
    const bool bEligibleSingle = bOptedIn &&
        FOrbitChaosGravityBridge::TryGetSolverField(ActiveFields, SolverField, UniformGravity, bPerParticlePoint);
    TArray<FOrbitFieldDefinition> MultiPointFields;
    const bool bEligibleMulti = bOptedIn && !bEligibleSingle && Settings->bExperimentalMultiplePointSources &&
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(ActiveFields, MultiPointFields);
    TArray<FOrbitFieldDefinition> MixedFields;
    const bool bEligibleMixed = bOptedIn && !bEligibleSingle && !bEligibleMulti &&
        Settings->bExperimentalMixedPointDirectionalSources &&
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(ActiveFields, MixedFields);
    TArray<FOrbitFieldDefinition> ExpandedFields;
    const bool bEligibleExpanded = bOptedIn && !bEligibleSingle && !bEligibleMulti && !bEligibleMixed &&
        Settings->bExperimentalExpandedFieldCompatibility &&
        FOrbitChaosGravityBridge::TryGetExpandedFields(ActiveFields, ExpandedFields);
    TArray<FOrbitFieldDefinition> ChannelFields;
    const bool bEligibleChannels = bOptedIn && Settings->bExperimentalReceiverChannelCompatibility &&
        FOrbitChaosGravityBridge::TryGetExpandedFields(ActiveFields, ChannelFields,
            true, Settings->bExperimentalVortexCompatibility);
    TArray<FOrbitFieldDefinition> VortexFields;
    const bool bEligibleVortex = bOptedIn && Settings->bExperimentalVortexCompatibility &&
        FOrbitChaosGravityBridge::TryGetVortexFields(ActiveFields, VortexFields);
    const OrbitPhysicsIntegrationMath::EExperimentalSolverSelection Selection =
        OrbitPhysicsIntegrationMath::SelectExperimentalSolver(
            bOptedIn, bEligibleSingle, Settings && Settings->bExperimentalMultiplePointSources, bEligibleMulti,
            Settings && Settings->bExperimentalMixedPointDirectionalSources, bEligibleMixed,
            Settings && Settings->bExperimentalExpandedFieldCompatibility, bEligibleExpanded,
            Settings && Settings->bExperimentalReceiverChannelCompatibility, bEligibleChannels,
            Settings && Settings->bExperimentalVortexCompatibility, bEligibleVortex);
    if (Selection == OrbitPhysicsIntegrationMath::EExperimentalSolverSelection::None)
    {
        if (bOptedIn && !bExperimentalUniformFallbackLogged)
        {
            UE_LOG(LogOrbitWorld, Display, TEXT("Experimental Chaos gravity unavailable for this field configuration; using Standard AddForce gravity."));
            bExperimentalUniformFallbackLogged = true;
        }
        if (!bOptedIn)
        {
            bExperimentalUniformFallbackLogged = false;
            bExperimentalUniformActivationFailed = false;
        }
        StopExperimentalGravityBridge();
        return;
    }
    bExperimentalUniformFallbackLogged = false;
    if (bExperimentalUniformActivationFailed) { return; } // Retry by toggling Standard then Experimental.
    if (!ChaosGravityBridge) { ChaosGravityBridge = new FOrbitChaosGravityBridge(); }
    if (!ChaosGravityBridge->Start(GetWorld()))
    {
        UE_LOG(LogOrbitWorld, Warning, TEXT("Experimental Chaos gravity registration failed; using Standard. Switch to Standard and back to retry."));
        bExperimentalUniformActivationFailed = true;
        StopExperimentalGravityBridge();
        return;
    }
    if (Selection == OrbitPhysicsIntegrationMath::EExperimentalSolverSelection::ChannelFields)
    {
        ChaosGravityBridge->UpdateChannelFields(ChannelFields, Settings->OverrideResolution);
    }
    else if (Selection == OrbitPhysicsIntegrationMath::EExperimentalSolverSelection::VortexFields)
    {
        ChaosGravityBridge->UpdateVortexFields(VortexFields, Settings->OverrideResolution);
    }
    else if (Selection == OrbitPhysicsIntegrationMath::EExperimentalSolverSelection::MultiplePoint)
    {
        ChaosGravityBridge->UpdateMultiplePoint(MultiPointFields, Settings->OverrideResolution);
    }
    else if (Selection == OrbitPhysicsIntegrationMath::EExperimentalSolverSelection::MixedPointDirectional)
    {
        ChaosGravityBridge->UpdateMixedPointDirectional(MixedFields, Settings->OverrideResolution);
    }
    else if (Selection == OrbitPhysicsIntegrationMath::EExperimentalSolverSelection::ExpandedFields)
    {
        ChaosGravityBridge->UpdateExpandedFields(ExpandedFields, Settings->OverrideResolution);
    }
    else
    {
        ChaosGravityBridge->Update(SolverField, UniformGravity, bPerParticlePoint);
    }
}

void UOrbitWorldSubsystem::RestoreBodyGravity()
{
    for (FManagedBody& Body : ManagedBodies)
    {
        UnbindBodyEvents(Body);
        RestoreExperimentalBodyGroup(Body);
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

bool UOrbitWorldSubsystem::IsCurrentExperimentalBodyOwned(UPrimitiveComponent* Component) const
{
    if (!IsValid(Component) || Component->GetWorld() != GetWorld() ||
        !Component->IsRegistered() || !Component->IsSimulatingPhysics() ||
        !Component->IsGravityEnabled()) { return false; }
    for (const FManagedBody& Body : ManagedBodies)
    {
        if (Body.Component.Get() == Component)
        {
            return Body.bExperimentalChaosGroupAssigned &&
                Body.ExperimentalChaosParticleId != INDEX_NONE &&
                FOrbitChaosGravityBridge::IsCurrentParticle(Component, Body.ExperimentalChaosParticleId);
        }
    }
    return false;
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
            RestoreExperimentalBodyGroup(Body);
            UnbindBodyEvents(Body);
            ManagedBodies.RemoveAtSwap(Index);
            continue;
        }
        if (!Component->IsSimulatingPhysics() || Component->ComponentHasTag(IgnoreTag))
        {
            RestoreBodyDamping(Body);
            RestoreExperimentalBodyGroup(Body);
            Component->SetEnableGravity(Body.bOriginalGravity);
            UnbindBodyEvents(Body);
            ManagedBodies.RemoveAtSwap(Index);
            continue;
        }

        // Recreating a physics state replaces the Chaos particle without replacing
        // its UPrimitiveComponent. Remove the stale receiver identity, then let
        // the normal current-tick assignment claim the replacement safely.
        if (Body.bExperimentalChaosGroupAssigned && Body.ExperimentalChaosParticleId != INDEX_NONE &&
            !FOrbitChaosGravityBridge::IsCurrentParticle(Component, Body.ExperimentalChaosParticleId))
        {
            if (ChaosGravityBridge)
            {
                ChaosGravityBridge->RemoveBodyReceiverMask(Body.ExperimentalChaosParticleId);
            }
            Body.bExperimentalChaosGroupAssigned = false;
            Body.ExperimentalChaosParticleId = INDEX_NONE;
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
            RestoreExperimentalBodyGroup(Body);
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
        // Channel-aware and velocity-aware integrations are separately opted
        // in. Standard remains the fallback for every unsupported body/field.
        bool bExperimentalChaos = false;
        if (!bNativeComparison && ChaosGravityBridge && ChaosGravityBridge->IsActive() && Body.bOriginalGravity &&
            FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(
                ChaosGravityBridge->IsMultiSourceMode(), GetReceiverChannelMask(Component->GetOwner()),
                ChaosGravityBridge->IsChannelAwareMode()))
        {
            if (!Body.bExperimentalChaosGroupAssigned)
            {
                int32 PreviousGroup = 0;
                int32 ParticleId = INDEX_NONE;
                const uint8 ReceiverMask = GetReceiverChannelMask(Component->GetOwner());
                if (ChaosGravityBridge->AssignBody(Component, PreviousGroup, ParticleId, ReceiverMask))
                {
                    Body.OriginalChaosGravityGroup = PreviousGroup;
                    Body.ExperimentalChaosParticleId = ParticleId;
                    Body.bExperimentalChaosGroupAssigned = true;
                }
            }
            if (Body.bExperimentalChaosGroupAssigned)
            {
                ChaosGravityBridge->SetBodyReceiverMask(Body.ExperimentalChaosParticleId,
                    GetReceiverChannelMask(Component->GetOwner()));
            }
            bExperimentalChaos = Body.bExperimentalChaosGroupAssigned;
        }
        if (!bExperimentalChaos) { RestoreExperimentalBodyGroup(Body); }
        // True means Chaos integrates gravity itself; either native group 0 or
        // the opt-in solver group 7, never both in the same simulation step.
        const bool bChaosGravity = bNativeComparison || bExperimentalChaos;
        if (Component->IsGravityEnabled() != bChaosGravity)
        {
            Component->SetEnableGravity(bChaosGravity);
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
                Body.LastDiagnostics.bExperimentalChaosUniformGravity = bExperimentalChaos && !ChaosGravityBridge->IsPointMode() && !ChaosGravityBridge->IsMultiSourceMode();
                Body.LastDiagnostics.bExperimentalChaosPointGravity = bExperimentalChaos && ChaosGravityBridge->IsPointMode();
                Body.LastDiagnostics.bExperimentalChaosMultiPointGravity = bExperimentalChaos && ChaosGravityBridge->IsMultiPointMode();
                Body.LastDiagnostics.bExperimentalChaosMixedPointDirectionalGravity = bExperimentalChaos && ChaosGravityBridge->IsMixedPointDirectionalMode();
                Body.LastDiagnostics.bExperimentalChaosExpandedFieldGravity = bExperimentalChaos && ChaosGravityBridge->IsExpandedFieldsMode();
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
        Diagnostics.bExperimentalChaosUniformGravity = bExperimentalChaos && !ChaosGravityBridge->IsPointMode() && !ChaosGravityBridge->IsMultiSourceMode();
        Diagnostics.bExperimentalChaosPointGravity = bExperimentalChaos && ChaosGravityBridge->IsPointMode();
        Diagnostics.bExperimentalChaosMultiPointGravity = bExperimentalChaos && ChaosGravityBridge->IsMultiPointMode();
        Diagnostics.bExperimentalChaosMixedPointDirectionalGravity = bExperimentalChaos && ChaosGravityBridge->IsMixedPointDirectionalMode();
        Diagnostics.bExperimentalChaosExpandedFieldGravity = bExperimentalChaos && ChaosGravityBridge->IsExpandedFieldsMode();
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

        if (!bChaosGravity && !Acceleration.IsNearlyZero())
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
        StopExperimentalGravityBridge();
        bNeedsInitialDiscovery = true;
        if (bMotionHistoryRecording) { FinishMotionHistory(TEXT("Orbit physics integration disabled or no eligible sources")); }
        return;
    }

    UpdateExperimentalGravityBridge(Settings);
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
    // Newly discovered bodies and receiver-mask changes are applied as one
    // coherent physics-thread input; never read UObject receiver state in Chaos.
    if (ChaosGravityBridge) { ChaosGravityBridge->CommitReceiverMasks(); }
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
