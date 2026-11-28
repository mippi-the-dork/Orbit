#include "OrbitStateTreeMoveToTask.h"
#include "OrbitAIBlueprintLibrary.h"
#include "AIController.h"
#include "OrbitAIPathFollowerComponent.h"
#include "StateTreeExecutionContext.h"

FOrbitStateTreeMoveToTask::FOrbitStateTreeMoveToTask()
{
    bShouldCallTick = true;
    bConsideredForCompletion = true;
}

const UStruct* FOrbitStateTreeMoveToTask::GetInstanceDataType() const
{
    return FInstanceDataType::StaticStruct();
}

EStateTreeRunStatus FOrbitStateTreeMoveToTask::EvaluateRequest(
    const UOrbitAIPathFollowerComponent* Follower, int32 RequestId)
{
    if (!IsValid(Follower) || RequestId <= 0 || Follower->MoveRequestId != RequestId)
    {
        // Another task/Blueprint has replaced the request. Never take ownership of it.
        return EStateTreeRunStatus::Failed;
    }
    switch (Follower->LastMoveResult)
    {
        case EOrbitAIMoveResult::Succeeded: return EStateTreeRunStatus::Succeeded;
        case EOrbitAIMoveResult::InProgress:
            return Follower->bFollowingPath ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
        default: return EStateTreeRunStatus::Failed;
    }
}

EStateTreeRunStatus FOrbitStateTreeMoveToTask::EnterState(
    FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    Data.OwnedRequestId = 0;
    Data.OwnedFollower.Reset();
    AAIController* Controller = Data.Controller.Get();
    if (!IsValid(Controller)) return EStateTreeRunStatus::Failed;
    UOrbitAIPathFollowerComponent* Follower = UOrbitAIBlueprintLibrary::GetOrbitFollower(Controller);
    if (!IsValid(Follower)) return EStateTreeRunStatus::Failed;
    int32 StartedId = 0;
    const bool bStarted = IsValid(Data.TargetActor.Get())
        ? UOrbitAIBlueprintLibrary::MoveToActor(Controller, Data.TargetActor.Get(),
            Data.SphereCenter, Data.SurfaceRadius, Data.NavigationSettings, StartedId)
        : UOrbitAIBlueprintLibrary::MoveToLocation(Controller, Data.Destination,
            Data.SphereCenter, Data.SurfaceRadius, Data.NavigationSettings, StartedId);
    if (!bStarted) return EStateTreeRunStatus::Failed;
    Data.OwnedRequestId = StartedId;
    Data.OwnedFollower = Follower;
    return EvaluateRequest(Follower, Data.OwnedRequestId);
}

EStateTreeRunStatus FOrbitStateTreeMoveToTask::Tick(
    FStateTreeExecutionContext& Context, float DeltaTime) const
{
    const FInstanceDataType& Data = Context.GetInstanceData(*this);
    return EvaluateRequest(Data.OwnedFollower.Get(), Data.OwnedRequestId);
}

void FOrbitStateTreeMoveToTask::ExitState(
    FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    if (UOrbitAIPathFollowerComponent* Follower = Data.OwnedFollower.Get())
    {
        if (EvaluateRequest(Follower, Data.OwnedRequestId) == EStateTreeRunStatus::Running)
        {
            Follower->AbortMoveRequest(Data.OwnedRequestId);
        }
    }
    Data.OwnedRequestId = 0;
    Data.OwnedFollower.Reset();
}
