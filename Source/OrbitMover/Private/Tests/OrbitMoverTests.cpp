#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OrbitMoverGravityPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitMoverZeroGravityTest, "Orbit.Mover.ZeroGravityPreservesUp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitMoverZeroGravityTest::RunTest(const FString& Parameters)
{
    FOrbitGravitySample Zero;
    Zero.bHasInfluence = true;
    Zero.bHasValidDirection = false;
    Zero.Acceleration = FVector::ZeroVector;
    const FOrbitMoverGravityDecision D = FOrbitMoverGravityPolicy::Resolve(Zero, FVector::RightVector);
    TestTrue(TEXT("Zero-g source must still override gravity"), D.bOverrideGravity);
    TestTrue(TEXT("Zero-g source must hold a stable Up override"), D.bOverrideUp);
    TestTrue(TEXT("Preserve the last Up"), D.Up.Equals(FVector::RightVector));
    TestTrue(TEXT("Acceleration is explicitly zero"), D.Acceleration.IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitMoverExitTest, "Orbit.Mover.ExitReleasesOverrides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitMoverExitTest::RunTest(const FString& Parameters)
{
    FOrbitGravitySample Outside;
    Outside.bHasInfluence = false;
    Outside.Acceleration = FVector(0, 0, -980); // native fallback is not an Orbit source
    const FOrbitMoverGravityDecision D = FOrbitMoverGravityPolicy::Resolve(Outside, FVector::UpVector);
    TestFalse(TEXT("Outside Orbit fields native Mover owns gravity"), D.bOverrideGravity);
    TestFalse(TEXT("Outside Orbit fields native Mover owns Up"), D.bOverrideUp);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitMoverDirectionTest, "Orbit.Mover.SidewaysAcceleration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitMoverDirectionTest::RunTest(const FString& Parameters)
{
    FOrbitGravitySample Side;
    Side.bHasInfluence = true;
    Side.bHasValidDirection = true;
    Side.Acceleration = FVector(-490, 0, 0);
    Side.Up = FVector::ForwardVector;
    const FOrbitMoverGravityDecision D = FOrbitMoverGravityPolicy::Resolve(Side, FVector::UpVector);
    TestTrue(TEXT("Sideways gravity passed unmodified"), D.Acceleration.Equals(FVector(-490, 0, 0)));
    TestTrue(TEXT("Local Up passed from the source"), D.Up.Equals(FVector::ForwardVector));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitMoverGravityComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitMoverMissingMoverSafeTest,
    "Orbit.Mover.MissingMoverSafe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitMoverMissingMoverSafeTest::RunTest(const FString& Parameters)
{
    // No Epic Mover UObject instance is required to create and query this component.
    UOrbitMoverGravityComponent* Adapter = NewObject<UOrbitMoverGravityComponent>(GetTransientPackage());
    TestNotNull(TEXT("Internal Orbit Mover adapter must be constructible without a Mover dependency"), Adapter);
    if (!Adapter) { return false; }
    Adapter->SetOrbitMoverGravityEnabled(false);
    Adapter->RefreshOrbitMoverGravity();
    TestFalse(TEXT("No backend can be claimed without a valid Mover component"), Adapter->bBackendSupported);
    TestEqual(TEXT("No Mover samples can be applied without a Mover component"), Adapter->SamplesApplied, 0);
    return true;
}
#endif
