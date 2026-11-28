#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "OrbitAISurfaceNavigation.h"
#include "OrbitStateTreeMoveToTask.generated.h"

class AAIController;
class AActor;
class UOrbitAIPathFollowerComponent;

/** Inputs are bindable from ordinary StateTree context/evaluator properties. */
USTRUCT(BlueprintType)
struct ORBITSTATETREE_API FOrbitStateTreeMoveToInstanceData
{
    GENERATED_BODY()

    /** Bind the current AIController; no Orbit-specific StateTree schema is required. */
    UPROPERTY(EditAnywhere, Category="Input", meta=(Input))
    TObjectPtr<AAIController> Controller = nullptr;

    /** Optional moving target; when provided this overrides Destination. */
    UPROPERTY(EditAnywhere, Category="Input", meta=(Input))
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(EditAnywhere, Category="Input", meta=(Input))
    FVector Destination = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category="Input", meta=(Input))
    FVector SphereCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category="Input", meta=(Input, ClampMin="100.0"))
    float SurfaceRadius = 1000.0f;

    UPROPERTY(EditAnywhere, Category="Parameter")
    FOrbitSphericalNavigationSettings NavigationSettings;

    /** A StateTree task may abort only the request it started. */
    UPROPERTY(Transient)
    int32 OwnedRequestId = 0;

    UPROPERTY(Transient)
    TWeakObjectPtr<UOrbitAIPathFollowerComponent> OwnedFollower;
};

/** Native StateTree Move To adapter; does not replace StateTree or Unreal's native AI. */
USTRUCT(meta=(DisplayName="Orbit Move to Location", Category="Orbit|AI"))
struct ORBITSTATETREE_API FOrbitStateTreeMoveToTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FOrbitStateTreeMoveToInstanceData;

    FOrbitStateTreeMoveToTask();
    virtual const UStruct* GetInstanceDataType() const override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const override;

    /** Pure request ownership/status translation, exposed for native verification. */
    static EStateTreeRunStatus EvaluateRequest(const UOrbitAIPathFollowerComponent* Follower, int32 RequestId);
};
