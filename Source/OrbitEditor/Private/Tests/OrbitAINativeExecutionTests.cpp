#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "OrbitAIBlueprintLibrary.h"
#include "OrbitAIPathFollowerComponent.h"
#include "OrbitStateTreeMoveToTask.h"
#include "EnvQueryGenerator_OrbitSurfacePoints.h"
#include "EnvQueryTest_OrbitSphericalDistance.h"

#include "Tests/AutomationCommon.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Engine/World.h"

namespace OrbitNativeExecution
{
    struct FPlanetFixture
    {
        FTestWorldWrapper Wrapper;
        UWorld* World = nullptr;
        AActor* Querier = nullptr;
        bool Initialize()
        {
            if (!Wrapper.CreateTestWorld(EWorldType::Game)) return false;
            World = Wrapper.GetTestWorld();
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
            Querier = World->SpawnActor<AActor>();
            if (!Querier) return false;
            USceneComponent* Root = NewObject<USceneComponent>(Querier);
            Querier->AddInstanceComponent(Root);
            Querier->SetRootComponent(Root);
            Root->RegisterComponent();
            Querier->SetActorLocation(FVector(1100, 0, 0));
            return true;
        }
        UEnvQuery* MakeQuery(bool bLocal, bool bProject, bool bDistanceTest,
            int32 Samples = 32, float Angle = 35.0f)
        {
            UEnvQuery* Query = NewObject<UEnvQuery>(GetTransientPackage());
            UEnvQueryOption* Option = NewObject<UEnvQueryOption>(Query);
            UEnvQueryGenerator_OrbitSurfacePoints* Generator =
                NewObject<UEnvQueryGenerator_OrbitSurfacePoints>(Option);
            Generator->SurfaceRadius = 1000.0f;
            Generator->SampleCount = Samples;
            Generator->bLimitToQuerierRegion = bLocal;
            Generator->QuerierRegionAngleDegrees = Angle;
            Generator->bProjectToWalkableSurface = bProject;
            Option->Generator = Generator;
            if (bDistanceTest)
            {
                UEnvQueryTest_OrbitSphericalDistance* Test =
                    NewObject<UEnvQueryTest_OrbitSphericalDistance>(Option);
                Test->SurfaceRadius = 1000.0f;
                Option->Tests.Add(Test);
            }
            Query->GetOptionsMutable().Add(Option);
            return Query;
        }
        // The automation test world's physics and collision scene is usable without
        // an AI system. EQS is different: its manager belongs to the world's AI
        // system, which the minimal FTestWorldWrapper does not guarantee.
        UEnvQueryManager* EnsureQueryManager()
        {
            if (!IsValid(World)) return nullptr;
            if (!World->GetAISystem())
            {
                World->CreateAISystem();
            }
            return UEnvQueryManager::GetCurrent(World);
        }

