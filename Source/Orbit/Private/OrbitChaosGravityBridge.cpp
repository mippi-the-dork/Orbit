#include "OrbitChaosGravityBridge.h"

#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/PhysicsSolverBaseImpl.h"
#include "Chaos/SimCallbackInput.h"
#include "Chaos/SimCallbackObject.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Chaos/ChaosEngineInterface.h"
#include "UObject/UObjectIterator.h"
#include "Stats/Stats.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrbitChaosGravity, Log, All);

struct FOrbitUniformGravityInput : public Chaos::FSimCallbackInput
{
    Chaos::FVec3 Acceleration = Chaos::FVec3(0, 0, 0);
    FOrbitFieldDefinition PointField;
    bool bPerParticlePoint = false;
    bool bPerParticleMultiPoint = false;
    TArray<FOrbitFieldDefinition> MultiPointFields;
    // Copied by the callback input. Only IDs owned by THIS solver group appear.
    // Never use the gravity group alone to decide a receiver's channels.
    TMap<int32, uint8> ReceiverMasksByParticleId;
    bool bUseReceiverChannelMasks = false;
    bool bUseBodyVelocity = false;
    EOrbitOverrideResolution OverrideResolution = EOrbitOverrideResolution::StrongestInfluence;
    void Reset()
    {
        Acceleration = Chaos::FVec3(0, 0, 0);
        PointField = FOrbitFieldDefinition();
        bPerParticlePoint = false;
        bPerParticleMultiPoint = false;
        MultiPointFields.Reset();
        ReceiverMasksByParticleId.Reset();
        bUseReceiverChannelMasks = false;
        bUseBodyVelocity = false;
        OverrideResolution = EOrbitOverrideResolution::StrongestInfluence;
    }
};

