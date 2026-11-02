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
#include "Chaos/ChaosEngineInterface.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrbitChaosGravity, Log, All);

struct FOrbitUniformGravityInput : public Chaos::FSimCallbackInput
{
    Chaos::FVec3 Acceleration = Chaos::FVec3(0, 0, 0);
    void Reset() { Acceleration = Chaos::FVec3(0, 0, 0); }
};

struct FOrbitUniformGravitySimCallback final : public Chaos::TSimCallbackObject<FOrbitUniformGravityInput>
{
    virtual void OnPreSimulate_Internal() override
    {
        if (const FOrbitUniformGravityInput* Input = GetConsumerInput_Internal())
        {
            // Solver thread only. The native scene callback independently owns group 0.
            auto* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(GetSolver());
            RigidsSolver->GetEvolution()->GetGravityForces().SetAcceleration(Input->Acceleration, FOrbitChaosGravityBridge::GravityGroup);
        }
    }

    virtual FName GetFNameForStatId() const override
    {
        static const FName Name(TEXT("OrbitUniformGravity"));
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
    UE_LOG(LogOrbitChaosGravity, Display, TEXT("Experimental uniform Chaos gravity started (group %d). Requires exclusive ownership of this group."), GravityGroup);
    return true;
}

void FOrbitChaosGravityBridge::Update(const FVector& Acceleration)
{
    if (Callback)
    {
        Callback->GetProducerInputData_External()->Acceleration = Chaos::FVec3(Acceleration);
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
}

bool FOrbitChaosGravityBridge::AssignBody(UPrimitiveComponent* Component, int32& OutPreviousGroup)
{
    // This first-stage implementation intentionally excludes skeletal/compound
    // rigid bodies; one actor-wide gravity group must not be applied to only a
    // subset of their individual physics particles.
    if (!Callback || !IsValid(Component) || !Component->IsSimulatingPhysics() ||
        !Component->IsA<UStaticMeshComponent>()) { return false; }
    FBodyInstance* Body = Component->GetBodyInstance();
    if (!Body) { return false; }
    bool bAssigned = false;
    FPhysicsCommand::ExecuteWrite(Body->GetPhysicsActorHandle(), [&](const FPhysicsActorHandle& Handle)
    {
        if (!FChaosEngineInterface::IsValid(Handle)) { return; }
        const int32 ExistingGroup = FChaosEngineInterface::GetGravityGroupIndex_AssumesLocked(Handle);
        if (ExistingGroup != 0) { return; } // Never steal a custom group from another integration.
        OutPreviousGroup = ExistingGroup;
        FChaosEngineInterface::SetGravityGroupIndex_AssumesLocked(Handle, GravityGroup);
        bAssigned = true;
    });
    return bAssigned;
}

void FOrbitChaosGravityBridge::RestoreBody(UPrimitiveComponent* Component, int32 PreviousGroup)
{
    if (!IsValid(Component) || !Component->IsRegistered()) { return; }
    FBodyInstance* Body = Component->GetBodyInstance();
    if (!Body) { return; }
    FPhysicsCommand::ExecuteWrite(Body->GetPhysicsActorHandle(), [PreviousGroup](const FPhysicsActorHandle& Handle)
    {
        if (FChaosEngineInterface::IsValid(Handle))
        {
            FChaosEngineInterface::SetGravityGroupIndex_AssumesLocked(Handle, PreviousGroup);
        }
    });
}