        TSharedPtr<FEnvQueryResult> Run(UEnvQuery* Query)
        {
            UEnvQueryManager* Manager = EnsureQueryManager();
            return Manager && IsValid(Query) && IsValid(Querier)
                ? Manager->RunInstantQuery(FEnvQueryRequest(Query, Querier), EEnvQueryRunMode::AllMatching)
                : nullptr;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEQSWorldAISystemLifecycle,
    "Orbit.AI.Native.LiveEQS.WorldAISystemLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEQSWorldAISystemLifecycle::RunTest(const FString& Parameters)
{
    using namespace OrbitNativeExecution;
    FPlanetFixture F;
    if (!TestTrue(TEXT("Create isolated sphere world"), F.Initialize())) return false;
    UEnvQueryManager* Manager = F.EnsureQueryManager();
    if (!TestNotNull(TEXT("Create/find native EQS manager in isolated Game world"), Manager)) return false;
    TestTrue(TEXT("Repeated manager access preserves native manager instance"),
        F.EnsureQueryManager() == Manager);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEQSActualQueryFullSphere,
    "Orbit.AI.Native.LiveEQS.FullSphereGenerationAndDistanceScoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEQSActualQueryFullSphere::RunTest(const FString& Parameters)
{
    using namespace OrbitNativeExecution;
    FPlanetFixture F;
    if (!TestTrue(TEXT("Create native EQS query world"), F.Initialize())) return false;
    UEnvQuery* Query = F.MakeQuery(false, false, true, 32);
    if (!TestNotNull(TEXT("Construct EQS asset with Orbit generator and scoring test"), Query)) return false;
    const UEnvQueryTest_OrbitSphericalDistance* DistanceNode =
        Cast<UEnvQueryTest_OrbitSphericalDistance>(Query->GetOptions()[0]->Tests[0]);
    if (!TestNotNull(TEXT("Native distance scoring node is attached to authored query"), DistanceNode)) return false;
    if (!TestNotNull(TEXT("Isolated world has a native EQS manager"), F.EnsureQueryManager())) return false;
    TSharedPtr<FEnvQueryResult> Result = F.Run(Query);
    if (!TestTrue(TEXT("Native EQS manager executed query and returned a result (check EQS query compilation)"), Result.IsValid())) return false;
    if (!TestTrue(TEXT("Engine finished Orbit EQS query successfully"), Result->IsSuccessful())) return false;
    TArray<FVector> Locations;
    Result->GetAllAsLocations(Locations);
    if (!TestEqual(TEXT("Unprojected full sphere returns every generated candidate"), Locations.Num(), 32)) return false;
    bool bHasFarCandidate = false;
    bool bHasNearCandidate = false;
    for (const FVector& Location : Locations)
    {
        if (!TestTrue(TEXT("All items remain on the authored sphere"),
            FMath::IsNearlyEqual(Location.Size(), 1000.0, 0.5))) return false;
        const float Arc = UEnvQueryTest_OrbitSphericalDistance::CalculateDistance(
            Location, F.Querier->GetActorLocation(), FVector::ZeroVector, 1000.0f);
        bHasFarCandidate |= Arc > 1700.0f;
        bHasNearCandidate |= Arc < 600.0f;
    }
    TestTrue(TEXT("Actual query covers distant hemisphere"), bHasFarCandidate);
    TestTrue(TEXT("Actual query covers nearby hemisphere"), bHasNearCandidate);
    TestTrue(TEXT("Scoring remains finite"), FMath::IsFinite(Result->GetItemScore(0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEQSActualLocalProjectedQuery,
    "Orbit.AI.Native.LiveEQS.LocalCollisionProjectionAndMovingQuerier",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEQSActualLocalProjectedQuery::RunTest(const FString& Parameters)
{
    using namespace OrbitNativeExecution;
    FPlanetFixture F;
    if (!TestTrue(TEXT("Create physical planet query world"), F.Initialize())) return false;
    UEnvQuery* Query = F.MakeQuery(true, true, true, 48, 28.0f);
    if (!TestNotNull(TEXT("Create local collision EQS asset"), Query)) return false;
    if (!TestNotNull(TEXT("Local query world has a native EQS manager"), F.EnsureQueryManager())) return false;
    for (const FVector& QuerierPosition : {FVector(1100, 0, 0), FVector(0, -1100, 0), FVector(0, 0, 1100)})
    {
        F.Querier->SetActorLocation(QuerierPosition);
        TSharedPtr<FEnvQueryResult> Result = F.Run(Query);
        if (!TestTrue(TEXT("Engine returned a local collision query result (check EQS query compilation)"), Result.IsValid())) return false;
        if (!TestTrue(TEXT("Local collision query finished successfully (check generation and scoring)"), Result->IsSuccessful())) return false;
        TArray<FVector> Locations;
        Result->GetAllAsLocations(Locations);
        if (!TestTrue(TEXT("Each querier orientation yields collision-checked points"),
            Locations.Num() > 0 && Locations.Num() <= 48)) return false;
        const FVector Radial = QuerierPosition.GetSafeNormal();
        for (const FVector& Location : Locations)
        {
            if (!TestTrue(TEXT("Collision result lies on planet"),
                FMath::IsNearlyEqual(Location.Size(), 1000.0, 2.5))) return false;
            if (!TestTrue(TEXT("Query result follows moving querier spherical cap"),
                FVector::DotProduct(Location.GetSafeNormal(), Radial) >=
                FMath::Cos(FMath::DegreesToRadians(29.0f)))) return false;
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitStateTreeActualFollowerLifecycle,
    "Orbit.AI.Native.StateTreePhysicsSteppedRequestCompletionAndReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitStateTreeActualFollowerLifecycle::RunTest(const FString& Parameters)
{
    using namespace OrbitNativeExecution;
    FPlanetFixture F;
    if (!TestTrue(TEXT("Create physical planet for StateTree-owned movement"), F.Initialize())) return false;
    ACharacter* Character = F.World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("Spawn actual Character"), Character)) return false;
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    Movement->bRunPhysicsWithNoController = true;
    Movement->MaxWalkSpeed = 240.0f;
    Movement->MaxAcceleration = 1500.0f;
    Movement->BrakingDecelerationWalking = 1400.0f;
    Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, FVector::ForwardVector));
    Character->SetActorLocation(FVector(1000.0 + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0, 0, 0),
        false, nullptr, ETeleportType::TeleportPhysics);
    Movement->SetGravityDirection(-FVector::ForwardVector);
    Movement->SetMovementMode(MOVE_Walking);
    UOrbitAIPathFollowerComponent* Follower = NewObject<UOrbitAIPathFollowerComponent>(Character);
    Character->AddInstanceComponent(Follower);
    Follower->RegisterComponent();
    Follower->AcceptanceRadius = 60.0f;
    Follower->bRequireGrounded = true;
    AAIController* Controller = F.World->SpawnActor<AAIController>();
    if (!TestNotNull(TEXT("Spawn standard AI Controller"), Controller)) return false;
    Controller->Possess(Character);
    FOrbitSphericalNavigationSettings Settings;
    Settings.LongitudeSamples = 24;
    Settings.LatitudeSamples = 13;
    int32 FirstId = 0;
    const FVector Goal(1000.0 * FMath::Cos(PI / 5.0), 1000.0 * FMath::Sin(PI / 5.0), 0);
    if (!TestTrue(TEXT("Generic controller starts Orbit move owned by StateTree adapter"),
        UOrbitAIBlueprintLibrary::MoveToLocation(Controller, Goal, FVector::ZeroVector,
            1000.0f, Settings, FirstId))) return false;
    TestEqual(TEXT("Task evaluates actual in-flight request"),
        FOrbitStateTreeMoveToTask::EvaluateRequest(Follower, FirstId), EStateTreeRunStatus::Running);
    for (int32 Frame = 0; Frame < 660 && Follower->bFollowingPath; ++Frame)
    {
        const FVector Up = Character->GetActorLocation().GetSafeNormal();
        Movement->SetGravityDirection(-Up);
        Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, Up));
        Follower->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
        Movement->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    }
    TestEqual(TEXT("StateTree adapter sees actual movement arrival"),
        FOrbitStateTreeMoveToTask::EvaluateRequest(Follower, FirstId), EStateTreeRunStatus::Succeeded);
    int32 ReplacementId = 0;
    if (!TestTrue(TEXT("Blueprint starts independent replacement move"),
        UOrbitAIBlueprintLibrary::MoveToLocation(Controller,
            FVector(1000.0 * FMath::Cos(0.8), 1000.0 * FMath::Sin(0.8), 0),
            FVector::ZeroVector, 1000.0f, Settings, ReplacementId))) return false;
    if (!TestTrue(TEXT("Replacement ID differs from completed StateTree request"), ReplacementId != FirstId)) return false;
    TestEqual(TEXT("Old StateTree task must fail once request is superseded"),
        FOrbitStateTreeMoveToTask::EvaluateRequest(Follower, FirstId), EStateTreeRunStatus::Failed);
    TestFalse(TEXT("Stale request cannot abort replacement"), Follower->AbortMoveRequest(FirstId));
    TestEqual(TEXT("Replacement is still independently running"),
        FOrbitStateTreeMoveToTask::EvaluateRequest(Follower, ReplacementId), EStateTreeRunStatus::Running);
    TestTrue(TEXT("Active request can be aborted by its owner"), Follower->AbortMoveRequest(ReplacementId));
    TestEqual(TEXT("Aborted movement reports failure to StateTree"),
        FOrbitStateTreeMoveToTask::EvaluateRequest(Follower, ReplacementId), EStateTreeRunStatus::Failed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEQSBlueprintBridgeRejectsInvalid,
    "Orbit.AI.Native.LiveEQS.BlueprintBridgeRejectsInvalidWithoutInterruption",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEQSBlueprintBridgeRejectsInvalid::RunTest(const FString& Parameters)
{
    int32 RequestId = 999;
    FOrbitSphericalNavigationSettings Settings;
    TestFalse(TEXT("Missing EQS result does not initiate any movement"),
        UOrbitAIBlueprintLibrary::MoveToBestEQSLocation(nullptr, nullptr,
            FVector::ZeroVector, 1000.0f, Settings, RequestId));
    TestEqual(TEXT("Failed EQS bridge returns no request identity"), RequestId, 0);
    return true;
}

#endif