// The uniform implementation remains unchanged. For single-Point and
// multi-source gravity, group 7 is set to zero and the pre-integrate callback injects each
// body's acceleration directly on the physics thread before Chaos integration.
// Solver-owned bodies never also use game-thread AddForce.
struct FOrbitUniformGravitySimCallback final : public Chaos::TSimCallbackObject<
    FOrbitUniformGravityInput, Chaos::FSimCallbackNoOutput,
    Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PreIntegrate>
{
    FOrbitFieldDefinition CurrentPointField;
    bool bCurrentPointField = false;
    bool bCurrentMultiPointField = false;
    TArray<FOrbitFieldDefinition> CurrentMultiPointFields;
    TMap<int32, uint8> CurrentReceiverMasks;
    bool bCurrentChannelAware = false;
    bool bCurrentBodyVelocity = false;
    EOrbitOverrideResolution CurrentOverrideResolution = EOrbitOverrideResolution::StrongestInfluence;

    virtual void OnPreSimulate_Internal() override
    {
        if (const FOrbitUniformGravityInput* Input = GetConsumerInput_Internal())
        {
            bCurrentPointField = Input->bPerParticlePoint;
            bCurrentMultiPointField = Input->bPerParticleMultiPoint;
            if (bCurrentPointField) { CurrentPointField = Input->PointField; }
            if (bCurrentMultiPointField)
            {
                // Deep copy on the physics thread, never read or retain a UObject.
                CurrentMultiPointFields = Input->MultiPointFields;
                CurrentOverrideResolution = Input->OverrideResolution;
                bCurrentChannelAware = Input->bUseReceiverChannelMasks;
                bCurrentBodyVelocity = Input->bUseBodyVelocity;
                if (bCurrentChannelAware) { CurrentReceiverMasks = Input->ReceiverMasksByParticleId; }
                else { CurrentReceiverMasks.Reset(); }
            }
            else
            {
                CurrentMultiPointFields.Reset();
                CurrentReceiverMasks.Reset();
                bCurrentChannelAware = false;
                bCurrentBodyVelocity = false;
            }
            auto* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
            RigidsSolver->GetEvolution()->GetGravityForces().SetAcceleration(
                (bCurrentPointField || bCurrentMultiPointField) ?
                    Chaos::FVec3(0, 0, 0) : Input->Acceleration,
                FOrbitChaosGravityBridge::GravityGroup);
        }
    }

    virtual void OnPreIntegrate_Internal() override
    {
        if (!bCurrentPointField && !bCurrentMultiPointField) { return; }
        // Visible in Unreal's native CPU profiling; only executed for an active
        // experimental physics-thread callback (never the Standard path).
        QUICK_SCOPE_CYCLE_COUNTER(STAT_OrbitExperimentalGravityPreIntegrate);
        auto* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
        const FOrbitFieldDefinition& Field = CurrentPointField;
        const TArray<FOrbitFieldDefinition>& Fields = CurrentMultiPointFields;
        const bool bMultiSource = bCurrentMultiPointField;
        const bool bChannelAware = bCurrentChannelAware;
        const bool bVelocityAware = bCurrentBodyVelocity;
        const TMap<int32, uint8>& ReceiverMasks = CurrentReceiverMasks;
        const EOrbitOverrideResolution Resolution = CurrentOverrideResolution;
        // The group is an ownership marker; it is NOT used to approximate
        // multiple positions with a shared gravity vector. Only dynamic,
        // awake particles participate; Chaos retains its own sleep decisions.
        RigidsSolver->GetEvolution()->GetParticles().GetNonDisabledDynamicView().ParallelFor(
            [&Field, &Fields, &ReceiverMasks, bMultiSource, bChannelAware, bVelocityAware, Resolution](auto& Particle, int32)
            {
                if (!Particle.GravityEnabled() ||
                    Particle.GravityGroupIndex() != FOrbitChaosGravityBridge::GravityGroup ||
                    Particle.ObjectState() != Chaos::EObjectStateType::Dynamic)
                {
                    return;
                }
                const auto& Position = Particle.GetX();
                const FVector WorldPosition(Position.X, Position.Y, Position.Z);
                FVector Acceleration = FVector::ZeroVector;
                if (bMultiSource)
                {
                    // Keyed by Chaos's persistent particle identity, not by
                    // position, Actor, source or allocation order. An unknown
                    // ID MUST NOT inherit channel 0: default to zero gravity
                    // until the game-thread registration snapshot arrives.
                    const uint8* KnownMask = bChannelAware ?
                        ReceiverMasks.Find(Particle.UniqueIdx().Idx) : nullptr;
                    const uint8 ReceiverMask = bChannelAware ? (KnownMask ? *KnownMask : 0u) : 1u;
                    if (ReceiverMask != 0u)
                    {
                        const auto& Velocity = Particle.GetV();
                        const FVector WorldVelocity(Velocity.X, Velocity.Y, Velocity.Z);
                        // Body Velocity is needed for Spiral Capture; the same
                        // evaluator also handles ordinary continuous Vortex.
                        const FOrbitGravitySample Sample = FOrbitFieldEvaluator::CombineForBody(
                            Fields, WorldPosition, bVelocityAware ? WorldVelocity : FVector::ZeroVector,
                            ReceiverMask, Resolution);
                        Acceleration = Sample.Acceleration;
                    }
                }
                else
                {
                    FOrbitFieldEvaluator::Evaluate(Field, WorldPosition, Acceleration);
                }
                if (!Acceleration.ContainsNaN() && !Acceleration.IsNearlyZero())
                {
                    Particle.Acceleration() += Chaos::FVec3(Acceleration);
                }
            });
    }

    virtual FName GetFNameForStatId() const override
    {
        static const FName Name(TEXT("OrbitExperimentalChaosGravity"));
        return Name;
    }
};

