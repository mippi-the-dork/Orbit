#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitAIController.h"
#include "OrbitAIPathFollowerComponent.h"
#include "OrbitAISurfaceNavigation.h"
#include "OrbitStateTreeMoveToTask.h"
#include "Tests/AutomationCommon.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

// Baseline-only scalability tests. Deliberately no shared navigation topology cache:
// world collision, caller exclusion and capsule dimensions all affect walkability.
namespace OrbitAIScalingTests
{
    constexpr double Radius = 1000.0;
    constexpr float Dt = 1.0f / 60.0f;

    struct FAgent
    {
        ACharacter* Character = nullptr;
        AOrbitAIController* Controller = nullptr;
        UOrbitAIPathFollowerComponent* Follower = nullptr;
        FVector Initial = FVector::ZeroVector;

        void Step()
        {
            const FVector Up = Character->GetActorLocation().GetSafeNormal();
            UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
            Movement->SetGravityDirection(-Up);
            Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, Up));
            Follower->TickComponent(Dt, LEVELTICK_All, nullptr);
            Movement->TickComponent(Dt, LEVELTICK_All, nullptr);
        }
    };

    struct FFixture
    {
        FTestWorldWrapper Wrapper;
        UWorld* World = nullptr;
        TArray<FAgent> Agents;
        FOrbitSphericalNavigationSettings Settings;

        static FVector Point(double Longitude)
        {
            return FVector(Radius * FMath::Cos(Longitude), Radius * FMath::Sin(Longitude), 0.0);
        }

        bool Initialize(int32 Count)
        {
            if (!Wrapper.CreateTestWorld(EWorldType::Game)) return false;
            World = Wrapper.GetTestWorld();
            if (!World) return false;
            AActor* Planet = World->SpawnActor<AActor>();
            if (!Planet) return false;
            USphereComponent* Surface = NewObject<USphereComponent>(Planet, TEXT("OrbitScalePlanet"));
            Planet->AddInstanceComponent(Surface);
            Planet->SetRootComponent(Surface);
            Surface->InitSphereRadius(Radius);
            Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Surface->SetCollisionResponseToAllChannels(ECR_Block);
            Surface->RegisterComponent();
            Settings.LongitudeSamples = 24;
            Settings.LatitudeSamples = 13;
            // 16 agents: full obstacle-aware validation. 32/64: isolate sample/search
            // throughput without pretending those reduced-cost routes are collision-verified.
            Settings.bValidateEdges = Count <= 16;
            Agents.Reserve(Count);
            for (int32 Index = 0; Index < Count; ++Index)
            {
                FAgent Agent;
                const double Longitude = 2.0 * PI * Index / Count;
                Agent.Character = World->SpawnActor<ACharacter>();
                if (!Agent.Character) return false;
                UCharacterMovementComponent* Movement = Agent.Character->GetCharacterMovement();
                if (!Movement) return false;
                Movement->bRunPhysicsWithNoController = true;
                Movement->MaxWalkSpeed = 240.0f;
                Movement->MaxAcceleration = 1500.0f;
                Movement->BrakingDecelerationWalking = 1400.0f;
                const FVector Up = Point(Longitude).GetSafeNormal();
                Agent.Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, Up));
                Agent.Character->SetActorLocation(Up * (Radius + Agent.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0),
                    false, nullptr, ETeleportType::TeleportPhysics);
                Movement->SetGravityDirection(-Up);
                Movement->SetMovementMode(MOVE_Walking);
                Agent.Follower = NewObject<UOrbitAIPathFollowerComponent>(Agent.Character);
                Agent.Character->AddInstanceComponent(Agent.Follower);
                Agent.Follower->RegisterComponent();
                Agent.Follower->bRequireGrounded = true;
                Agent.Follower->AcceptanceRadius = 60.0f;
                // The 32/64 scale profile isolates graph throughput from crowd clearance.
                // Other AI pawns are not a navigation topology cache or crowd-avoidance system.
                Agent.Follower->bUseCharacterCapsuleForSurfaceNavigation = Count <= 16;
                Agent.Follower->MovementStallTimeout = 3.0f;
                Agent.Controller = World->SpawnActor<AOrbitAIController>();
                if (!Agent.Controller) return false;
                Agent.Controller->Possess(Agent.Character);
                if (Agent.Controller->GetPawn() != Agent.Character) return false;
                Agent.Initial = Agent.Character->GetActorLocation();
                Agents.Add(Agent);
            }
            return true;
        }
    };

    bool RunScale(FAutomationTestBase& Test, int32 Count)
    {
        FFixture F;
        if (!Test.TestTrue(TEXT("Build shared collision planet and independent possessed Characters"), F.Initialize(Count))) return false;
        int32 Accepted = 0;
        const double PlanStart = FPlatformTime::Seconds();
        for (int32 I = 0; I < Count; ++I)
        {
            const FVector Goal = FFixture::Point(2.0 * PI * I / Count + 0.25);
            if (F.Agents[I].Controller->MoveOrbitToLocation(Goal, FVector::ZeroVector, Radius, F.Settings)) ++Accepted;
        }
        const double PlanMilliseconds = (FPlatformTime::Seconds() - PlanStart) * 1000.0;
        if (!Test.TestEqual(TEXT("Every independent controller accepts a route"), Accepted, Count)) return false;
        // Replanning work and physical stepping are timed independently. Frame-time
        // instrumentation does not claim to measure a real PIE renderer or full Chaos tick.
        const double StepStart = FPlatformTime::Seconds();
        for (int32 Frame = 0; Frame < 24; ++Frame)
        {
            for (FAgent& Agent : F.Agents)
            {
                if (Agent.Follower->bFollowingPath) Agent.Step();
            }
        }
        const double StepMilliseconds = (FPlatformTime::Seconds() - StepStart) * 1000.0;
        int32 Moved = 0;
        int32 Active = 0;
        for (const FAgent& Agent : F.Agents)
        {
            if (FVector::Dist(Agent.Initial, Agent.Character->GetActorLocation()) > 3.0) ++Moved;
            if (Agent.Follower->bFollowingPath) ++Active;
        }
        UE_LOG(LogTemp, Display, TEXT("Orbit AI scale %d: graph=%dx%d edgeValidation=%d capsuleClearance=%d planning=%.3f ms (%.3f ms/agent), 24 physics-stepped frames=%.3f ms, moved=%d, active=%d"),
            Count, F.Settings.LongitudeSamples, F.Settings.LatitudeSamples, F.Settings.bValidateEdges ? 1 : 0, Count <= 16 ? 1 : 0,
            PlanMilliseconds, PlanMilliseconds / Count, StepMilliseconds, Moved, Active);
        if (!Test.TestTrue(TEXT("Most Characters physically move within the sampling window"), Moved >= Count * 3 / 4)) return false;
        // Abort a middle request after everybody has moved. Other followers retain
        // their own request IDs even when their numerical ID is identical.
        const int32 Victim = Count / 2;
        const int32 VictimRequest = F.Agents[Victim].Follower->MoveRequestId;
        if (!Test.TestTrue(TEXT("Selected agent retains active move before abort"), F.Agents[Victim].Follower->bFollowingPath)) return false;
        if (!Test.TestTrue(TEXT("Selected agent aborts own request"), F.Agents[Victim].Follower->AbortMoveRequest(VictimRequest))) return false;
        if (!Test.TestEqual(TEXT("Selected agent reports abort"), F.Agents[Victim].Follower->LastMoveResult, EOrbitAIMoveResult::Aborted)) return false;
        for (int32 I = 0; I < Count; ++I)
        {
            if (I == Victim) continue;
            if (!Test.TestTrue(FString::Printf(TEXT("Other agent %d retains independent in-progress request"), I),
                F.Agents[I].Follower->bFollowingPath)) return false;
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIScale16,
    "Orbit.AI.Scale.SixteenAgentsCollisionVerified",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAIScale16::RunTest(const FString& Parameters) { return OrbitAIScalingTests::RunScale(*this, 16); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIScale32,
    "Orbit.AI.Scale.ThirtyTwoAgentsSampleAndSteering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAIScale32::RunTest(const FString& Parameters) { return OrbitAIScalingTests::RunScale(*this, 32); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIScale64,
    "Orbit.AI.Scale.SixtyFourAgentsSampleAndSteering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAIScale64::RunTest(const FString& Parameters) { return OrbitAIScalingTests::RunScale(*this, 64); }



// Iteration 077: exercise the real packaged Blueprint path and the opt-in
// native diagnostics through identical collision and sampling settings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAILazyEdgeValidationParity,
    "Orbit.AI.Scale.LazyEdgeValidationAndNativeAPIParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAILazyEdgeValidationParity::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create isolated profile world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    if (!TestNotNull(TEXT("Profile world exists"), World)) return false;
    AActor* Planet = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Spawn spherical navigation surface"), Planet)) return false;
    USphereComponent* Surface = NewObject<USphereComponent>(Planet);
    Planet->AddInstanceComponent(Surface);
    Planet->SetRootComponent(Surface);
    Surface->InitSphereRadius(OrbitAIScalingTests::Radius);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent();

    FOrbitSphericalNavigationSettings Settings;
    Settings.LongitudeSamples = 24;
    Settings.LatitudeSamples = 13;
    Settings.bValidateEdges = true;
    const FVector Start = OrbitAIScalingTests::FFixture::Point(0.0);
    const FVector Goal = OrbitAIScalingTests::FFixture::Point(0.72);
    TArray<FVector> BlueprintRoute;
    TArray<FVector> ProfiledRoute;
    FString BlueprintFailure;
    FString ProfiledFailure;
    FOrbitSurfaceNavigationProfile Profile;
    const bool bBlueprint = UOrbitAISurfaceNavigation::FindSphericalSurfacePath(World,
        Start, Goal, FVector::ZeroVector, OrbitAIScalingTests::Radius, Settings, BlueprintRoute, BlueprintFailure);
    const bool bNative = UOrbitAISurfaceNavigation::FindSphericalSurfacePathProfiled(World,
        Start, Goal, FVector::ZeroVector, OrbitAIScalingTests::Radius, Settings, ProfiledRoute, ProfiledFailure, &Profile);
    if (!TestTrue(TEXT("Blueprint-compatible route succeeds"), bBlueprint)) return false;
    if (!TestTrue(TEXT("Profiled native route succeeds"), bNative)) return false;
    if (!TestEqual(TEXT("Native API has identical number of waypoints"), ProfiledRoute.Num(), BlueprintRoute.Num())) return false;
    for (int32 Index = 0; Index < BlueprintRoute.Num(); ++Index)
    {
        if (!TestTrue(FString::Printf(TEXT("Waypoint %d matches native API"), Index),
            ProfiledRoute[Index].Equals(BlueprintRoute[Index], 0.1))) return false;
    }
    TestTrue(TEXT("Surface samples were measured"), Profile.SampledNodes > 0);
    TestTrue(TEXT("A* exploration was measured"), Profile.ExpandedNodes > 0);
    TestTrue(TEXT("Live collision edges were validated"), Profile.EdgeChecks > 0);
    TestTrue(TEXT("Sampling time is nonnegative"), Profile.GraphSamplingMilliseconds >= 0.0);
    TestTrue(TEXT("Validation time is nonnegative"), Profile.EdgeValidationMilliseconds >= 0.0);
    UE_LOG(LogTemp, Display, TEXT("Orbit AI lazy route parity: samples=%d expanded=%d edges=%d reused=%d blocked=%d sample=%.3fms validate=%.3fms search=%.3fms"),
        Profile.SampledNodes, Profile.ExpandedNodes, Profile.EdgeChecks, Profile.ReusedEdgeChecks,
        Profile.BlockedEdges, Profile.GraphSamplingMilliseconds, Profile.EdgeValidationMilliseconds,
        Profile.SearchMilliseconds);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIVerifiedPlanningProfile,
    "Orbit.AI.Scale.VerifiedPlanningProfile16_32_64",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAIVerifiedPlanningProfile::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create isolated verified planning world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    if (!TestNotNull(TEXT("Verified planning world exists"), World)) return false;
    AActor* Planet = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Spawn verified planning surface"), Planet)) return false;
    USphereComponent* Surface = NewObject<USphereComponent>(Planet);
    Planet->AddInstanceComponent(Surface);
    Planet->SetRootComponent(Surface);
    Surface->InitSphereRadius(OrbitAIScalingTests::Radius);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent();
    FOrbitSphericalNavigationSettings Settings;
    Settings.LongitudeSamples = 24;
    Settings.LatitudeSamples = 13;
    Settings.bValidateEdges = true;
    Settings.AgentRadius = 0.0f; // Deliberate point-only baseline, not capsule parity.
    for (const int32 Count : { 16, 32, 64 })
    {
        double TotalEndpointMs = 0.0;
        double TotalSampleMs = 0.0;
        double TotalEdgeMs = 0.0;
        double TotalSearchMs = 0.0;
        double TotalConnectorMs = 0.0;
        int64 TotalEdges = 0;
        int32 Accepted = 0;
        const double BatchStart = FPlatformTime::Seconds();
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const double Angle = 2.0 * PI * Index / Count;
            const FVector Start = OrbitAIScalingTests::FFixture::Point(Angle);
            const FVector Goal = OrbitAIScalingTests::FFixture::Point(Angle + 0.42);
            TArray<FVector> Route;
            FString Failure;
            FOrbitSurfaceNavigationProfile Profile;
            const bool bFound = UOrbitAISurfaceNavigation::FindSphericalSurfacePathProfiled(
                World, Start, Goal, FVector::ZeroVector, OrbitAIScalingTests::Radius,
                Settings, Route, Failure, &Profile);
            if (!bFound)
            {
                AddError(FString::Printf(TEXT("Verified request %d/%d rejected: %s"), Index + 1, Count, *Failure));
                return false;
            }
            if (Route.Num() < 2 || Profile.EdgeChecks <= 0 || Profile.ExpandedNodes <= 0)
            {
                AddError(FString::Printf(TEXT("Verified request %d/%d lacked route/diagnostics"), Index + 1, Count));
                return false;
            }
            ++Accepted;
            TotalEndpointMs += Profile.EndpointMilliseconds;
            TotalSampleMs += Profile.GraphSamplingMilliseconds;
            TotalEdgeMs += Profile.EdgeValidationMilliseconds;
            TotalSearchMs += Profile.SearchMilliseconds;
            TotalConnectorMs += Profile.ConnectorMilliseconds;
            TotalEdges += Profile.EdgeChecks;
        }
        const double BatchMs = (FPlatformTime::Seconds() - BatchStart) * 1000.0;
        UE_LOG(LogTemp, Display, TEXT("Orbit AI verified profile %d: total=%.3fms per=%.3fms endpoint=%.3fms sampling=%.3fms edges=%.3fms search=%.3fms connectors=%.3fms checks=%lld; point-clearance only"),
            Count, BatchMs, BatchMs / Count, TotalEndpointMs, TotalSampleMs,
            TotalEdgeMs, TotalSearchMs, TotalConnectorMs, static_cast<long long>(TotalEdges));
        if (!TestEqual(TEXT("Every same-configuration verified query accepts"), Accepted, Count)) return false;
    }
    return true;
}


// Iteration 078: verify demand-sampling does not skip collision validation and
// that requests cannot reuse stale samples from prior queries.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIDemandSurfaceSampling,
    "Orbit.AI.Scale.DemandSurfaceSamplingAndRequestIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAIDemandSurfaceSampling::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create sampling world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    AActor* Planet = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Spawn planet"), Planet)) return false;
    USphereComponent* Surface = NewObject<USphereComponent>(Planet);
    Planet->AddInstanceComponent(Surface);
    Planet->SetRootComponent(Surface);
    Surface->InitSphereRadius(OrbitAIScalingTests::Radius);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent();
    FOrbitSphericalNavigationSettings Settings;
    Settings.LongitudeSamples = 24;
    Settings.LatitudeSamples = 13;
    Settings.bValidateEdges = true;
    Settings.AgentRadius = 42.0f;
    Settings.AgentHalfHeight = 96.0f;
    const FVector Start = OrbitAIScalingTests::FFixture::Point(0.14);
    const FVector Goal = OrbitAIScalingTests::FFixture::Point(0.67);
    FOrbitSurfaceNavigationProfile First;
    TArray<FVector> Route;
    FString Failure;
    if (!TestTrue(TEXT("Capsule-verified path succeeds"),
        UOrbitAISurfaceNavigation::FindSphericalSurfacePathProfiled(World, Start, Goal,
            FVector::ZeroVector, OrbitAIScalingTests::Radius, Settings, Route, Failure, &First))) return false;
    const int32 FullGraph = Settings.LongitudeSamples * Settings.LatitudeSamples;
    TestTrue(TEXT("Samples only a subset of available nodes"),
        First.AttemptedNodeSamples > 0 && First.AttemptedNodeSamples < FullGraph / 2);
    TestTrue(TEXT("Every successful sample was attempted"),
        First.SampledNodes <= First.AttemptedNodeSamples);
    TestTrue(TEXT("Real graph edges were checked"), First.EdgeChecks > 0);
    FOrbitSurfaceNavigationProfile Second;
    TArray<FVector> SecondRoute;
    FString SecondFailure;
    if (!TestTrue(TEXT("Independent second request succeeds"),
        UOrbitAISurfaceNavigation::FindSphericalSurfacePathProfiled(World, Start, Goal,
            FVector::ZeroVector, OrbitAIScalingTests::Radius, Settings,
            SecondRoute, SecondFailure, &Second))) return false;
    TestEqual(TEXT("Identical requests attempt the same number of nodes"),
        First.AttemptedNodeSamples, Second.AttemptedNodeSamples);
    TestEqual(TEXT("Second request executes its own traces, not a global cache"),
        First.SampledNodes, Second.SampledNodes);
    TestEqual(TEXT("Routes retain identical topology"), Route.Num(), SecondRoute.Num());
    UE_LOG(LogTemp, Display, TEXT("Orbit demand surface sampling: attempted=%d of %d reused=%d valid=%d edges=%d sampled=%.3fms"),
        First.AttemptedNodeSamples, FullGraph, First.ReusedNodeSamples, First.SampledNodes,
        First.EdgeChecks, First.GraphSamplingMilliseconds);
    return true;
}

