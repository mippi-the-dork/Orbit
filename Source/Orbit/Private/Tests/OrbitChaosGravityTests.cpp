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

    FOrbitFieldDefinition MakePointField()
    {
        FOrbitFieldDefinition Field;
        Field.Shape = EOrbitGravityShape::Point;
        Field.Direction = EOrbitGravityDirection::Inward;
        Field.Falloff = EOrbitGravityFalloff::Constant;
        Field.Center = FVector(100.0, 200.0, -300.0);
        Field.Strength = 980.0;
        Field.bPhysics = true;
        Field.ChannelMask = 1u;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosPointEligibilityTest,
    "Orbit.Physics.GravityIntegration.SinglePointEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosPointEligibilityTest::RunTest(const FString& Parameters)
{
    TArray<FOrbitFieldDefinition> Fields{MakePointField()};
    FOrbitFieldDefinition Selected;
    FVector Uniform;
    bool bPoint = false;
    TestTrue(TEXT("One Point field is eligible for per-particle integration"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Selected, Uniform, bPoint));
    TestTrue(TEXT("Point field uses per-particle mode"), bPoint);
    TestTrue(TEXT("Point center is transferred with the snapshot"),
        Selected.Center.Equals(Fields[0].Center, 0.001));

    Fields[0].Falloff = EOrbitGravityFalloff::InverseSquare;
    Fields[0].MaxDistance = 10000.0;
    TestTrue(TEXT("Finite inverse-square Point fields are eligible"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Selected, Uniform, bPoint));
    TestTrue(TEXT("Falloff is preserved"), Selected.Falloff == EOrbitGravityFalloff::InverseSquare);

    Fields[0] = MakeDirectionalField();
    TestTrue(TEXT("Existing single Directional mode remains eligible"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Selected, Uniform, bPoint));
    TestFalse(TEXT("Directional must use the proven uniform path"), bPoint);
    TestTrue(TEXT("Directional acceleration is unchanged"),
        Uniform.Equals(FVector(0, 0, -980.0), 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosPointVectorTest,
    "Orbit.Physics.GravityIntegration.PointBodyVectors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosPointVectorTest::RunTest(const FString& Parameters)
{
    const FOrbitFieldDefinition Field = MakePointField();
    FVector A;
    FVector B;
    const FVector P1 = Field.Center + FVector(500, 0, 0);
    const FVector P2 = Field.Center + FVector(0, 500, 0);
    TestTrue(TEXT("Point field evaluates at body A"), FOrbitFieldEvaluator::Evaluate(Field, P1, A));
    TestTrue(TEXT("Point field evaluates at body B"), FOrbitFieldEvaluator::Evaluate(Field, P2, B));
    TestTrue(TEXT("Body A receives gravity toward its own center vector"),
        A.Equals(FVector(-980, 0, 0), 0.001));
    TestTrue(TEXT("Body B receives a different gravity vector at the same substep"),
        B.Equals(FVector(0, -980, 0), 0.001));

    FOrbitFieldDefinition InverseSquare = Field;
    InverseSquare.Falloff = EOrbitGravityFalloff::InverseSquare;
    InverseSquare.ReferenceDistance = 100.0;
    TestTrue(TEXT("Inverse-square gravity evaluates at distance"),
        FOrbitFieldEvaluator::Evaluate(InverseSquare, P1, A));
    TestTrue(TEXT("Inverse square scales strength per particle"),
        A.Equals(FVector(-39.2, 0, 0), 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosPointSafetyTest,
    "Orbit.Physics.GravityIntegration.UnsupportedPointFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosPointSafetyTest::RunTest(const FString& Parameters)
{
    TArray<FOrbitFieldDefinition> Fields{MakePointField()};
    FOrbitFieldDefinition Selected;
    FVector Uniform;
    bool bPoint = false;
    Fields[0].Direction = EOrbitGravityDirection::VortexInward;
    TestFalse(TEXT("Vortex remains on Standard"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Selected, Uniform, bPoint));
    Fields[0] = MakePointField();
    Fields[0].bLimitToBox = true;
    TestFalse(TEXT("Box-limited Point remains on Standard"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Selected, Uniform, bPoint));
    Fields[0] = MakePointField();
    Fields.Add(MakeDirectionalField());
    TestFalse(TEXT("Overlapping multi-source field cannot use shared per-particle path"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Selected, Uniform, bPoint));
    Fields.Reset();
    Fields.Add(MakePointField());
    Fields[0].ChannelMask = 0u;
    TestFalse(TEXT("A disabled gravity channel cannot start a solver callback"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Selected, Uniform, bPoint));
    Fields[0] = MakePointField();
    Fields[0].Shape = EOrbitGravityShape::Axis;
    TestFalse(TEXT("Axis remains on Standard"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Selected, Uniform, bPoint));
    return true;
}
#endif