bool FOrbitChaosGravityBridge::TryGetUniformField(const TArray<FOrbitFieldDefinition>& Fields, FVector& OutAcceleration)
{
    OutAcceleration = FVector::ZeroVector;
    int32 EligibleCount = 0;
    for (const FOrbitFieldDefinition& Field : Fields)
    {
        if (!Field.bPhysics || Field.ChannelMask == 0u) { continue; }
        ++EligibleCount;
        // Finite, falloff, velocity-dependent and multi-field cases must not be approximated.
        if (EligibleCount != 1 || Field.Shape != EOrbitGravityShape::Directional ||
            Field.Falloff != EOrbitGravityFalloff::Constant || Field.MaxDistance > 0.0 || Field.bLimitToBox ||
            (Field.Direction != EOrbitGravityDirection::Inward && Field.Direction != EOrbitGravityDirection::Outward) ||
            !FMath::IsFinite(Field.Strength) || Field.DirectionVector.ContainsNaN())
        {
            return false;
        }
        FVector Sample;
        if (!FOrbitFieldEvaluator::Evaluate(Field, FVector::ZeroVector, Sample) || Sample.ContainsNaN())
        {
            return false;
        }
        OutAcceleration = Sample;
    }
    return EligibleCount == 1;
}

bool FOrbitChaosGravityBridge::TryGetSolverField(const TArray<FOrbitFieldDefinition>& Fields,
    FOrbitFieldDefinition& OutField, FVector& OutUniformAcceleration, bool& bOutPerParticlePoint)
{
    bOutPerParticlePoint = false;
    OutUniformAcceleration = FVector::ZeroVector;
    // Preserve the proven uniform eligibility contract byte-for-byte.
    if (TryGetUniformField(Fields, OutUniformAcceleration))
    {
        for (const FOrbitFieldDefinition& Field : Fields)
        {
            if (Field.bPhysics && Field.ChannelMask != 0u) { OutField = Field; break; }
        }
        return true;
    }

    // First per-body experiment: exactly one ordinary Point gravity source.
    // No vortex, spline, axis, volume, or multi-source approximation.
    const FOrbitFieldDefinition* Candidate = nullptr;
    for (const FOrbitFieldDefinition& Field : Fields)
    {
        if (!Field.bPhysics || Field.ChannelMask == 0u) { continue; }
        if (Candidate) { return false; }
        Candidate = &Field;
    }
    if (!Candidate || Candidate->Shape != EOrbitGravityShape::Point ||
        (Candidate->Direction != EOrbitGravityDirection::Inward &&
         Candidate->Direction != EOrbitGravityDirection::Outward) ||
        Candidate->bLimitToBox || Candidate->Center.ContainsNaN() ||
        !FMath::IsFinite(Candidate->Strength) ||
        !FMath::IsFinite(Candidate->MaxDistance) ||
        !FMath::IsFinite(Candidate->ReferenceDistance) ||
        Candidate->MaxDistance < 0.0 || Candidate->ReferenceDistance <= 0.0)
    {
        return false;
    }
    OutField = *Candidate;
    bOutPerParticlePoint = true;
    return true;
}

bool FOrbitChaosGravityBridge::TryGetMultiplePointFields(
    const TArray<FOrbitFieldDefinition>& Fields, TArray<FOrbitFieldDefinition>& OutFields)
{
    OutFields.Reset();
    // A bounded, channel-homogeneous experiment. The single-source paths
    // retain their historical policy and do not call this method.
    for (const FOrbitFieldDefinition& Field : Fields)
    {
        if (!Field.bPhysics || Field.ChannelMask == 0u) { continue; }
        if (OutFields.Num() >= MaxExperimentalPointSources ||
            Field.Shape != EOrbitGravityShape::Point || Field.ChannelMask != 1u ||
            (Field.Direction != EOrbitGravityDirection::Inward &&
             Field.Direction != EOrbitGravityDirection::Outward) ||
            Field.bLimitToBox || Field.Center.ContainsNaN() ||
            !FMath::IsFinite(Field.Strength) || !FMath::IsFinite(Field.MaxDistance) ||
            !FMath::IsFinite(Field.ReferenceDistance) ||
            Field.MaxDistance < 0.0 || Field.ReferenceDistance <= 0.0)
        {
            OutFields.Reset();
            return false;
        }
        OutFields.Add(Field);
    }
    if (OutFields.Num() < 2)
    {
        OutFields.Reset();
        return false;
    }
    return true;
}

