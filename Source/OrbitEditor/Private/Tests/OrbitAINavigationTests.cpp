#include "Misc/AutomationTest.h"
#include "OrbitAIPathFollowerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAITangentSteering,
    "Orbit.AI.GravityTangentSteering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAITangentSteering::RunTest(const FString& Parameters)
{
    const FVector Target(100.0, 40.0, 200.0);
    const FVector SteeringZ = UOrbitAIPathFollowerComponent::CalculateTangentSteering(FVector::ZeroVector, Target, FVector::DownVector);
    TestTrue(TEXT("Z-down steering stays tangent"), FMath::Abs(SteeringZ.Z) < KINDA_SMALL_NUMBER);
    TestTrue(TEXT("Z-down steering points toward tangent target"), SteeringZ.X > 0.0 && SteeringZ.Y > 0.0);
    const FVector SteeringX = UOrbitAIPathFollowerComponent::CalculateTangentSteering(FVector::ZeroVector, Target, FVector(-1, 0, 0));
    TestTrue(TEXT("X-down steering stays tangent"), FMath::Abs(SteeringX.X) < KINDA_SMALL_NUMBER);
    TestTrue(TEXT("X-down steering moves toward target"), SteeringX.Y > 0.0 && SteeringX.Z > 0.0);
    TestTrue(TEXT("No gravity uses full vector"), UOrbitAIPathFollowerComponent::CalculateTangentSteering(
        FVector::ZeroVector, FVector::UpVector * 100.0, FVector::ZeroVector).Equals(FVector::UpVector));
    TestTrue(TEXT("Aligned gravity returns zero instead of invalid movement"), UOrbitAIPathFollowerComponent::CalculateTangentSteering(
        FVector::ZeroVector, FVector::UpVector * 100.0, FVector::DownVector).IsNearlyZero());
    TestTrue(TEXT("Near tangent waypoint with small normal offset can be accepted"),
        UOrbitAIPathFollowerComponent::IsWaypointReached(FVector::ZeroVector, FVector(20, 0, 60), FVector::DownVector, 30, 75, true));
    TestFalse(TEXT("Large normal difference cannot count as arrival"),
        UOrbitAIPathFollowerComponent::IsWaypointReached(FVector::ZeroVector, FVector(20, 0, 500), FVector::DownVector, 30, 75, true));
    TestFalse(TEXT("Full distance remains mandatory when tangent arrival is disabled"),
        UOrbitAIPathFollowerComponent::IsWaypointReached(FVector::ZeroVector, FVector(20, 0, 60), FVector::DownVector, 30, 75, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAISphericalArc,
    "Orbit.AI.SphericalArcGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAISphericalArc::RunTest(const FString& Parameters)
{
    TArray<FVector> Points;
    const FVector Center(300, -500, 900);
    const FVector Start = Center + FVector(1000, 0, 0);
    const FVector Goal = Center + FVector(0, 1000, 0);
    TestTrue(TEXT("Quarter-circle route generated"), UOrbitAIPathFollowerComponent::BuildSphericalArcWaypoints(
        Start, Goal, Center, 125, Points));
    TestTrue(TEXT("Route contains multiple points"), Points.Num() > 5);
    if (!Points.IsEmpty())
    {
        TestTrue(TEXT("Last generated point exactly matches goal"), Points.Last().Equals(Goal));
    }
    for (const FVector& Point : Points)
    {
        TestTrue(TEXT("Points stay on sphere"), FMath::Abs((Point - Center).Size() - 1000.0) < 0.5);
    }
    TArray<FVector> Antipodal;
    TestTrue(TEXT("Antipodal route uses deterministic fallback tangent"),
        UOrbitAIPathFollowerComponent::BuildSphericalArcWaypoints(Start, Center - FVector(1000, 0, 0), Center, 100, Antipodal));
    for (const FVector& Point : Antipodal)
    {
        TestTrue(TEXT("Antipodal points remain finite"), !Point.ContainsNaN());
        TestTrue(TEXT("Antipodal points remain on sphere"), FMath::Abs((Point - Center).Size() - 1000.0) < 0.5);
    }
    TestFalse(TEXT("Reject center as start"), UOrbitAIPathFollowerComponent::BuildSphericalArcWaypoints(
        Center, Goal, Center, 100, Points));
    TestFalse(TEXT("Reject incompatible radius"), UOrbitAIPathFollowerComponent::BuildSphericalArcWaypoints(
        Start, Center + FVector(0, 3000, 0), Center, 100, Points));
    TestFalse(TEXT("Reject zero spacing"), UOrbitAIPathFollowerComponent::BuildSphericalArcWaypoints(
        Start, Goal, Center, 0, Points));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIDefaults,
    "Orbit.AI.PathFollowerDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAIDefaults::RunTest(const FString& Parameters)
{
    const UOrbitAIPathFollowerComponent* Defaults = GetDefault<UOrbitAIPathFollowerComponent>();
    TestFalse(TEXT("No AI autopilot starts without explicit goals"), Defaults->bFollowingPath);
    TestEqual(TEXT("No default waypoint"), Defaults->CurrentWaypointIndex, INDEX_NONE);
    TestTrue(TEXT("Grounded movement by default"), Defaults->bRequireGrounded);
    TestTrue(TEXT("Tangent arrival enabled by default"), Defaults->bUseTangentArrival);
    TestTrue(TEXT("Appropriate acceptance radius"), Defaults->AcceptanceRadius > 0.0f);
    return true;
}
#endif