// Unlike the previous 32/64 reduced-clearance fixture, these three workloads
// all use matching edge validation AND Character-sized capsule clearance.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAICapsuleVerifiedPlanningProfile,
    "Orbit.AI.Scale.CapsuleVerifiedPlanningProfile16_32_64",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAICapsuleVerifiedPlanningProfile::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create capsule planning world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    AActor* Planet = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Spawn planet"), Planet)) return false;
    USphereComponent* Surface = NewObject<USphereComponent>(Planet);
    Planet->AddInstanceComponent(Surface);
    Planet->SetRootComponent(Surface);
    Surface->InitSphereRadius(OrbitAIScalingTests::Radius);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent();
    FOrbitSphericalNavigationSettings Settings;
    Settings.LongitudeSamples = 24;
    Settings.LatitudeSamples = 13;
    Settings.bValidateEdges = true;
    Settings.AgentRadius = 42.0f;
    Settings.AgentHalfHeight = 96.0f;
    Settings.SurfaceSkin = 3.0f;
    for (const int32 Count : { 16, 32, 64 })
    {
        int32 Accepted = 0;
        int64 Samples = 0;
        int64 EdgeChecks = 0;
        double SampleMs = 0.0;
        double EdgeMs = 0.0;
        const double StartTime = FPlatformTime::Seconds();
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const double Angle = 2.0 * PI * Index / Count;
            const FVector Start = OrbitAIScalingTests::FFixture::Point(Angle);
            const FVector Goal = OrbitAIScalingTests::FFixture::Point(Angle + 0.42);
            TArray<FVector> Route;
            FString Failure;
            FOrbitSurfaceNavigationProfile Profile;
            if (!UOrbitAISurfaceNavigation::FindSphericalSurfacePathProfiled(World, Start, Goal,
                FVector::ZeroVector, OrbitAIScalingTests::Radius, Settings, Route, Failure, &Profile))
            {
                AddError(FString::Printf(TEXT("Capsule request %d/%d: %s"), Index + 1, Count, *Failure));
                return false;
            }
            if (Route.Num() < 2 || Profile.EdgeChecks <= 0)
            {
                AddError(FString::Printf(TEXT("Capsule request %d/%d missing verified legs"), Index + 1, Count));
                return false;
            }
            ++Accepted;
            Samples += Profile.AttemptedNodeSamples;
            EdgeChecks += Profile.EdgeChecks;
            SampleMs += Profile.GraphSamplingMilliseconds;
            EdgeMs += Profile.EdgeValidationMilliseconds;
        }
        const double TotalMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
        UE_LOG(LogTemp, Display, TEXT("Orbit AI capsule-verified %d: total=%.3fms per=%.3fms sample=%.3fms edge=%.3fms nodes=%lld edges=%lld radius=42 halfheight=96"),
            Count, TotalMs, TotalMs / Count, SampleMs, EdgeMs,
            static_cast<long long>(Samples), static_cast<long long>(EdgeChecks));
        if (!TestEqual(TEXT("All capsule-verified queries pass"), Accepted, Count)) return false;
    }
    return true;
}