bool FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(
    const TArray<FOrbitFieldDefinition>& Fields, TArray<FOrbitFieldDefinition>& OutFields)
{
    OutFields.Reset();
    bool bHasPoint = false;
    bool bHasDirectional = false;
    for (const FOrbitFieldDefinition& Field : Fields)
    {
        if (!Field.bPhysics || Field.ChannelMask == 0u) { continue; }
        if (OutFields.Num() >= MaxExperimentalPointSources || Field.ChannelMask != 1u ||
            Field.bLimitToBox || !FMath::IsFinite(Field.Strength) ||
            !FMath::IsFinite(Field.MaxDistance) || Field.MaxDistance < 0.0 ||
            Field.Center.ContainsNaN() ||
            (Field.Direction != EOrbitGravityDirection::Inward &&
             Field.Direction != EOrbitGravityDirection::Outward))
        {
            OutFields.Reset();
            return false;
        }
        if (Field.Shape == EOrbitGravityShape::Point)
        {
            if (!FMath::IsFinite(Field.ReferenceDistance) || Field.ReferenceDistance <= 0.0)
            {
                OutFields.Reset();
                return false;
            }
            bHasPoint = true;
        }
        else if (Field.Shape == EOrbitGravityShape::Directional)
        {
            // Limit the first mixed experiment to the already-validated uniform
            // Directional subset. No position-dependent Directional boundaries.
            if (Field.MaxDistance != 0.0 || Field.Falloff != EOrbitGravityFalloff::Constant ||
                Field.DirectionVector.ContainsNaN() || Field.DirectionVector.IsNearlyZero())
            {
                OutFields.Reset();
                return false;
            }
            bHasDirectional = true;
        }
        else
        {
            OutFields.Reset();
            return false;
        }
        OutFields.Add(Field);
    }
    if (!bHasPoint || !bHasDirectional || OutFields.Num() < 2)
    {
        OutFields.Reset();
        return false;
    }
    return true;
}

