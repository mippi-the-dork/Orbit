#include "OrbitChaosGravityBridge.h"
#include "OrbitSettings.h"
#include "Chaos/PerParticleGravity.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
    FOrbitFieldDefinition MakeDirectionalField()
    {
        FOrbitFieldDefinition Field;
        Field.Shape = EOrbitGravityShape::Directional;
        Field.Direction = EOrbitGravityDirection::Inward;
        Field.DirectionVector = FVector::DownVector;
        Field.Strength = 980.0;
        Field.MaxDistance = 0.0;
        Field.Falloff = EOrbitGravityFalloff::Constant;
        Field.bPhysics = true;
        return Field;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMethodCompatibilityTest,
    "Orbit.Physics.GravityIntegration.StandardIsDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMethodCompatibilityTest::RunTest(const FString& Parameters)
{
    // This test must pass whether a tester has explicitly selected Experimental
    // in their Project Settings or is running with the default configuration.
    TestEqual(TEXT("Standard is serialized as enum 0 for safe no-override config"),
        static_cast<uint8>(EOrbitRigidBodyGravityMethod::Standard), static_cast<uint8>(0));
    TestEqual(TEXT("Experimental is a separate non-default enum value"),
        static_cast<uint8>(EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform), static_cast<uint8>(1));
    TestEqual(TEXT("The experimental path never uses main native gravity group 0"),
        FOrbitChaosGravityBridge::GravityGroup, 7);
    TestTrue(TEXT("Group 7 exists in this version of Chaos"),
        FOrbitChaosGravityBridge::GravityGroup < static_cast<int32>(Chaos::FPerParticleGravity::MaxGravityGroups));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosUniformEligibilityTest,
    "Orbit.Physics.GravityIntegration.SingleUniformDirectional",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosUniformEligibilityTest::RunTest(const FString& Parameters)
{
    TArray<FOrbitFieldDefinition> Fields{MakeDirectionalField()};
    FVector Acceleration;
    TestTrue(TEXT("Single unlimited constant directional source can use native solver gravity"),
        FOrbitChaosGravityBridge::TryGetUniformField(Fields, Acceleration));
    TestTrue(TEXT("Gravity magnitude and direction preserved"),
        Acceleration.Equals(FVector(0, 0, -980), 0.001));

    Fields[0].DirectionVector = FVector::RightVector;
    TestTrue(TEXT("Sideways direction is eligible"),
        FOrbitChaosGravityBridge::TryGetUniformField(Fields, Acceleration));
    TestTrue(TEXT("Sideways gravity has the authored acceleration"),
        Acceleration.Equals(FVector(0, 980, 0), 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosUniformRejectionTest,
    "Orbit.Physics.GravityIntegration.NonUniformFallsBack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosUniformRejectionTest::RunTest(const FString& Parameters)
{
    FVector Result;
    TArray<FOrbitFieldDefinition> Fields;
    TestFalse(TEXT("No source cannot activate experimental gravity"),
        FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));

    Fields.Add(MakeDirectionalField());
    Fields[0].Shape = EOrbitGravityShape::Point;
    TestFalse(TEXT("Radial fields require Standard"), FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));
    Fields[0].Shape = EOrbitGravityShape::Axis;
    TestFalse(TEXT("Axis fields require Standard"), FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));
    Fields[0].Shape = EOrbitGravityShape::Spline;
    TestFalse(TEXT("Spline fields require Standard"), FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));
    Fields[0].Shape = EOrbitGravityShape::Volume;
    TestFalse(TEXT("Bounded volumes require Standard"), FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));

    Fields[0] = MakeDirectionalField();
    Fields[0].MaxDistance = 500.0;
    TestFalse(TEXT("Finite directional field falls back"), FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));
    Fields[0] = MakeDirectionalField();
    Fields[0].Falloff = EOrbitGravityFalloff::Linear;
    TestFalse(TEXT("Falloff field falls back"), FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));
    Fields[0] = MakeDirectionalField();
    Fields[0].bLimitToBox = true;
    TestFalse(TEXT("Box-limited field falls back"), FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));
    Fields[0] = MakeDirectionalField();
    Fields[0].bPhysics = false;
    TestFalse(TEXT("Non-physics source cannot activate"), FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));
    Fields[0] = MakeDirectionalField();
    Fields.Add(MakeDirectionalField());
    TestFalse(TEXT("Multiple physics sources must not be approximated"),
        FOrbitChaosGravityBridge::TryGetUniformField(Fields, Result));
    return true;
}
#endif
