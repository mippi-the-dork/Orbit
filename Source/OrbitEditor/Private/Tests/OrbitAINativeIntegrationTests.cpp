#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitAIController.h"
#include "OrbitAIPathFollowerComponent.h"
#include "OrbitCharacterGravityComponent.h"
#include "OrbitGravitySource.h"
#include "OrbitWorldSubsystem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTreeManager.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BTTask_OrbitMoveTo.h"
#include "Tests/AutomationCommon.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"

namespace OrbitNativeAITests
{
    struct FFixture
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* World = nullptr;
        ACharacter* Character = nullptr;
        AOrbitAIController* Controller = nullptr;
        UOrbitAIPathFollowerComponent* Follower = nullptr;
        FOrbitSphericalNavigationSettings Settings;
        bool Initialize()
        {
            if (!WorldWrapper.CreateTestWorld(EWorldType::Game)) return false;
            World = WorldWrapper.GetTestWorld();
            if (!World) return false;
            AActor* Planet = World->SpawnActor<AActor>();
            if (!Planet) return false;
            USphereComponent* Surface = NewObject<USphereComponent>(Planet);
            Planet->AddInstanceComponent(Surface);
            Planet->SetRootComponent(Surface);
            Surface->InitSphereRadius(1000.0f);
            Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Surface->SetCollisionResponseToAllChannels(ECR_Block);
            Surface->RegisterComponent();
            Character = World->SpawnActor<ACharacter>();
            if (!Character) return false;
            Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, FVector::ForwardVector));
            Character->SetActorLocation(FVector(1000.0 + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0, 0, 0),
                false, nullptr, ETeleportType::TeleportPhysics);
            Character->GetCharacterMovement()->SetGravityDirection(FVector(-1, 0, 0));
            Follower = NewObject<UOrbitAIPathFollowerComponent>(Character);
            Character->AddInstanceComponent(Follower);
            Follower->RegisterComponent();
            Follower->bRequireGrounded = false;
            Controller = World->SpawnActor<AOrbitAIController>();
            if (!Controller) return false;
            Controller->Possess(Character);
            Settings.LongitudeSamples = 24;
            Settings.LatitudeSamples = 13;
            return Controller->GetPawn() == Character;
        }
        FVector SurfacePoint(double Angle) const
        {
            return FVector(1000.0 * FMath::Cos(Angle), 1000.0 * FMath::Sin(Angle), 0.0);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAINativeControllerPossession,
    "Orbit.AI.Native.ControllerPossessionAndRequestOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAINativeControllerPossession::RunTest(const FString& Parameters)
{
    OrbitNativeAITests::FFixture F;
    if (!TestTrue(TEXT("Possess Character in isolated world"), F.Initialize())) return false;
    if (!TestEqual(TEXT("Controller resolves the possessed Character's follower"),
        F.Controller->GetOrbitPathFollower(), F.Follower)) return false;
    if (!TestTrue(TEXT("Controller accepts spherical surface request"),
        F.Controller->MoveOrbitToLocation(F.SurfacePoint(0.5), FVector::ZeroVector, 1000.0f, F.Settings))) return false;
    const int32 FirstRequest = F.Follower->MoveRequestId;
    TestTrue(TEXT("Spherical move is active"), F.Follower->bFollowingPath);
    TestTrue(TEXT("Active move result"), F.Follower->LastMoveResult == EOrbitAIMoveResult::InProgress);
    if (!TestTrue(TEXT("Replacement path is accepted"), F.Follower->FollowLocation(F.SurfacePoint(0.3)))) return false;
    TestTrue(TEXT("Replacement request advances identity"), F.Follower->MoveRequestId != FirstRequest);
    TestFalse(TEXT("Older task must not abort newer move"), F.Follower->AbortMoveRequest(FirstRequest));
    TestTrue(TEXT("Replacement remains active"), F.Follower->bFollowingPath);
    TestTrue(TEXT("Active task can abort its own move"), F.Follower->AbortMoveRequest(F.Follower->MoveRequestId));
    TestTrue(TEXT("Abort reports terminal status"), F.Follower->LastMoveResult == EOrbitAIMoveResult::Aborted);
    if (!TestTrue(TEXT("Possessed Character may begin another request"),
        F.Controller->MoveOrbitToLocation(F.SurfacePoint(0.55), FVector::ZeroVector, 1000.0f, F.Settings))) return false;
    F.Controller->UnPossess();
    TestFalse(TEXT("Unpossessing cancels active Orbit movement"), F.Follower->bFollowingPath);
    TestTrue(TEXT("Unpossess has abort result"), F.Follower->LastMoveResult == EOrbitAIMoveResult::Aborted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAINativeActorChase,
    "Orbit.AI.Native.MovingTargetRepathAndLoss",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAINativeActorChase::RunTest(const FString& Parameters)
{
    OrbitNativeAITests::FFixture F;
    if (!TestTrue(TEXT("Create possessed Character fixture"), F.Initialize())) return false;
    AActor* Target = F.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Moving target"), Target)) return false;
    USceneComponent* TargetRoot = NewObject<USceneComponent>(Target);
    Target->AddInstanceComponent(TargetRoot);
    Target->SetRootComponent(TargetRoot);
    TargetRoot->RegisterComponent();
    Target->SetActorLocation(F.SurfacePoint(0.5));
    F.Follower->MovingTargetReplanDistance = 50.0f;
    if (!TestTrue(TEXT("Start surface Actor chase"),
        F.Controller->MoveOrbitToActor(Target, FVector::ZeroVector, 1000.0f, F.Settings))) return false;
    const int32 Request = F.Follower->MoveRequestId;
    Target->SetActorLocation(F.SurfacePoint(0.7));
    F.Follower->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Moving target triggers a new surface path"), F.Follower->SurfaceReplanCount > 0);
    TestEqual(TEXT("Replanning retains request identity"), F.Follower->MoveRequestId, Request);
    TestTrue(TEXT("Chase continues after moving target"), F.Follower->bFollowingPath);
    Target->Destroy();
    F.Follower->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Destroyed target ends the chase"), F.Follower->bFollowingPath);
    TestTrue(TEXT("Lost target is failure, not success"), F.Follower->LastMoveResult == EOrbitAIMoveResult::Failed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAINativeBTPolicy,
    "Orbit.AI.Native.BehaviorTreeTaskSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAINativeBTPolicy::RunTest(const FString& Parameters)
{
    UBTTask_OrbitMoveTo* Task = NewObject<UBTTask_OrbitMoveTo>();
    if (!TestNotNull(TEXT("Orbit BT task is constructible"), Task)) return false;
    TestTrue(TEXT("BT task advertises spherical navigation"), Task->GetStaticDescription().Contains(TEXT("Spherical")));
    TestTrue(TEXT("BT task has per-execution request memory"), Task->GetInstanceMemorySize() > 0);
    TestTrue(TEXT("BT task has valid default planet radius"), Task->SurfaceRadius > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAINativeDiagnostics,
    "Orbit.AI.Native.LiveMoveDiagnostics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAINativeDiagnostics::RunTest(const FString& Parameters)
{
    OrbitNativeAITests::FFixture F;
    if (!TestTrue(TEXT("Create possessed Character fixture"), F.Initialize())) return false;
    const FOrbitAIMoveDiagnostics Idle = F.Controller->GetOrbitMoveDiagnostics();
    TestFalse(TEXT("Initial snapshot is inactive"), Idle.bActive);
    TestEqual(TEXT("Initial snapshot has no waypoints"), Idle.WaypointCount, 0);
    if (!TestTrue(TEXT("Controller starts an Orbit move"),
        F.Controller->MoveOrbitToLocation(F.SurfacePoint(0.45), FVector::ZeroVector, 1000.0f, F.Settings))) return false;
    const FOrbitAIMoveDiagnostics Active = F.Controller->GetOrbitMoveDiagnostics();
    TestTrue(TEXT("Live snapshot reports active movement"), Active.bActive);
    TestTrue(TEXT("Live snapshot recognizes the spherical route"), Active.bSphericalRoute);
    TestEqual(TEXT("Live snapshot uses active request ID"), Active.RequestId, F.Follower->MoveRequestId);
    TestTrue(TEXT("Live snapshot includes path points"), Active.WaypointCount > 0);
    TestEqual(TEXT("Live snapshot has in-progress result"), Active.Result, EOrbitAIMoveResult::InProgress);
    TestTrue(TEXT("Live status is populated"), !Active.Status.IsEmpty());
    const int32 CurrentId = Active.RequestId;
    TestTrue(TEXT("Only current request may be aborted"), F.Follower->AbortMoveRequest(CurrentId));
    const FOrbitAIMoveDiagnostics Stopped = F.Controller->GetOrbitMoveDiagnostics();
    TestFalse(TEXT("Terminal snapshot is inactive"), Stopped.bActive);
    TestEqual(TEXT("Terminal snapshot identifies aborted request"), Stopped.RequestId, CurrentId);
    TestEqual(TEXT("Terminal snapshot reports aborted result"), Stopped.Result, EOrbitAIMoveResult::Aborted);
    F.Controller->UnPossess();
    TestFalse(TEXT("Controller returns safe idle snapshot without pawn"), F.Controller->GetOrbitMoveDiagnostics().bActive);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAINativeBehaviorTreeExecution,
    "Orbit.AI.Native.BehaviorTreeStartAndAbort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAINativeBehaviorTreeExecution::RunTest(const FString& Parameters)
{
    OrbitNativeAITests::FFixture F;
    if (!TestTrue(TEXT("Create AI-possessed Character and spherical planet"), F.Initialize())) return false;

    UBlackboardData* BlackboardAsset = NewObject<UBlackboardData>(GetTransientPackage());
    FBlackboardEntry& GoalEntry = BlackboardAsset->Keys.AddDefaulted_GetRef();
    GoalEntry.EntryName = TEXT("OrbitGoal");
    GoalEntry.KeyType = NewObject<UBlackboardKeyType_Vector>(BlackboardAsset);
    // Transient fixtures do not receive the editor/PostLoad asset bookkeeping that
    // normally resolves key IDs before AAIController::UseBlackboard.
    BlackboardAsset->UpdateParentKeys();
    BlackboardAsset->UpdateKeyIDs();
    if (!TestTrue(TEXT("Transient Blackboard schema is valid"), BlackboardAsset->IsValid())) return false;
    if (!TestTrue(TEXT("Transient Blackboard resolves destination key"),
        BlackboardAsset->GetKeyID(GoalEntry.EntryName) != FBlackboard::InvalidKey)) return false;

    UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage());
    Tree->BlackboardAsset = BlackboardAsset;
    UBTComposite_Sequence* Root = NewObject<UBTComposite_Sequence>(Tree);
    UBTTask_OrbitMoveTo* MoveTask = NewObject<UBTTask_OrbitMoveTo>(Tree);
    MoveTask->TargetKey.SelectedKeyName = GoalEntry.EntryName;
    MoveTask->SurfaceRadius = 1000.0f;
    MoveTask->NavigationSettings = F.Settings;
    Root->Children.AddDefaulted_GetRef().ChildTask = MoveTask;
    Tree->RootNode = Root;
    // The transient task never went through the editor's Blackboard-selector
    // resolution pass. Resolve the key against the same schema as the controller.
    MoveTask->TargetKey.ResolveSelectedKey(*BlackboardAsset);
    if (!TestEqual(TEXT("BT destination selector resolves as a Vector key"),
        MoveTask->TargetKey.SelectedKeyType.Get(), UBlackboardKeyType_Vector::StaticClass())) return false;

    // FTestWorldWrapper creates a minimal game world. Unlike a normally started
    // gameplay world it may not instantiate the AI system, which native
    // Blackboard initialization requires. Create it before UseBlackboard.
    if (!TestNotNull(TEXT("Isolated gameplay world has an AI system"), F.World->CreateAISystem())) return false;
    // UE 5.8.3 World.cpp: UWorld::BeginPlay starts subsystems, then calls
    // GameMode->StartPlay when a GameMode exists. FTestWorldWrapper has no
    // gameplay GameMode, so WorldSettings::NotifyBeginPlay is never reached.
    // NotifyBeginPlay sets UWorld's begun-play state AND dispatches BeginPlay
    // on the test actors. Calling UWorld::BeginPlay alone does neither here.
    if (!F.World->HasBegunPlay())
    {
        AWorldSettings* WorldSettings = F.World->GetWorldSettings();
        if (!TestNotNull(TEXT("Isolated world has WorldSettings for gameplay startup"), WorldSettings)) return false;
        F.World->BeginPlay();
        WorldSettings->NotifyBeginPlay();
    }
    if (!TestTrue(TEXT("AI fixture completed WorldSettings BeginPlay notification"), F.World->HasBegunPlay())) return false;
    UBlackboardComponent* Blackboard = nullptr;
    if (!TestTrue(TEXT("Controller initializes Blackboard asset"), F.Controller->UseBlackboard(BlackboardAsset, Blackboard))) return false;
    if (!TestNotNull(TEXT("Runtime Blackboard exists"), Blackboard)) return false;
    Blackboard->SetValueAsVector(GoalEntry.EntryName, F.SurfacePoint(0.45));
    if (!TestTrue(TEXT("Blackboard provides the intended world-space destination"),
        Blackboard->GetValueAsVector(GoalEntry.EntryName).Equals(F.SurfacePoint(0.45), 0.1))) return false;
    if (!TestTrue(TEXT("Controller remains possessed before BT startup"), F.Controller->GetPawn() == F.Character)) return false;
    if (!TestTrue(TEXT("Controller starts actual Behavior Tree"), F.Controller->RunBehaviorTree(Tree))) return false;
    UBehaviorTreeComponent* Brain = Cast<UBehaviorTreeComponent>(F.Controller->GetBrainComponent());
    if (!TestNotNull(TEXT("Behavior Tree runtime component exists"), Brain)) return false;
    // Inspect the runtime *duplicate*, not the authoring node. UE's BT manager
    // duplicates RootNode and initializes each cloned task on LoadTree.
    UBehaviorTreeManager* RuntimeManager = UBehaviorTreeManager::GetCurrent(F.World);
    if (!TestNotNull(TEXT("Runtime BT manager exists"), RuntimeManager)) return false;
    UBTCompositeNode* RuntimeRoot = nullptr;
    uint16 RuntimeMemoryBytes = 0;
    if (!TestTrue(TEXT("BT manager supplies initialized runtime template"),
        RuntimeManager->LoadTree(*Tree, RuntimeRoot, RuntimeMemoryBytes))) return false;
    if (!TestNotNull(TEXT("Runtime template root"), RuntimeRoot)) return false;
    if (!TestTrue(TEXT("Runtime template retains task"),
        RuntimeRoot->Children.Num() == 1 && RuntimeRoot->Children[0].ChildTask != nullptr)) return false;
    UBTTask_OrbitMoveTo* RuntimeTask = Cast<UBTTask_OrbitMoveTo>(RuntimeRoot->Children[0].ChildTask);
    if (!TestNotNull(TEXT("Runtime template contains Orbit Move To"), RuntimeTask)) return false;
    if (!TestEqual(TEXT("Runtime task has resolved Vector selector"),
        RuntimeTask->TargetKey.SelectedKeyType.Get(), UBlackboardKeyType_Vector::StaticClass())) return false;
    // In the transient test world, RunBehaviorTree creates its Brain after
    // UseBlackboard has already initialized the controller's Blackboard.
    // Unlike a normally authored AI Controller with pre-existing components,
    // the new Brain has not cached that earlier Blackboard. Use the native
    // UBrainComponent API to establish the same relationship explicitly.
    Brain->CacheBlackboardComponent(Blackboard);
    Blackboard->CacheBrainComponent(*Brain);
    if (!TestEqual(TEXT("Behavior Tree shares the controller Blackboard"),
        Brain->GetBlackboardComponent(), Blackboard)) return false;
    // BT startup is scheduled through the world's timer/tick infrastructure,
    // not merely the BehaviorTreeComponent's direct TickComponent call.
    // Advance the isolated game world to service pending execution requests.
    for (int32 Step = 0; Step < 12 && !F.Follower->bFollowingPath; ++Step)
    {
        F.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
        Brain->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    }
    if (!TestTrue(FString::Printf(TEXT("Real BT task issues an Orbit move (path=%s, treeRunning=%d, key=%s, worldBegunPlay=%d, destination=(%s), request=%d, rootChildren=%d, runtimeKey=%s, brainRegistered=%d)"),
        *F.Follower->LastPathStatus, Brain->IsRunning() ? 1 : 0,
        *MoveTask->TargetKey.SelectedKeyName.ToString(), F.World->HasBegunPlay() ? 1 : 0,
        *Blackboard->GetValueAsVector(GoalEntry.EntryName).ToString(), F.Follower->MoveRequestId,
        Root->Children.Num(), *RuntimeTask->TargetKey.SelectedKeyName.ToString(), Brain->IsRegistered() ? 1 : 0),
        F.Follower->bFollowingPath)) return false;
    const int32 StartedRequest = F.Follower->MoveRequestId;
    Brain->StopTree(EBTStopMode::Safe);
    for (int32 Step = 0; Step < 8 && F.Follower->bFollowingPath; ++Step)
    {
        F.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
        Brain->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    }
    TestFalse(TEXT("Stopping Behavior Tree cancels the move it owns"), F.Follower->bFollowingPath);
    TestEqual(TEXT("Behavior Tree abort retains original request ID"), F.Follower->MoveRequestId, StartedRequest);
    TestEqual(TEXT("Behavior Tree abort reports aborted result"), F.Follower->LastMoveResult, EOrbitAIMoveResult::Aborted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAINativeRegisteredGravity,
    "Orbit.AI.Native.RegisteredPlanetGravitySource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAINativeRegisteredGravity::RunTest(const FString& Parameters)
{
    OrbitNativeAITests::FFixture F;
    if (!TestTrue(TEXT("Create AI-possessed Character and spherical collision world"), F.Initialize())) return false;
    UOrbitWorldSubsystem* GravityWorld = F.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Orbit world subsystem exists"), GravityWorld)) return false;
    AOrbitGravitySource* Source = F.World->SpawnActor<AOrbitGravitySource>();
    if (!TestNotNull(TEXT("Spawn registered point gravity source"), Source)) return false;
    Source->Shape = EOrbitGravityShape::Point;
    Source->Direction = EOrbitGravityDirection::Inward;
    Source->Falloff = EOrbitGravityFalloff::Constant;
    Source->Strength = 980.0;
    Source->MaxDistance = 0.0;
    Source->bAffectCharacters = true;
    Source->bEnabled = true;
    GravityWorld->RegisterSource(Source);
    const FOrbitGravitySample Sample = GravityWorld->SampleGravityForActor(
        F.Character, F.Character->GetActorLocation(), EOrbitGravityChannel::Character);
    TestTrue(TEXT("Planet gravity source influences the AI Character"), Sample.bHasInfluence);
    TestTrue(TEXT("Planet's gravity points towards its center"),
        FVector::DotProduct(Sample.Down.GetSafeNormal(), FVector(-1, 0, 0)) > 0.99);

    UOrbitCharacterGravityComponent* GravityAdapter = NewObject<UOrbitCharacterGravityComponent>(F.Character);
    F.Character->AddInstanceComponent(GravityAdapter);
    GravityAdapter->bSmoothGravityDirection = false;
    GravityAdapter->bAlignActorToGravity = false;
    GravityAdapter->RegisterComponent();
    // FTestWorldWrapper creates a usable collision/query world, but its Actor
    // BeginPlay and component tick scheduling aren't equivalent to PIE startup.
    // Step this actual runtime component explicitly so the source-acquisition
    // assertion does not depend on the fixture's world tick registration.
    GravityAdapter->Activate(true);
    if (!TestTrue(TEXT("Gravity adapter is active"), GravityAdapter->IsActive())) return false;
    // TickComponent is public on the UActorComponent interface, while Orbit's
    // override is intentionally protected. Dispatch through the public base
    // interface so virtual dispatch still runs the real Orbit implementation.
    static_cast<UActorComponent*>(GravityAdapter)->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    const FOrbitAIMoveDiagnostics Snapshot = F.Controller->GetOrbitMoveDiagnostics();
    TestTrue(TEXT("Orbit gravity component sees the registered source"), GravityAdapter->bInsideOrbitField);
    TestTrue(TEXT("Live AI diagnostics expose source ownership"), Snapshot.bInsideOrbitGravityField);
    TestEqual(TEXT("AI diagnostics report the same dominant source"), Snapshot.GravitySourceId, GravityAdapter->LastDominantSourceId);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAINativeBehaviorTreePhysicalCompletion,
    "Orbit.AI.Native.BehaviorTreePhysicsSteppedSuccess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAINativeBehaviorTreePhysicalCompletion::RunTest(const FString& Parameters)
{
    OrbitNativeAITests::FFixture F;
    if (!TestTrue(TEXT("Create native Character and planet"), F.Initialize())) return false;
    if (!TestNotNull(TEXT("Initialize AI system"), F.World->CreateAISystem())) return false;
    if (!F.World->HasBegunPlay())
    {
        AWorldSettings* Settings = F.World->GetWorldSettings();
        if (!TestNotNull(TEXT("WorldSettings for BT simulation"), Settings)) return false;
        F.World->BeginPlay();
        Settings->NotifyBeginPlay();
    }
    if (!TestTrue(TEXT("Gameplay BeginPlay completed"), F.World->HasBegunPlay())) return false;

    UBlackboardData* Schema = NewObject<UBlackboardData>(GetTransientPackage());
    FBlackboardEntry& GoalKey = Schema->Keys.AddDefaulted_GetRef();
    GoalKey.EntryName = TEXT("OrbitPhysicalGoal");
    GoalKey.KeyType = NewObject<UBlackboardKeyType_Vector>(Schema);
    Schema->UpdateParentKeys();
    Schema->UpdateKeyIDs();
    UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage());
    Tree->BlackboardAsset = Schema;
    UBTComposite_Sequence* Root = NewObject<UBTComposite_Sequence>(Tree);
    UBTTask_OrbitMoveTo* Task = NewObject<UBTTask_OrbitMoveTo>(Tree);
    Task->TargetKey.SelectedKeyName = GoalKey.EntryName;
    Task->TargetKey.ResolveSelectedKey(*Schema);
    Task->SurfaceRadius = 1000.0f;
    Task->NavigationSettings = F.Settings;
    Root->Children.AddDefaulted_GetRef().ChildTask = Task;
    Tree->RootNode = Root;
    UBlackboardComponent* Blackboard = nullptr;
    if (!TestTrue(TEXT("Initialize real native Blackboard"), F.Controller->UseBlackboard(Schema, Blackboard))) return false;
    if (!TestNotNull(TEXT("BT Blackboard exists"), Blackboard)) return false;
    Blackboard->SetValueAsVector(GoalKey.EntryName, F.SurfacePoint(PI / 5.0));
    if (!TestTrue(TEXT("Native controller creates and starts its Brain"), F.Controller->RunBehaviorTree(Tree))) return false;
    UBehaviorTreeComponent* Brain = Cast<UBehaviorTreeComponent>(F.Controller->GetBrainComponent());
    if (!TestNotNull(TEXT("Native BT Brain exists"), Brain)) return false;
    Brain->CacheBlackboardComponent(Blackboard);
    Blackboard->CacheBrainComponent(*Brain);
    // Single-run semantics expose a terminal BT success cleanly, without the
    // controller's usual looped tree immediately requesting the goal again.
    Brain->StopTree(EBTStopMode::Forced);
    Brain->StartTree(*Tree, EBTExecutionMode::SingleRun);
    for (int32 Step = 0; Step < 12 && !F.Follower->bFollowingPath; ++Step)
    {
        F.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
        Brain->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    }
    if (!TestTrue(TEXT("Runtime BT task created an active spherical request"), F.Follower->bFollowingPath)) return false;
    const int32 Request = F.Follower->MoveRequestId;
    UCharacterMovementComponent* Movement = F.Character->GetCharacterMovement();
    if (!TestNotNull(TEXT("Native CharacterMovement exists"), Movement)) return false;
    Movement->bRunPhysicsWithNoController = true;
    Movement->MaxWalkSpeed = 240.0f;
    Movement->MaxAcceleration = 1500.0f;
    Movement->BrakingDecelerationWalking = 1400.0f;
    Movement->SetMovementMode(MOVE_Walking);
    F.Follower->AcceptanceRadius = 60.0f;
    F.Follower->bRequireGrounded = true;
    const FVector InitialLocation = F.Character->GetActorLocation();
    constexpr float Dt = 1.0f / 60.0f;
    for (int32 Step = 0; Step < 660 && F.Follower->bFollowingPath; ++Step)
    {
        const FVector Up = F.Character->GetActorLocation().GetSafeNormal();
        Movement->SetGravityDirection(-Up);
        F.Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, Up));
        F.Follower->TickComponent(Dt, LEVELTICK_All, nullptr);
        Movement->TickComponent(Dt, LEVELTICK_All, nullptr);
        Brain->TickComponent(Dt, LEVELTICK_All, nullptr);
    }
    if (!TestTrue(TEXT("BT-controlled Character physically traversed sphere"),
        FVector::Dist(InitialLocation, F.Character->GetActorLocation()) > 150.0)) return false;
    if (!TestEqual(FString::Printf(TEXT("BT follower reached physical goal (%s)"), *F.Follower->LastPathStatus),
        F.Follower->LastMoveResult, EOrbitAIMoveResult::Succeeded)) return false;
    TestEqual(TEXT("BT success retains request identity"), F.Follower->MoveRequestId, Request);
    // Advance the native tree once after movement succeeds so its TickTask can
    // call FinishLatentTask(Succeeded) instead of only observing follower state.
    for (int32 Step = 0; Step < 12 && Brain->IsRunning(); ++Step)
    {
        Brain->TickComponent(Dt, LEVELTICK_All, nullptr);
        F.World->Tick(LEVELTICK_All, Dt);
    }
    TestFalse(TEXT("Single-run BT finishes after real arrival"), Brain->IsRunning());
    return true;
}

#endif