// Native StateTree adapter contract: the task owns only its original request,
// and interprets native follower completion without a second movement system.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIStateTreeMoveToContract,
    "Orbit.AI.Native.StateTreeTaskSchemaAndRequestOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAIStateTreeMoveToContract::RunTest(const FString& Parameters)
{
    FOrbitStateTreeMoveToTask Task;
    if (!TestTrue(TEXT("StateTree node uses its own native instance data"),
        Task.GetInstanceDataType() == FOrbitStateTreeMoveToInstanceData::StaticStruct())) return false;
    FOrbitStateTreeMoveToInstanceData Data;
    TestTrue(TEXT("Bindable controller defaults to null"), Data.Controller == nullptr);
    TestEqual(TEXT("No owned request by default"), Data.OwnedRequestId, 0);
    UOrbitAIPathFollowerComponent* Follower = NewObject<UOrbitAIPathFollowerComponent>(GetTransientPackage());
    if (!TestNotNull(TEXT("Create isolated native follower"), Follower)) return false;
    const int32 RequestId = 123;
    Follower->MoveRequestId = RequestId;
    Follower->bFollowingPath = true;
    Follower->LastMoveResult = EOrbitAIMoveResult::InProgress;
    TestEqual(TEXT("Active request is running"),
        Task.EvaluateRequest(Follower, RequestId), EStateTreeRunStatus::Running);
    Follower->bFollowingPath = false;
    Follower->LastMoveResult = EOrbitAIMoveResult::Succeeded;
    TestEqual(TEXT("Arrival becomes StateTree success"),
        Task.EvaluateRequest(Follower, RequestId), EStateTreeRunStatus::Succeeded);
    Follower->LastMoveResult = EOrbitAIMoveResult::Aborted;
    TestEqual(TEXT("Abort becomes StateTree failure"),
        Task.EvaluateRequest(Follower, RequestId), EStateTreeRunStatus::Failed);
    Follower->LastMoveResult = EOrbitAIMoveResult::InProgress;
    Follower->bFollowingPath = true;
    Follower->MoveRequestId = RequestId + 1;
    TestEqual(TEXT("Replacement request is never adopted"),
        Task.EvaluateRequest(Follower, RequestId), EStateTreeRunStatus::Failed);
    TestEqual(TEXT("Null follower cannot report success"),
        Task.EvaluateRequest(nullptr, RequestId), EStateTreeRunStatus::Failed);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIGravityRelativeQuery,
    "Orbit.AI.Native.GravityRelativeQueryProjectionAndDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAIGravityRelativeQuery::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create spherical query world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    AActor* Planet = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Spawn query surface"), Planet)) return false;
    USphereComponent* Surface = NewObject<USphereComponent>(Planet);
    Planet->AddInstanceComponent(Surface);
    Planet->SetRootComponent(Surface);
    Surface->InitSphereRadius(OrbitAIScalingTests::Radius);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent();
    FOrbitSphericalNavigationSettings Settings;
    FVector Projected;
    const FVector Candidate(3000.0, 0.0, 0.0);
    if (!TestTrue(TEXT("World-direction query projects to actual physical sphere"),
        UOrbitAISurfaceNavigation::ProjectToSphericalSurface(World, Candidate,
            FVector::ZeroVector, OrbitAIScalingTests::Radius, Settings, Projected))) return false;
    TestTrue(TEXT("Projected radius matches physical collision"),
        FMath::IsNearlyEqual(Projected.Size(), OrbitAIScalingTests::Radius, 1.0));
    const double QuarterArc = UOrbitAISurfaceNavigation::SphericalArcDistance(
        FVector(1000.0, 0.0, 0.0), FVector(0.0, 1000.0, 0.0),
        FVector::ZeroVector, OrbitAIScalingTests::Radius);
    TestTrue(TEXT("Spherical arc is a quarter-circumference"),
        FMath::IsNearlyEqual(QuarterArc, HALF_PI * OrbitAIScalingTests::Radius, 0.01));
    TestTrue(TEXT("Candidate at gravity center is rejected"),
        !UOrbitAISurfaceNavigation::ProjectToSphericalSurface(World, FVector::ZeroVector,
            FVector::ZeroVector, OrbitAIScalingTests::Radius, Settings, Projected));
    TestTrue(TEXT("Invalid arc input never silently returns zero"),
        UOrbitAISurfaceNavigation::SphericalArcDistance(FVector::ZeroVector,
            FVector(1000.0, 0.0, 0.0), FVector::ZeroVector,
            OrbitAIScalingTests::Radius) < 0.0);
    return true;
}

#endif