bool FOrbitChaosGravityBridge::TryGetExpandedFields(
    const TArray<FOrbitFieldDefinition>& Fields, TArray<FOrbitFieldDefinition>& OutFields,
    bool bAllowChannels, bool bAllowVortex)
{
    OutFields.Reset();
    // Legacy callers remain channel-0-only. The separate opted-in channel
    // selector carries each body's mask by stable Chaos particle identity.
    for (const FOrbitFieldDefinition& Field : Fields)
    {
        if (!Field.bPhysics || Field.ChannelMask == 0u) { continue; }
        const bool bVortex = Field.Direction == EOrbitGravityDirection::VortexInward ||
            Field.Direction == EOrbitGravityDirection::VortexOutward;
        const bool bValidVortex = bAllowVortex && bVortex &&
            (Field.Shape == EOrbitGravityShape::Point || Field.Shape == EOrbitGravityShape::Axis ||
             Field.Shape == EOrbitGravityShape::Spline) &&
            FMath::IsFinite(Field.VortexSpin) && FMath::IsFinite(Field.VortexResponseRate) &&
            FMath::IsFinite(Field.VortexConvergenceRate) &&
            FMath::IsFinite(Field.VortexAngularRateRadians) &&
            FMath::IsFinite(Field.VortexMaxFlowSpeed) &&
            FMath::IsFinite(Field.VortexMaxAccelerationFactor);
        if (OutFields.Num() >= MaxExperimentalPointSources ||
            (bAllowChannels ? ((Field.ChannelMask & ~0x07u) != 0u) : Field.ChannelMask != 1u) ||
            (bVortex && !bValidVortex) ||
            Field.Center.ContainsNaN() || Field.Axis.ContainsNaN() ||
            Field.DirectionVector.ContainsNaN() || Field.Rotation.ContainsNaN() ||
            Field.BoxExtents.ContainsNaN() ||
            !FMath::IsFinite(Field.Strength) || !FMath::IsFinite(Field.MaxDistance) ||
            !FMath::IsFinite(Field.ReferenceDistance) ||
            !FMath::IsFinite(Field.VolumeRadius) || !FMath::IsFinite(Field.VolumeHalfHeight) ||
            Field.Strength < 0.0 || Field.MaxDistance < 0.0 || Field.ReferenceDistance <= 0.0 ||
            (Field.BoxExtents.X < 0.0 || Field.BoxExtents.Y < 0.0 || Field.BoxExtents.Z < 0.0) || Field.VolumeRadius <= 0.0 ||
            Field.VolumeHalfHeight <= 0.0 ||
            (Field.Direction != EOrbitGravityDirection::Inward &&
             Field.Direction != EOrbitGravityDirection::Outward &&
             Field.Direction != EOrbitGravityDirection::SplineUp &&
             Field.Direction != EOrbitGravityDirection::SplineDown && !bValidVortex))
        {
            OutFields.Reset();
            return false;
        }
        if (Field.Shape == EOrbitGravityShape::Point)
        {
            if (Field.Direction != EOrbitGravityDirection::Inward &&
                Field.Direction != EOrbitGravityDirection::Outward && !bValidVortex)
            { OutFields.Reset(); return false; }
        }
        else if (Field.Shape == EOrbitGravityShape::Axis)
        {
            if (Field.Axis.IsNearlyZero() ||
                (Field.Direction != EOrbitGravityDirection::Inward &&
                 Field.Direction != EOrbitGravityDirection::Outward && !bValidVortex))
            { OutFields.Reset(); return false; }
        }
        else if (Field.Shape == EOrbitGravityShape::Directional || Field.Shape == EOrbitGravityShape::Volume)
        {
            if (Field.DirectionVector.IsNearlyZero() ||
                (Field.Direction != EOrbitGravityDirection::Inward &&
                 Field.Direction != EOrbitGravityDirection::Outward)) { OutFields.Reset(); return false; }
            if (Field.Shape == EOrbitGravityShape::Volume &&
                Field.VolumeShape != EOrbitVolumeShape::Box &&
                Field.VolumeShape != EOrbitVolumeShape::Sphere &&
                Field.VolumeShape != EOrbitVolumeShape::Cylinder &&
                Field.VolumeShape != EOrbitVolumeShape::Capsule)
            {
                OutFields.Reset(); return false;
            }
        }
        else if (Field.Shape == EOrbitGravityShape::Spline)
        {
            if (Field.Curve.Num() < 2 || Field.Curve.Num() > 4096 ||
                ((Field.Direction == EOrbitGravityDirection::SplineUp ||
                  Field.Direction == EOrbitGravityDirection::SplineDown) &&
                  Field.CurveUp.Num() != Field.Curve.Num()))
            {
                OutFields.Reset(); return false;
            }
            bool bUsableSegment = false;
            for (int32 Index = 0; Index < Field.Curve.Num(); ++Index)
            {
                if (Field.Curve[Index].ContainsNaN() ||
                    (Field.CurveUp.IsValidIndex(Index) && Field.CurveUp[Index].ContainsNaN()))
                {
                    OutFields.Reset(); return false;
                }
                if (Index > 0 && !Field.Curve[Index].Equals(Field.Curve[Index - 1], 0.001))
                {
                    bUsableSegment = true;
                }
            }
            if (!bUsableSegment) { OutFields.Reset(); return false; }
        }
        else
        {
            OutFields.Reset(); return false;
        }
        OutFields.Add(Field);
    }
    if (OutFields.IsEmpty()) { return false; }
    return true;
}

bool FOrbitChaosGravityBridge::TryGetVortexFields(
    const TArray<FOrbitFieldDefinition>& Fields, TArray<FOrbitFieldDefinition>& OutFields,
    bool bAllowChannels)
{
    if (!TryGetExpandedFields(Fields, OutFields, bAllowChannels, true)) { return false; }
    // Don't steal existing (non-Vortex) modes merely because this option is on.
    for (const FOrbitFieldDefinition& Field : OutFields)
    {
        if (Field.Direction == EOrbitGravityDirection::VortexInward ||
            Field.Direction == EOrbitGravityDirection::VortexOutward)
        {
            return true;
        }
    }
    OutFields.Reset();
    return false;
}

