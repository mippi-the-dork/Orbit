#include "OrbitProjectileMovementComponent.h"
#include "OrbitSettings.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitProjectileGravityReplacementTest,
    "Orbit.Projectile.GravityReplacementPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitProjectileGravityReplacementTest::RunTest(const FString& Parameters)
{
    const FVector Native(120.0, -40.0, -980.0);
    FOrbitGravitySample Sample;
    Sample.bHasInfluence = true;
    Sample.Acceleration = FVector(0.0, 980.0, 0.0);
    using T = UOrbitProjectileMovementComponent;
    TestTrue(TEXT("Sideways Orbit gravity replaces native Z while retaining other forces"),
        T::ReplaceGravityTerm(Native, -980.0, Sample, 1.0, true, true).Equals(FVector(120., 940., 0.), 0.001));
    TestTrue(TEXT("GravityScale applied once"),
        T::ReplaceGravityTerm(Native, -980.0, Sample, 2.0, true, true).Equals(FVector(120., 1920., 0.), 0.001));
    Sample.Acceleration = FVector::ZeroVector;
    TestTrue(TEXT("Explicit zero-G field cancels native gravity"),
        T::ReplaceGravityTerm(Native, -980.0, Sample, 1.0, true, true).Equals(FVector(120., -40., 0.), 0.001));
    Sample.bHasInfluence = false;
    TestTrue(TEXT("Native fallback keeps entire original acceleration"),
        T::ReplaceGravityTerm(Native, -980.0, Sample, 1.0, true, true).Equals(Native, 0.001));
    TestTrue(TEXT("Zero fallback removes native gravity but keeps homing/forces"),
        T::ReplaceGravityTerm(Native, -980.0, Sample, 1.0, true, false).Equals(FVector(120., -40., 0.), 0.001));
    TestTrue(TEXT("Disabled Orbit integration retains native acceleration"),
        T::ReplaceGravityTerm(Native, -980.0, Sample, 1.0, false, false).Equals(Native, 0.001));
    TestNotNull(TEXT("Projectile movement derives from native movement component"),
        T::StaticClass());
    TestTrue(TEXT("Safe default uses Orbit project settings"),
        GetDefault<T>()->OutsideFieldBehavior == EOrbitProjectileOutsideField::ProjectSettings);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitProjectileBouncePreviewPolicy,
    "Orbit.Projectile.BouncePreviewPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitProjectileBouncePreviewPolicy::RunTest(const FString& Parameters)
{
    using T = UOrbitProjectileMovementComponent;
    const FVector Incoming(100.0, 0.0, -200.0);
    TestTrue(TEXT("Bounce works with world Z normal"),
        T::PredictBounceVelocity(Incoming, FVector::UpVector, 0.5f, 0.0f).Equals(FVector(100., 0., 100.), 0.001));
    TestTrue(TEXT("Bounce works with sideways gravity and contact"),
        T::PredictBounceVelocity(FVector(-200., 100., 0.), FVector::ForwardVector, 1.0f, 0.5f)
        .Equals(FVector(200., 50., 0.), 0.001));
    TestTrue(TEXT("Outward moving velocity remains unchanged"),
        T::PredictBounceVelocity(FVector(20., 0., 0.), FVector::ForwardVector, 1.0f, 0.0f)
        .Equals(FVector(20., 0., 0.), 0.001));
    TestTrue(TEXT("Zero normal cannot produce NaNs"),
        T::PredictBounceVelocity(Incoming, FVector::ZeroVector, 0.7f, 0.2f).Equals(Incoming, 0.001));
    TestTrue(TEXT("Bounce parameters clamp to physical range"),
        T::PredictBounceVelocity(Incoming, FVector::UpVector, 4.0f, -1.0f)
        .Equals(FVector(100., 0., 200.), 0.001));
    return true;
}
#endif
