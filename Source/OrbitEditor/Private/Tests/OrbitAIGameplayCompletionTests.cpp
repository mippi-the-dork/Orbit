#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitAIController.h"
#include "OrbitAIPathFollowerComponent.h"
#include "Tests/AutomationCommon.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

namespace OrbitAIGameplayTests
{
    constexpr float StepSeconds = 1.0f / 60.0f;
    constexpr double PlanetRadius = 1000.0;

    struct FAgent
    {
        ACharacter* Character = nullptr;
        AOrbitAIController* Controller = nullptr;
        UOrbitAIPathFollowerComponent* Follower = nullptr;
        UCharacterMovementComponent* Movement = nullptr;
        FVector StartLocation = FVector::ZeroVector;

        void Step()
        {
            // Keep the real native CharacterMovement collision/locomotion active;
            // isolate steering from the unrelated gravity-source registration path.
            const FVector Up = Character->GetActorLocation().GetSafeNormal();
            Movement->SetGravityDirection(-Up);
            Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, Up));
            Follower->TickComponent(StepSeconds, LEVELTICK_All, nullptr);
            Movement->TickComponent(StepSeconds, LEVELTICK_All, nullptr);
        }
    };

    struct FFixture
    {
        FTestWorldWrapper Wrapper;
        UWorld* World = nullptr;
        FOrbitSphericalNavigationSettings Settings;
        TArray<FAgent> Agents;

        static FVector Point(double Angle)
        {
            return FVector(PlanetRadius * FMath::Cos(Angle), PlanetRadius * FMath::Sin(Angle), 0.0);
        }

        bool Initialize()
        {
            if (!Wrapper.CreateTestWorld(EWorldType::Game)) return false;
            World = Wrapper.GetTestWorld();
            if (!World) return false;
            AActor* Planet = World->SpawnActor<AActor>();
            if (!Planet) return false;
            USphereComponent* Surface = NewObject<USphereComponent>(Planet, TEXT("OrbitAICompletionPlanet"));
            Planet->AddInstanceComponent(Surface);
            Planet->SetRootComponent(Surface);
            Surface->InitSphereRadius(PlanetRadius);
            Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Surface->SetCollisionResponseToAllChannels(ECR_Block);
            Surface->RegisterComponent();
            Settings.LongitudeSamples = 24;
            Settings.LatitudeSamples = 13;
            return true;
        }

        FAgent* AddAgent(double StartAngle)
        {
            FAgent Agent;
            Agent.Character = World->SpawnActor<ACharacter>();
            if (!Agent.Character) return nullptr;
            Agent.Movement = Agent.Character->GetCharacterMovement();
            if (!Agent.Movement) return nullptr;
            Agent.Movement->bRunPhysicsWithNoController = true;
            Agent.Movement->MaxWalkSpeed = 240.0f;
            Agent.Movement->MaxAcceleration = 1500.0f;
            Agent.Movement->BrakingDecelerationWalking = 1400.0f;
            const FVector Up = Point(StartAngle).GetSafeNormal();
            Agent.Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, Up));
            Agent.Character->SetActorLocation(Up * (PlanetRadius + Agent.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0),
                false, nullptr, ETeleportType::TeleportPhysics);
            Agent.Movement->SetGravityDirection(-Up);
            Agent.Movement->SetMovementMode(MOVE_Walking);
            Agent.Follower = NewObject<UOrbitAIPathFollowerComponent>(Agent.Character);
            Agent.Character->AddInstanceComponent(Agent.Follower);
            Agent.Follower->RegisterComponent();
            Agent.Follower->AcceptanceRadius = 60.0f;
            Agent.Follower->bRequireGrounded = true;
            Agent.Follower->MovementStallTimeout = 3.0f;
            Agent.Controller = World->SpawnActor<AOrbitAIController>();
            if (!Agent.Controller) return nullptr;
            Agent.Controller->Possess(Agent.Character);
            if (Agent.Controller->GetPawn() != Agent.Character) return nullptr;
            Agent.StartLocation = Agent.Character->GetActorLocation();
            Agents.Add(Agent);
            return &Agents.Last();
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIControllerPhysicalArrival,
    "Orbit.AI.Gameplay.PhysicsSteppedControllerArrival",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAIControllerPhysicalArrival::RunTest(const FString& Parameters)
{
    using namespace OrbitAIGameplayTests;
    FFixture F;
    if (!TestTrue(TEXT("Create native spherical collision world"), F.Initialize())) return false;
    FAgent* Agent = F.AddAgent(0.0);
    if (!TestNotNull(TEXT("Possess native Character"), Agent)) return false;
    if (!TestTrue(TEXT("Controller requests obstacle-aware planet route"),
        Agent->Controller->MoveOrbitToLocation(FFixture::Point(PI / 5.0), FVector::ZeroVector, PlanetRadius, F.Settings))) return false;
    const int32 RequestId = Agent->Follower->MoveRequestId;
    for (int32 Frame = 0; Frame < 660 && Agent->Follower->bFollowingPath; ++Frame) Agent->Step();
    TestTrue(TEXT("Character physically traversed planet"),
        FVector::Dist(Agent->StartLocation, Agent->Character->GetActorLocation()) > 150.0);
    TestFalse(FString::Printf(TEXT("Controller movement completed: %s"), *Agent->Follower->LastPathStatus), Agent->Follower->bFollowingPath);
    TestEqual(TEXT("Arrival retains owning request ID"), Agent->Follower->MoveRequestId, RequestId);
    TestEqual(TEXT("Physical destination is success"), Agent->Follower->LastMoveResult, EOrbitAIMoveResult::Succeeded);
    TestEqual(TEXT("Controller diagnostics agree on success"), Agent->Controller->GetOrbitMoveDiagnostics().Result, EOrbitAIMoveResult::Succeeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIMultipleCharacterIsolation,
    "Orbit.AI.Gameplay.MultipleAgentsIndependentRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAIMultipleCharacterIsolation::RunTest(const FString& Parameters)
{
    using namespace OrbitAIGameplayTests;
    FFixture F;
    if (!TestTrue(TEXT("Create shared spherical collision world"), F.Initialize())) return false;
    constexpr int32 AgentCount = 3;
    const double Starts[AgentCount] = { 0.0, 1.65, -1.65 };
    for (int32 Index = 0; Index < AgentCount; ++Index)
    {
        FAgent* Agent = F.AddAgent(Starts[Index]);
        if (!TestNotNull(TEXT("Spawn separate Character/Controller pair"), Agent)) return false;
        if (!TestTrue(TEXT("Each controller accepts independent route"),
            Agent->Controller->MoveOrbitToLocation(FFixture::Point(Starts[Index] + 0.48), FVector::ZeroVector, PlanetRadius, F.Settings))) return false;
    }
    const int32 FirstRequest = F.Agents[0].Follower->MoveRequestId;
    const int32 OtherRequest = F.Agents[1].Follower->MoveRequestId;
    // The same numeric request ID on different followers is legal: identity is
    // follower-scoped, never globally shared across all AI Characters.
    if (!TestTrue(TEXT("First agent aborts only its own request"),
        F.Agents[0].Follower->AbortMoveRequest(FirstRequest))) return false;
    TestFalse(TEXT("First agent stopped"), F.Agents[0].Follower->bFollowingPath);
    TestTrue(TEXT("Second agent remains active"), F.Agents[1].Follower->bFollowingPath);
    TestEqual(TEXT("Second agent request unchanged"), F.Agents[1].Follower->MoveRequestId, OtherRequest);
    TestTrue(TEXT("Third agent remains active"), F.Agents[2].Follower->bFollowingPath);
    for (int32 Frame = 0; Frame < 660 && (F.Agents[1].Follower->bFollowingPath || F.Agents[2].Follower->bFollowingPath); ++Frame)
    {
        // Deterministic frame interleave, each with its own native movement and
        // gravity direction, against the SAME planet collision world.
        for (int32 Index = 1; Index < AgentCount; ++Index)
        {
            if (F.Agents[Index].Follower->bFollowingPath) F.Agents[Index].Step();
        }
    }
    for (int32 Index = 1; Index < AgentCount; ++Index)
    {
        const FAgent& Agent = F.Agents[Index];
        TestTrue(FString::Printf(TEXT("Agent %d physically moved"), Index),
            FVector::Dist(Agent.StartLocation, Agent.Character->GetActorLocation()) > 100.0);
        TestFalse(FString::Printf(TEXT("Agent %d completed route (%s)"), Index, *Agent.Follower->LastPathStatus), Agent.Follower->bFollowingPath);
        TestEqual(FString::Printf(TEXT("Agent %d reached destination"), Index), Agent.Follower->LastMoveResult, EOrbitAIMoveResult::Succeeded);
    }
    TestEqual(TEXT("Aborted agent remains aborted"), F.Agents[0].Follower->LastMoveResult, EOrbitAIMoveResult::Aborted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIPhysicalMovingGoal,
    "Orbit.AI.Gameplay.MovingTargetPhysicalReplanAndLoss",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAIPhysicalMovingGoal::RunTest(const FString& Parameters)
{
    using namespace OrbitAIGameplayTests;
    FFixture F;
    if (!TestTrue(TEXT("Create physical chase world"), F.Initialize())) return false;
    FAgent* Agent = F.AddAgent(0.0);
    if (!TestNotNull(TEXT("Create chasing Character"), Agent)) return false;
    AActor* Target = F.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Create movable target Actor"), Target)) return false;
    USceneComponent* Root = NewObject<USceneComponent>(Target);
    Target->AddInstanceComponent(Root);
    Target->SetRootComponent(Root);
    Root->RegisterComponent();
    Target->SetActorLocation(FFixture::Point(0.65));
    Agent->Follower->MovingTargetReplanDistance = 35.0f;
    if (!TestTrue(TEXT("Controller starts actual Actor chase"),
        Agent->Controller->MoveOrbitToActor(Target, FVector::ZeroVector, PlanetRadius, F.Settings))) return false;
    const int32 Request = Agent->Follower->MoveRequestId;
    for (int32 Frame = 0; Frame < 80 && Agent->Follower->bFollowingPath; ++Frame) Agent->Step();
    const double MovedBefore = FVector::Dist(Agent->StartLocation, Agent->Character->GetActorLocation());
    if (!TestTrue(TEXT("Chaser physically moved before target relocation"), MovedBefore > 30.0)) return false;
    Target->SetActorLocation(FFixture::Point(0.95));
    Agent->Step();
    TestTrue(TEXT("Moving target triggers surface path replan"), Agent->Follower->SurfaceReplanCount > 0);
    TestEqual(TEXT("Target replan retains request identity"), Agent->Follower->MoveRequestId, Request);
    for (int32 Frame = 0; Frame < 60 && Agent->Follower->bFollowingPath; ++Frame) Agent->Step();
    TestTrue(TEXT("Chaser keeps moving after replan"),
        FVector::Dist(Agent->StartLocation, Agent->Character->GetActorLocation()) > MovedBefore + 30.0);
    Target->Destroy();
    Agent->Follower->TickComponent(StepSeconds, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Destroyed goal terminates active chase"), Agent->Follower->bFollowingPath);
    TestEqual(TEXT("Target loss reports failure"), Agent->Follower->LastMoveResult, EOrbitAIMoveResult::Failed);
    TestEqual(TEXT("Target loss cannot cancel another request"), Agent->Follower->MoveRequestId, Request);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIDynamicBlockerRecovery,
    "Orbit.AI.Gameplay.DynamicBlockerRemovalRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAIDynamicBlockerRecovery::RunTest(const FString& Parameters)
{
    using namespace OrbitAIGameplayTests;
    FFixture F;
    if (!TestTrue(TEXT("Create shared planet for runtime blocker recovery"), F.Initialize())) return false;
    FAgent* Agent = F.AddAgent(0.0);
    if (!TestNotNull(TEXT("Spawn physical Character"), Agent)) return false;
    Agent->Follower->bReplanBlockedSurfaceRoutes = false; // Explicit obstacle state transitions.
    if (!TestTrue(TEXT("Start capsule-verified route to distant planet position"),
        Agent->Controller->MoveOrbitToLocation(FFixture::Point(0.95), FVector::ZeroVector, PlanetRadius, F.Settings))) return false;
    const int32 Request = Agent->Follower->MoveRequestId;
    for (int32 Frame = 0; Frame < 65 && Agent->Follower->bFollowingPath; ++Frame) Agent->Step();
    const FVector BeforeBlocker = Agent->Character->GetActorLocation();
    if (!TestTrue(TEXT("Character moved before obstacle appears"),
        FVector::Dist(Agent->StartLocation, BeforeBlocker) > 30.0)) return false;

    AActor* BlockerActor = F.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Create dynamic obstacle Actor"), BlockerActor)) return false;
    USphereComponent* Blocker = NewObject<USphereComponent>(BlockerActor, TEXT("OrbitDynamicBlocker"));
    BlockerActor->AddInstanceComponent(Blocker);
    BlockerActor->SetRootComponent(Blocker);
    Blocker->InitSphereRadius(135.0f);
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Blocker->SetCollisionResponseToAllChannels(ECR_Block);
    Blocker->RegisterComponent();
    BlockerActor->SetActorLocation(FFixture::Point(0.55));
    if (!TestTrue(FString::Printf(TEXT("Route can detour when blocker appears (%s)"), *Agent->Follower->LastPathStatus),
        Agent->Follower->ReplanSphericalSurfacePathNow())) return false;
    if (!TestEqual(TEXT("First replan retains active request"), Agent->Follower->MoveRequestId, Request)) return false;
    if (!TestEqual(TEXT("First collision replan recorded"), Agent->Follower->SurfaceReplanCount, 1)) return false;

    // Removing the obstruction must not leave stale collision topology in a
    // shared cache. The same request can recompute against the new world state.
    Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BlockerActor->Destroy();
    if (!TestTrue(FString::Printf(TEXT("Removed obstacle permits fresh verified route (%s)"), *Agent->Follower->LastPathStatus),
        Agent->Follower->ReplanSphericalSurfacePathNow())) return false;
    if (!TestEqual(TEXT("Second replan also preserves owner"), Agent->Follower->MoveRequestId, Request)) return false;
    TestEqual(TEXT("Both obstacle-state changes generated a fresh route"), Agent->Follower->SurfaceReplanCount, 2);
    for (int32 Frame = 0; Frame < 660 && Agent->Follower->bFollowingPath; ++Frame) Agent->Step();
    TestFalse(FString::Printf(TEXT("No stale blocker prevents route completion (%s)"), *Agent->Follower->LastPathStatus),
        Agent->Follower->bFollowingPath);
    TestEqual(TEXT("Physical arrival after obstacle removal succeeds"), Agent->Follower->LastMoveResult, EOrbitAIMoveResult::Succeeded);
    return true;
}

#endif