bool FOrbitChaosGravityBridge::Start(UWorld* World)
{
    if (Callback) { return true; }
    if (!World || !World->GetPhysicsScene() || !World->GetPhysicsScene()->GetSolver()) { return false; }
    // Do not overwrite a group already used by the host project. This check
    // happens only on first experimental activation, never in Standard mode.
    for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
    {
        UPrimitiveComponent* Component = *It;
        if (!IsValid(Component) || Component->GetWorld() != World ||
            !Component->IsRegistered() || !Component->IsSimulatingPhysics()) { continue; }
        FBodyInstance* Body = Component->GetBodyInstance();
        if (!Body) { continue; }
        bool bConflict = false;
        FPhysicsCommand::ExecuteRead(Body->GetPhysicsActorHandle(), [&](const FPhysicsActorHandle& Handle)
        {
            bConflict = FChaosEngineInterface::IsValid(Handle) &&
                FChaosEngineInterface::GetGravityGroupIndex_AssumesLocked(Handle) == GravityGroup;
        });
        if (bConflict)
        {
            UE_LOG(LogOrbitChaosGravity, Warning,
                TEXT("Experimental gravity refused: a physics body already uses Chaos gravity group %d. Standard mode remains active."), GravityGroup);
            return false;
        }
    }
    Solver = World->GetPhysicsScene()->GetSolver();
    Callback = Solver->CreateAndRegisterSimCallbackObject_External<FOrbitUniformGravitySimCallback>();
    if (!Callback) { Solver = nullptr; return false; }
    UE_LOG(LogOrbitChaosGravity, Display, TEXT("Experimental Chaos gravity started (group %d). Requires exclusive ownership of this group."), GravityGroup);
    return true;
}

void FOrbitChaosGravityBridge::Update(const FOrbitFieldDefinition& Field,
    const FVector& UniformAcceleration, bool bPerParticlePoint)
{
    if (Callback)
    {
        bPointMode = bPerParticlePoint;
        bMultiPointMode = false;
        bMixedPointDirectionalMode = false;
        bExpandedFieldsMode = false;
        bChannelAwareMode = false;
        bVortexMode = false;
        FOrbitUniformGravityInput* Input = Callback->GetProducerInputData_External();
        Input->Acceleration = Chaos::FVec3(UniformAcceleration);
        Input->bPerParticlePoint = bPerParticlePoint;
        Input->bPerParticleMultiPoint = false;
        Input->bUseReceiverChannelMasks = false;
        Input->bUseBodyVelocity = false;
        Input->ReceiverMasksByParticleId.Reset();
        Input->MultiPointFields.Reset();
        if (bPerParticlePoint) { Input->PointField = Field; }
    }
}

void FOrbitChaosGravityBridge::UpdateMultiplePoint(
    const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution)
{
    if (Callback)
    {
        bPointMode = false;
        bMultiPointMode = true;
        bMixedPointDirectionalMode = false;
        bExpandedFieldsMode = false;
        bChannelAwareMode = false;
        bVortexMode = false;
        FOrbitUniformGravityInput* Input = Callback->GetProducerInputData_External();
        Input->Acceleration = Chaos::FVec3(0, 0, 0);
        Input->bPerParticlePoint = false;
        Input->bPerParticleMultiPoint = true;
        Input->bUseReceiverChannelMasks = false;
        Input->bUseBodyVelocity = false;
        Input->ReceiverMasksByParticleId.Reset();
        Input->MultiPointFields = Fields;
        Input->OverrideResolution = Resolution;
    }
}

void FOrbitChaosGravityBridge::UpdateMixedPointDirectional(
    const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution)
{
    if (Callback)
    {
        bPointMode = false;
        bMultiPointMode = false;
        bMixedPointDirectionalMode = true;
        bExpandedFieldsMode = false;
        bChannelAwareMode = false;
        bVortexMode = false;
        // The solver callback already uses CombineForBody for multi-source
        // integration. Mixed support changes eligibility, not gravity math.
        FOrbitUniformGravityInput* Input = Callback->GetProducerInputData_External();
        Input->Acceleration = Chaos::FVec3(0, 0, 0);
        Input->bPerParticlePoint = false;
        Input->bPerParticleMultiPoint = true;
        Input->bUseReceiverChannelMasks = false;
        Input->bUseBodyVelocity = false;
        Input->ReceiverMasksByParticleId.Reset();
        Input->MultiPointFields = Fields;
        Input->OverrideResolution = Resolution;
    }
}

