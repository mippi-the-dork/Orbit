#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "OrbitAISurfaceNavigation.h"
#include "BTTask_OrbitMoveTo.generated.h"

/** Behavior Tree move task using Orbit's collision-sampled spherical planner, not Recast.
 * Select a Vector or Actor Blackboard key, specify the planet and its surface radius.
 * Abort is request-scoped, so a replaced move cannot be cancelled by an older task.
 */
UCLASS()
class ORBIT_API UBTTask_OrbitMoveTo : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_OrbitMoveTo();

    UPROPERTY(EditAnywhere, Category="Orbit|AI|Move")
    FBlackboardKeySelector TargetKey;

    UPROPERTY(EditAnywhere, Category="Orbit|AI|Move")
    FVector SphereCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category="Orbit|AI|Move", meta=(ClampMin="1.0", Units="cm"))
    float SurfaceRadius = 1000.0f;

    UPROPERTY(EditAnywhere, Category="Orbit|AI|Move")
    FOrbitSphericalNavigationSettings NavigationSettings;

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual uint16 GetInstanceMemorySize() const override;
    virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
    virtual FString GetStaticDescription() const override;
};
