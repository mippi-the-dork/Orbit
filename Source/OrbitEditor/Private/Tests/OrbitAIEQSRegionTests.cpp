#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
#include "EnvQueryGenerator_OrbitSurfacePoints.h"
#include "EnvQueryTest_OrbitSphericalDistance.h"
#include "OrbitAIBlueprintLibrary.h"
#include "OrbitAIPathFollowerComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "OrbitAISurfaceNavigation.h"
#include "AIController.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEQSLocalRegionDistribution,
    "Orbit.AI.Native.EQSQuerierRegionCoverageAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEQSLocalRegionDistribution::RunTest(const FString& Parameters)
{
    UEnvQueryGenerator_OrbitSurfacePoints* Generator = NewObject<UEnvQueryGenerator_OrbitSurfacePoints>();
    if (!TestNotNull(TEXT("Create native EQS generator"), Generator)) return false;
    TestFalse(TEXT("Existing full sphere mode is the default"), Generator->bLimitToQuerierRegion);
    TestFalse(TEXT("Default sphere center remains literal for existing assets"),
        Generator->bUseSphereCenterContext);
    UEnvQueryTest_OrbitSphericalDistance* DistanceTest = NewObject<UEnvQueryTest_OrbitSphericalDistance>();
    if (!TestNotNull(TEXT("Native EQS distance test constructible"), DistanceTest)) return false;
    TestFalse(TEXT("Distance test also defaults to literal center for compatibility"),
        DistanceTest->bUseSphereCenterContext);
    constexpr int32 Count = 128;
    const FVector Axes[] = { FVector(1, 0, 0), FVector(-1, 0, 0),
        FVector(0, 1, 0), FVector(0, 0, -1), FVector(0.42, -0.7, 0.58).GetSafeNormal() };
    for (const FVector& Axis : Axes)
    {
        FVector Sum = FVector::ZeroVector;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVector Direction = UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(
                Index, Count, Axis, 45.0f);
            if (!TestTrue(TEXT("Local candidate remains unit length"), Direction.IsUnit(0.001))) return false;
            if (!TestTrue(TEXT("Local candidate is inside the requested 45-degree cap"),
                FVector::DotProduct(Axis, Direction) >= FMath::Cos(FMath::DegreesToRadians(45.0f)) - 0.0001f))
                return false;
            Sum += Direction;
        }
        TestTrue(TEXT("Local samples average toward the querier"),
            FVector::DotProduct(Sum.GetSafeNormal(), Axis) > 0.99f);
    }
    const FVector Forward(1, 0, 0);
    TestTrue(TEXT("Cap distribution is deterministic"),
        UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(20, Count, Forward, 60.0f).Equals(
            UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(20, Count, Forward, 60.0f), 0.000001));
    TestTrue(TEXT("Different querier directions produce different candidates"),
        !UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(20, Count, Forward, 60.0f).Equals(
            UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(20, Count, FVector(0, 1, 0), 60.0f)));
    TestTrue(TEXT("Invalid zero-axis rejects the local sample"),
        UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(20, Count,
            FVector::ZeroVector, 45.0f).IsNearlyZero());
    TestTrue(TEXT("Invalid sample index is rejected"),
        UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(Count, Count,
            Forward, 45.0f).IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEQSLocalRegionAngleLimits,
    "Orbit.AI.Native.EQSQuerierRegionAngularLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEQSLocalRegionAngleLimits::RunTest(const FString& Parameters)
{
    const FVector Axis = FVector(-0.4, 0.6, 0.7).GetSafeNormal();
    const int32 Count = 256;
    for (const float Angle : {1.0f, 20.0f, 90.0f, 179.0f, 180.0f})
    {
        double WorstDot = 1.0;
        for (int32 I = 0; I < Count; ++I)
        {
            const FVector Direction = UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(
                I, Count, Axis, Angle);
            WorstDot = FMath::Min(WorstDot, FVector::DotProduct(Axis, Direction));
            if (!TestTrue(TEXT("Candidate within cap at all supported angles"),
                FVector::DotProduct(Axis, Direction) >= FMath::Cos(FMath::DegreesToRadians(Angle)) - 0.0001f))
                return false;
        }
        TestTrue(TEXT("Sampler reaches near outer edge instead of clustering at center"),
            WorstDot < FMath::Cos(FMath::DegreesToRadians(Angle * 0.9f)));
    }
    TestTrue(TEXT("NaN angle rejected"),
        UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(1, Count,
            Axis, std::numeric_limits<float>::quiet_NaN()).IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEQSLocalRegionPhysicalProjection,
    "Orbit.AI.Native.EQSQuerierRegionPhysicalSurface",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEQSLocalRegionPhysicalProjection::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create actual spherical collision world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    if (!TestNotNull(TEXT("World exists"), World)) return false;
    AActor* Planet = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Spawn physical spherical surface"), Planet)) return false;
    USphereComponent* Surface = NewObject<USphereComponent>(Planet);
    Planet->AddInstanceComponent(Surface);
    Planet->SetRootComponent(Surface);
    Surface->InitSphereRadius(1000.0f);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent();
    FOrbitSphericalNavigationSettings Settings;
    constexpr float Radius = 1000.0f;
    const FVector Center(0, 0, 0);
    for (const FVector& Axis : {FVector::ForwardVector, -FVector::ForwardVector,
        FVector::UpVector, FVector(0.3, -0.5, 0.8).GetSafeNormal()})
    {
        int32 Accepted = 0;
        for (int32 I = 0; I < 32; ++I)
        {
            const FVector Dir = UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(
                I, 32, Axis, 30.0f);
            const FVector Candidate = Center + Radius * Dir;
            FVector Projected;
            if (UOrbitAISurfaceNavigation::ProjectToSphericalSurface(
                World, Candidate, Center, Radius, Settings, Projected))
            {
                ++Accepted;
                if (!TestTrue(TEXT("Actual collision projection lies on planet"),
                    FMath::IsNearlyEqual(Projected.Size(), Radius, 2.0))) return false;
                if (!TestTrue(TEXT("Projected point remains inside bounded search region"),
                    FVector::DotProduct(Projected.GetSafeNormal(), Axis) >=
                    FMath::Cos(FMath::DegreesToRadians(31.0f)))) return false;
            }
        }
        if (!TestTrue(TEXT("Local region produces collision-projected candidates on every hemisphere"),
            Accepted > 0)) return false;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitBlueprintMoveRequestStatusIsolation,
    "Orbit.AI.Native.BlueprintRequestStatusIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitBlueprintMoveRequestStatusIsolation::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Null controller cannot own a request"),
        UOrbitAIBlueprintLibrary::IsRequestCurrent(nullptr, 1));
    FOrbitAIMoveDiagnostics Snapshot;
    Snapshot.RequestId = 123;
    TestFalse(TEXT("Null controller cannot return request status"),
        UOrbitAIBlueprintLibrary::GetRequestStatus(nullptr, 123, Snapshot));
    TestEqual(TEXT("Failed status query resets data instead of exposing stale values"), Snapshot.RequestId, 0);
    AAIController* Controller = NewObject<AAIController>(GetTransientPackage());
    if (!TestNotNull(TEXT("Create ordinary controller"), Controller)) return false;
    TestFalse(TEXT("Controller with no pawn is safe"),
        UOrbitAIBlueprintLibrary::IsRequestCurrent(Controller, 5));
    Snapshot.RequestId = 456;
    TestFalse(TEXT("Controller without Orbit component cannot report status"),
        UOrbitAIBlueprintLibrary::GetRequestStatus(Controller, 5, Snapshot));
    TestEqual(TEXT("Invalid request clears stale snapshot"), Snapshot.RequestId, 0);
    return true;
}
#endif