void FOrbitChaosGravityBridge::UpdateExpandedFields(
    const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution)
{
    if (!Callback) { return; }
    bPointMode = false;
    bMultiPointMode = false;
    bMixedPointDirectionalMode = false;
    bExpandedFieldsMode = true;
    bChannelAwareMode = false;
    bVortexMode = false;
    // Use the existing physics-thread multi-field evaluator. No new force path.
    FOrbitUniformGravityInput* Input = Callback->GetProducerInputData_External();
    Input->Acceleration = Chaos::FVec3(0, 0, 0);
    Input->bPerParticlePoint = false;
    Input->bPerParticleMultiPoint = true;
    Input->bUseReceiverChannelMasks = false;
    Input->bUseBodyVelocity = false;
    Input->ReceiverMasksByParticleId.Reset();
    Input->MultiPointFields = Fields;
    Input->OverrideResolution = Resolution;
}

void FOrbitChaosGravityBridge::UpdateChannelFields(
    const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution)
{
    if (!Callback) { return; }
    bPointMode = false;
    bMultiPointMode = false;
    bMixedPointDirectionalMode = false;
    bExpandedFieldsMode = true;
    bChannelAwareMode = true;
    bVortexMode = Fields.ContainsByPredicate([](const FOrbitFieldDefinition& Field)
    {
        return Field.Direction == EOrbitGravityDirection::VortexInward ||
            Field.Direction == EOrbitGravityDirection::VortexOutward;
    });
    FOrbitUniformGravityInput* Input = Callback->GetProducerInputData_External();
    Input->Acceleration = Chaos::FVec3(0, 0, 0);
    Input->bPerParticlePoint = false;
    Input->bPerParticleMultiPoint = true;
    Input->bUseReceiverChannelMasks = true;
    Input->bUseBodyVelocity = bVortexMode;
    Input->MultiPointFields = Fields;
    Input->ReceiverMasksByParticleId = ReceiverMasksByParticleId;
    Input->OverrideResolution = Resolution;
}

void FOrbitChaosGravityBridge::UpdateVortexFields(
    const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution)
{
    if (!Callback) { return; }
    bPointMode = false;
    bMultiPointMode = false;
    bMixedPointDirectionalMode = false;
    bExpandedFieldsMode = true;
    bChannelAwareMode = false;
    bVortexMode = true;
    FOrbitUniformGravityInput* Input = Callback->GetProducerInputData_External();
    Input->Acceleration = Chaos::FVec3(0, 0, 0);
    Input->bPerParticlePoint = false;
    Input->bPerParticleMultiPoint = true;
    Input->bUseReceiverChannelMasks = false;
    Input->bUseBodyVelocity = true;
    Input->ReceiverMasksByParticleId.Reset();
    Input->MultiPointFields = Fields;
    Input->OverrideResolution = Resolution;
}

void FOrbitChaosGravityBridge::SetBodyReceiverMask(int32 ParticleId, uint8 ReceiverMask)
{
    if (ParticleId != INDEX_NONE && ReceiverMask != 0u)
    {
        ReceiverMasksByParticleId.Add(ParticleId, ReceiverMask);
    }
}

void FOrbitChaosGravityBridge::RemoveBodyReceiverMask(int32 ParticleId)
{
    if (ParticleId != INDEX_NONE) { ReceiverMasksByParticleId.Remove(ParticleId); }
}

void FOrbitChaosGravityBridge::CommitReceiverMasks()
{
    if (Callback && bChannelAwareMode)
    {
        FOrbitUniformGravityInput* Input = Callback->GetProducerInputData_External();
        Input->ReceiverMasksByParticleId = ReceiverMasksByParticleId;
    }
}

