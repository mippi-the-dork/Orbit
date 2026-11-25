#include "BTTask_OrbitMoveTo.h"
#include "OrbitAIPathFollowerComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "BehaviorTree/BlackboardData.h"

namespace
{
    struct FOrbitBTMoveMemory
    {
        TWeakObjectPtr<UOrbitAIPathFollowerComponent> Follower;
        int32 RequestId = 0;
    };

    UOrbitAIPathFollowerComponent* FindFollower(UBehaviorTreeComponent& OwnerComp)
    {
        AAIController* Controller = OwnerComp.GetAIOwner();
        if (!Controller) Controller = Cast<AAIController>(OwnerComp.GetOwner());
        ACharacter* Character = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
        return Character ? Character->FindComponentByClass<UOrbitAIPathFollowerComponent>() : nullptr;
    }
}

UBTTask_OrbitMoveTo::UBTTask_OrbitMoveTo()
{
    NodeName = TEXT("Orbit Move To (Spherical Surface)");
    bNotifyTick = true;
    TargetKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_OrbitMoveTo, TargetKey));
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_OrbitMoveTo, TargetKey), AActor::StaticClass());
}

void UBTTask_OrbitMoveTo::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);
    if (UBlackboardData* Blackboard = Asset.BlackboardAsset)
    {
        // Resolve against the tree being initialized, including the runtime
        // template duplicated by UBehaviorTreeManager::LoadTree.
        TargetKey.ResolveSelectedKey(*Blackboard);
    }
}

uint16 UBTTask_OrbitMoveTo::GetInstanceMemorySize() const
{
    return sizeof(FOrbitBTMoveMemory);
}

EBTNodeResult::Type UBTTask_OrbitMoveTo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FOrbitBTMoveMemory* Memory = reinterpret_cast<FOrbitBTMoveMemory*>(NodeMemory);
    // UE initializes task memory as raw bytes. Weak pointers need constructed storage.
    new (Memory) FOrbitBTMoveMemory();
    UOrbitAIPathFollowerComponent* Follower = FindFollower(OwnerComp);
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    // A transient test may create the Blackboard before its BrainComponent.
    // The controller's Blackboard remains the authoritative data source.
    if (!Blackboard)
    {
        AAIController* Controller = OwnerComp.GetAIOwner()
            ? OwnerComp.GetAIOwner() : Cast<AAIController>(OwnerComp.GetOwner());
        Blackboard = Controller ? Controller->GetBlackboardComponent() : nullptr;
    }
#if WITH_DEV_AUTOMATION_TESTS
    UE_LOG(LogTemp, Display, TEXT("Orbit BT ExecuteTask: follower=%d blackboard=%d key=%s keyType=%s controller=%s"),
        Follower != nullptr, Blackboard != nullptr, *TargetKey.SelectedKeyName.ToString(),
        *GetNameSafe(TargetKey.SelectedKeyType.Get()), *GetNameSafe(OwnerComp.GetAIOwner()));
#endif
    if (!Follower || !Blackboard || TargetKey.SelectedKeyName.IsNone()) return EBTNodeResult::Failed;
    bool bStarted = false;
    if (TargetKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
    {
        AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetKey.SelectedKeyName));
        if (IsValid(Target))
        {
            Follower->StopFollowing();
            bStarted = Follower->FollowSphericalSurfaceActor(Target, SphereCenter, SurfaceRadius, NavigationSettings);
        }
    }
    else if (TargetKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
    {
        const FVector Destination = Blackboard->GetValueAsVector(TargetKey.SelectedKeyName);
        if (!Destination.ContainsNaN())
        {
            Follower->StopFollowing();
            bStarted = Follower->FollowSphericalSurfacePath(Destination, SphereCenter, SurfaceRadius, NavigationSettings);
        }
    }
    if (!bStarted)
    {
#if WITH_DEV_AUTOMATION_TESTS
        UE_LOG(LogTemp, Display, TEXT("Orbit BT ExecuteTask: request failed; pathStatus=%s selectedType=%s"),
            Follower ? *Follower->LastPathStatus : TEXT("no follower"),
            *GetNameSafe(TargetKey.SelectedKeyType.Get()));
#endif
        return EBTNodeResult::Failed;
    }
    Memory->Follower = Follower;
    Memory->RequestId = Follower->MoveRequestId;
    return EBTNodeResult::InProgress;
}

void UBTTask_OrbitMoveTo::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const FOrbitBTMoveMemory* Memory = reinterpret_cast<const FOrbitBTMoveMemory*>(NodeMemory);
    UOrbitAIPathFollowerComponent* Follower = Memory->Follower.Get();
    if (!Follower || Memory->RequestId != Follower->MoveRequestId)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }
    if (Follower->bFollowingPath) return;
    FinishLatentTask(OwnerComp, Follower->LastMoveResult == EOrbitAIMoveResult::Succeeded
        ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}

EBTNodeResult::Type UBTTask_OrbitMoveTo::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    const FOrbitBTMoveMemory* Memory = reinterpret_cast<const FOrbitBTMoveMemory*>(NodeMemory);
    if (UOrbitAIPathFollowerComponent* Follower = Memory->Follower.Get())
    {
        Follower->AbortMoveRequest(Memory->RequestId);
    }
    return EBTNodeResult::Aborted;
}

FString UBTTask_OrbitMoveTo::GetStaticDescription() const
{
    return FString::Printf(TEXT("Target: %s | Planet radius: %.0f cm | Spherical collision navigation (not Recast)"),
        *TargetKey.SelectedKeyName.ToString(), SurfaceRadius);
}