void FOrbitChaosGravityBridge::Stop()
{
    if (Solver && Callback)
    {
        Solver->UnregisterAndFreeSimCallbackObject_External(Callback);
    }
    Callback = nullptr;
    Solver = nullptr;
    bPointMode = false;
    bMultiPointMode = false;
    bMixedPointDirectionalMode = false;
    bExpandedFieldsMode = false;
    bChannelAwareMode = false;
    bVortexMode = false;
    ReceiverMasksByParticleId.Reset();
}

bool FOrbitChaosGravityBridge::AssignBody(UPrimitiveComponent* Component,
    int32& OutPreviousGroup, int32& OutParticleId, uint8 ReceiverMask)
{
    // This first-stage implementation intentionally excludes skeletal/compound
    // rigid bodies; one actor-wide gravity group must not be applied to only a
    // subset of their individual physics particles.
    OutParticleId = INDEX_NONE;
    if (!Callback || ReceiverMask == 0u || !IsValid(Component) || !Component->IsSimulatingPhysics() ||
        !Component->IsA<UStaticMeshComponent>()) { return false; }
    FBodyInstance* Body = Component->GetBodyInstance();
    if (!Body) { return false; }
    bool bAssigned = false;
    FPhysicsCommand::ExecuteWrite(Body->GetPhysicsActorHandle(), [&](const FPhysicsActorHandle& Handle)
    {
        if (!FChaosEngineInterface::IsValid(Handle)) { return; }
        const int32 ExistingGroup = FChaosEngineInterface::GetGravityGroupIndex_AssumesLocked(Handle);
        if (ExistingGroup != 0) { return; } // Never steal a custom group from another integration.
        const Chaos::FUniqueIdx UniqueId = Handle->GetGameThreadAPI().UniqueIdx();
        if (bChannelAwareMode && !UniqueId.IsValid()) { return; }
        OutPreviousGroup = ExistingGroup;
        OutParticleId = UniqueId.IsValid() ? UniqueId.Idx : INDEX_NONE;
        FChaosEngineInterface::SetGravityGroupIndex_AssumesLocked(Handle, GravityGroup);
        bAssigned = true;
    });
    if (bAssigned) { SetBodyReceiverMask(OutParticleId, ReceiverMask); }
    return bAssigned;
}

bool FOrbitChaosGravityBridge::IsCurrentParticle(UPrimitiveComponent* Component, int32 ExpectedParticleId)
{
    if (ExpectedParticleId == INDEX_NONE || !IsValid(Component) || !Component->IsRegistered()) { return false; }
    FBodyInstance* Body = Component->GetBodyInstance();
    if (!Body) { return false; }
    bool bMatches = false;
    FPhysicsCommand::ExecuteWrite(Body->GetPhysicsActorHandle(), [&](const FPhysicsActorHandle& Handle)
    {
        if (!FChaosEngineInterface::IsValid(Handle)) { return; }
        const Chaos::FUniqueIdx Identity = Handle->GetGameThreadAPI().UniqueIdx();
        bMatches = Identity.IsValid() && Identity.Idx == ExpectedParticleId;
    });
    return bMatches;
}

void FOrbitChaosGravityBridge::RestoreBody(UPrimitiveComponent* Component, int32 PreviousGroup, int32 ExpectedParticleId)
{
    if (!IsValid(Component) || !Component->IsRegistered()) { return; }
    FBodyInstance* Body = Component->GetBodyInstance();
    if (!Body) { return; }
    FPhysicsCommand::ExecuteWrite(Body->GetPhysicsActorHandle(), [PreviousGroup, ExpectedParticleId](const FPhysicsActorHandle& Handle)
    {
        if (FChaosEngineInterface::IsValid(Handle))
        {
            const Chaos::FUniqueIdx Identity = Handle->GetGameThreadAPI().UniqueIdx();
            if (ExpectedParticleId != INDEX_NONE &&
                (!Identity.IsValid() || Identity.Idx != ExpectedParticleId)) { return; }
            FChaosEngineInterface::SetGravityGroupIndex_AssumesLocked(Handle, PreviousGroup);
        }
    });
}
