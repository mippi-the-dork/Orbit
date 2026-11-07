#include "OrbitChaosGravityBridge.h"
#include "OrbitSettings.h"
#include "OrbitPhysicsIntegrationMath.h"
#include "Chaos/PerParticleGravity.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosEarlyOwnershipPolicyTest,
    "Orbit.Physics.GravityIntegration.EarlyOwnershipOptIn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosEarlyOwnershipPolicyTest::RunTest(const FString& Parameters)
{
    using namespace OrbitPhysicsIntegrationMath;
    TestFalse(TEXT("Standard never preclaims even if extra option is enabled"),
        ShouldPreclaimExperimentalGravity(true, true, false, true, true));
    TestFalse(TEXT("Existing Experimental projects stay unchanged unless explicitly opted in"),
        ShouldPreclaimExperimentalGravity(true, true, true, false, true));
    TestFalse(TEXT("Physics integration off disables early ownership"),
        ShouldPreclaimExperimentalGravity(false, true, true, true, true));
    TestFalse(TEXT("Automatic-body discovery off disables early ownership"),
        ShouldPreclaimExperimentalGravity(true, false, true, true, true));
    TestFalse(TEXT("Unsupported multiple/complex fields never preclaim"),
        ShouldPreclaimExperimentalGravity(true, true, true, true, false));
    TestTrue(TEXT("Only explicitly enabled eligible fields preclaim"),
        ShouldPreclaimExperimentalGravity(true, true, true, true, true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosEarlyOwnershipBodyTest,
    "Orbit.Physics.GravityIntegration.EarlyOwnershipBodyPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosEarlyOwnershipBodyTest::RunTest(const FString& Parameters)
{
    using namespace OrbitPhysicsIntegrationMath;
    TestTrue(TEXT("Registered native-gravity simulated body within matching source may be claimed"),
        CanPreclaimExperimentalBody(true, true, true, false, true));
    TestFalse(TEXT("Never claim native gravity disabled body"),
        CanPreclaimExperimentalBody(true, true, false, false, true));
    TestFalse(TEXT("Never claim an ignored body"),
        CanPreclaimExperimentalBody(true, true, true, true, true));
    TestFalse(TEXT("Never claim a channel mismatch or out-of-range body"),
        CanPreclaimExperimentalBody(true, true, true, false, false));
    TestFalse(TEXT("Never claim unregistered body"),
        CanPreclaimExperimentalBody(false, true, true, false, true));
    TestFalse(TEXT("Never claim non-simulating body"),
        CanPreclaimExperimentalBody(true, false, true, false, true));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMultiPointOptInTest,
    "Orbit.Physics.GravityIntegration.MultiplePointOptIn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMultiPointOptInTest::RunTest(const FString& Parameters)
{
    using namespace OrbitPhysicsIntegrationMath;
    TestFalse(TEXT("Standard mode never enables multiple Point solver"),
        ShouldActivateMultiplePointGravity(false, true, true));
    TestFalse(TEXT("Prior Experimental opt-in alone does not enable new multi-source mode"),
        ShouldActivateMultiplePointGravity(true, false, true));
    TestFalse(TEXT("Unsupported fields never enable multi-source mode"),
        ShouldActivateMultiplePointGravity(true, true, false));
    TestTrue(TEXT("Separate opt-in plus eligible fields enables the new mode"),
        ShouldActivateMultiplePointGravity(true, true, true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMultiPointEligibilityTest,
    "Orbit.Physics.GravityIntegration.MultiplePointEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMultiPointEligibilityTest::RunTest(const FString& Parameters)
{
    TArray<FOrbitFieldDefinition> Fields;
    FOrbitFieldDefinition A = MakePointField();
    A.Center = FVector::ZeroVector;
    A.Combine = EOrbitGravityCombine::Additive;
    FOrbitFieldDefinition B = A;
    B.Center = FVector(-2500, 0, 0);
    B.MaxDistance = 2000.0;
    B.Falloff = EOrbitGravityFalloff::Linear;
    Fields.Add(A);
    Fields.Add(B);
    TArray<FOrbitFieldDefinition> Snapshot;
    TestTrue(TEXT("Two channel-zero ordinary Points support multi-Point solver"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Snapshot));
    TestEqual(TEXT("Both sources are retained"), Snapshot.Num(), 2);
    TestTrue(TEXT("Per-source position preserved"), Snapshot[1].Center.Equals(B.Center));
    TestTrue(TEXT("Per-source finite distance preserved"), Snapshot[1].MaxDistance == 2000.0);
    TestTrue(TEXT("Per-source falloff preserved"), Snapshot[1].Falloff == EOrbitGravityFalloff::Linear);
    FVector Uniform;
    FOrbitFieldDefinition Single;
    bool bSinglePoint = false;
    TestFalse(TEXT("The original single-source selector still rejects multi-source fields"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Single, Uniform, bSinglePoint));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMultiPointAdditiveTest,
    "Orbit.Physics.GravityIntegration.MultiplePointAdditiveVectors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMultiPointAdditiveTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition A = MakePointField();
    A.Center = FVector::ZeroVector;
    A.Combine = EOrbitGravityCombine::Additive;
    FOrbitFieldDefinition B = A;
    B.Center = FVector(-2000, 0, 0);
    TArray<FOrbitFieldDefinition> Fields{A, B};
    const FVector Between(-1000, 0, 0);
    FOrbitGravitySample Sample = FOrbitFieldEvaluator::CombineForBody(
        Fields, Between, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Opposing additive sources cancel in overlap"), Sample.bHasInfluence);
    TestTrue(TEXT("Opposing equal gravity is exactly zero"), Sample.Acceleration.IsNearlyZero(0.001));

    Fields[1].Strength = 600.0;
    Sample = FOrbitFieldEvaluator::CombineForBody(
        Fields, Between, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Asymmetric additive acceleration uses both vectors"),
        Sample.Acceleration.Equals(FVector(380, 0, 0), 0.001));

    Fields[0].MaxDistance = 800.0; // First Point no longer influences midpoint.
    Sample = FOrbitFieldEvaluator::CombineForBody(
        Fields, Between, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Finite range removes only the out-of-range contribution"),
        Sample.Acceleration.Equals(FVector(-600, 0, 0), 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMultiPointOverrideTest,
    "Orbit.Physics.GravityIntegration.MultiplePointCompetition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMultiPointOverrideTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition A = MakePointField();
    A.Center = FVector::ZeroVector;
    A.Combine = EOrbitGravityCombine::Override;
    A.Priority = 0;
    A.SourceId = 10;
    FOrbitFieldDefinition B = A;
    B.Center = FVector(-2000, 0, 0);
    B.Priority = 1;
    B.Strength = 350;
    B.SourceId = 11;
    TArray<FOrbitFieldDefinition> Fields{A, B};
    const FVector Position(-1000, 0, 0);
    FOrbitGravitySample Sample = FOrbitFieldEvaluator::CombineForBody(
        Fields, Position, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Higher-priority override wins even when weaker"),
        Sample.Acceleration.Equals(FVector(-350, 0, 0), 0.001));
    TestEqual(TEXT("Highest-priority override identifies correct source"), Sample.DominantSourceId, 11);

    Fields[1].Priority = 0;
    Sample = FOrbitFieldEvaluator::CombineForBody(
        Fields, Position, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Equal priority selects the stronger influence"),
        Sample.Acceleration.Equals(FVector(980, 0, 0), 0.001));
    Fields[1].Strength = 980;
    Sample = FOrbitFieldEvaluator::CombineForBody(
        Fields, Position, FVector::ZeroVector, 1u, EOrbitOverrideResolution::PriorityOverride);
    TestTrue(TEXT("Legacy priority resolution uses first Point when equally distant"),
        Sample.Acceleration.Equals(FVector(980, 0, 0), 0.001));
    Sample = FOrbitFieldEvaluator::CombineForBody(
        Fields, Position, FVector::ZeroVector, 1u, EOrbitOverrideResolution::BlendedInfluence);
    TestTrue(TEXT("Blended equal-and-opposite influences cancel"),
        Sample.Acceleration.IsNearlyZero(0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMultiPointFallbackTest,
    "Orbit.Physics.GravityIntegration.MultiplePointSafetyFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMultiPointFallbackTest::RunTest(const FString& Parameters)
{
    const FOrbitFieldDefinition A = MakePointField();
    TArray<FOrbitFieldDefinition> Fields{A, A};
    TArray<FOrbitFieldDefinition> Out;
    TestTrue(TEXT("Two eligible fields accepted"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Out));
    Fields[1].Shape = EOrbitGravityShape::Directional;
    TestFalse(TEXT("Mixed Point/Directional falls back to Standard"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Out));
    TestEqual(TEXT("Rejected configuration clears output"), Out.Num(), 0);
    Fields[1] = A;
    Fields[1].Direction = EOrbitGravityDirection::VortexInward;
    TestFalse(TEXT("Vortex Point falls back to Standard"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Out));
    Fields[1] = A;
    Fields[1].ChannelMask = 2u;
    TestFalse(TEXT("Mixed source channel masks safely fall back"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Out));
    Fields[1] = A;
    Fields[1].bLimitToBox = true;
    TestFalse(TEXT("Box mask falls back to Standard"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Out));
    Fields[1] = A;
    Fields.SetNum(FOrbitChaosGravityBridge::MaxExperimentalPointSources + 1);
    TestFalse(TEXT("Oversized source configuration stays Standard"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Out));
    TestTrue(TEXT("Only default channel-zero receiver can be assigned"),
        FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(true, 1u));
    TestFalse(TEXT("Non-default receiver cannot be assigned multi-source group"),
        FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(true, 2u));
    TestFalse(TEXT("Multi-channel receiver cannot be assigned multi-source group"),
        FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(true, 3u));
    TestTrue(TEXT("Existing single-source receiver policy remains unchanged"),
        FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(false, 3u));
    return true;
}


// Iteration 038: new field shapes are gated separately from the existing
// multi-Point setting. These test the actual selector used by preclaim and tick.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMixedModeSelectionTest,
    "Orbit.Physics.GravityIntegration.MixedSourceOptInAndSwitching",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMixedModeSelectionTest::RunTest(const FString& Parameters)
{
    using namespace OrbitPhysicsIntegrationMath;
    using EMode = EExperimentalSolverSelection;
    TestTrue(TEXT("Standard cannot select a solver path"),
        SelectExperimentalSolver(false, true, true, true, true, true) == EMode::None);
    TestTrue(TEXT("Single-source eligibility wins regardless of unrelated flags"),
        SelectExperimentalSolver(true, true, true, true, true, true) == EMode::Single);
    TestTrue(TEXT("Previously opted-in multi-Point remains unchanged"),
        SelectExperimentalSolver(true, false, true, true, false, true) == EMode::MultiplePoint);
    TestTrue(TEXT("Old multi-Point opt-in never turns on new mixed mode"),
        SelectExperimentalSolver(true, false, true, false, false, true) == EMode::None);
    TestTrue(TEXT("Mixed must be independently enabled"),
        SelectExperimentalSolver(true, false, false, false, true, true) == EMode::MixedPointDirectional);
    TestTrue(TEXT("Invalid mixed config falls back"),
        SelectExperimentalSolver(true, false, false, false, true, false) == EMode::None);
    TestTrue(TEXT("Switching both opt-ins off restores Standard eligibility"),
        SelectExperimentalSolver(true, false, false, true, false, true) == EMode::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMixedEligibilityTest,
    "Orbit.Physics.GravityIntegration.MixedPointDirectionalEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMixedEligibilityTest::RunTest(const FString& Parameters)
{
    const FOrbitFieldDefinition Point = MakePointField();
    FOrbitFieldDefinition Directional = MakeDirectionalField();
    Directional.ChannelMask = 1u;
    TArray<FOrbitFieldDefinition> Fields{Point, Directional};
    TArray<FOrbitFieldDefinition> Output;
    TestTrue(TEXT("One Point and one uniform Directional are eligible"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    TestEqual(TEXT("Both source snapshots retained"), Output.Num(), 2);
    TestTrue(TEXT("Field order preserved"), Output[0].Shape == EOrbitGravityShape::Point &&
        Output[1].Shape == EOrbitGravityShape::Directional);

    Fields[1].MaxDistance = 2000.0;
    TestFalse(TEXT("Finite Directional currently falls back"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    Fields[1] = Directional;
    Fields[1].Falloff = EOrbitGravityFalloff::Linear;
    TestFalse(TEXT("Directional falloff currently falls back"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    Fields[1] = Directional;
    Fields[1].DirectionVector = FVector::ZeroVector;
    TestFalse(TEXT("Invalid Directional vector falls back"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    Fields[1] = Directional;
    Fields[1].Shape = EOrbitGravityShape::Axis;
    TestFalse(TEXT("Axis requires a future dedicated opt-in"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    Fields[1] = Directional;
    Fields[1].ChannelMask = 2u;
    TestFalse(TEXT("Channel 1 source falls back"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    Fields[1] = Directional;
    Fields[0].Direction = EOrbitGravityDirection::VortexInward;
    TestFalse(TEXT("Vortex requires Standard"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    Fields[0] = Point;
    Fields[1] = Point;
    TestFalse(TEXT("Two Points still use prior selector, not mixed"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    Fields[1] = Directional;
    Fields.SetNum(FOrbitChaosGravityBridge::MaxExperimentalPointSources + 1);
    TestFalse(TEXT("Over-cap configuration safely falls back"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Output));
    TestEqual(TEXT("Rejected field list is cleared"), Output.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMixedVectorTest,
    "Orbit.Physics.GravityIntegration.MixedVectorParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMixedVectorTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Point = MakePointField();
    Point.Center = FVector::ZeroVector;
    Point.Combine = EOrbitGravityCombine::Additive;
    FOrbitFieldDefinition Directional = MakeDirectionalField();
    Directional.Combine = EOrbitGravityCombine::Additive;
    Directional.DirectionVector = FVector::UpVector;
    TArray<FOrbitFieldDefinition> Fields{Point, Directional};
    TArray<FOrbitFieldDefinition> Accepted;
    TestTrue(TEXT("Both fields are eligible"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Accepted));
    const FOrbitGravitySample Additive = FOrbitFieldEvaluator::CombineForBody(
        Accepted, FVector(1000, 0, 0), FVector::ZeroVector, 1u,
        EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Additive horizontal Point + upward Directional are summed"),
        Additive.Acceleration.Equals(FVector(-980, 0, 980), 0.001));
    Directional.Combine = EOrbitGravityCombine::Override;
    Directional.Priority = 2;
    Fields[1] = Directional;
    const FOrbitGravitySample Override = FOrbitFieldEvaluator::CombineForBody(
        Fields, FVector(1000, 0, 0), FVector::ZeroVector, 1u,
        EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Priority Override uses the same evaluator as Standard"),
        Override.Acceleration.Equals(FVector(0, 0, 980), 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMixedMovementTest,
    "Orbit.Physics.GravityIntegration.MixedSourceMovementMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMixedMovementTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Point = MakePointField();
    Point.Center = FVector::ZeroVector;
    Point.Combine = EOrbitGravityCombine::Additive;
    FOrbitFieldDefinition Directional = MakeDirectionalField();
    Directional.Combine = EOrbitGravityCombine::Additive;
    TArray<FOrbitFieldDefinition> Fields{Point, Directional};
    const FVector SamplePosition(1100, 0, 0);
    const FVector Before = FOrbitFieldEvaluator::CombineForBody(
        Fields, SamplePosition, FVector::ZeroVector, 1u,
        EOrbitOverrideResolution::StrongestInfluence).Acceleration;
    Fields[0].Center = FVector(2200, 0, 0);
    const FVector After = FOrbitFieldEvaluator::CombineForBody(
        Fields, SamplePosition, FVector::ZeroVector, 1u,
        EOrbitOverrideResolution::StrongestInfluence).Acceleration;
    TestTrue(TEXT("Moved Point reverses its radial contribution"),
        Before.Equals(FVector(-980, 0, -980), 0.001) &&
        After.Equals(FVector(980, 0, -980), 0.001));
    TArray<FOrbitFieldDefinition> Snapshot;
    TestTrue(TEXT("The moved snapshot remains solver eligible"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Snapshot));
    return true;
}

// Evaluation throughput is a diagnostic, NOT an FPS/physics-step guarantee:
// timings vary with machine, CPU load and Development build configuration.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosSolverEvaluationCostTest,
    "Orbit.Physics.GravityIntegration.EvaluationCostBaseline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosSolverEvaluationCostTest::RunTest(const FString& Parameters)
{
    double Checksum = 0.0;
    for (const int32 SourceCount : {2, 8, 32})
    {
        TArray<FOrbitFieldDefinition> Fields;
        for (int32 Index = 0; Index < SourceCount; ++Index)
        {
            FOrbitFieldDefinition Source = MakePointField();
            Source.Center = FVector(Index * 320.0, (Index % 3) * 700.0, 0.0);
            Source.MaxDistance = 4000.0;
            Source.Combine = EOrbitGravityCombine::Additive;
            Fields.Add(Source);
        }
        for (const int32 BodyCount : {64, 256})
        {
            // Warm up first to avoid counting one-time initialization.
            for (int32 Index = 0; Index < BodyCount; ++Index)
            {
                FOrbitFieldEvaluator::CombineForBody(Fields,
                    FVector(Index * 23.0, Index * 11.0, 1000.0), FVector::ZeroVector, 1u,
                    EOrbitOverrideResolution::StrongestInfluence);
            }
            const double Begin = FPlatformTime::Seconds();
            const int32 Iterations = 20;
            for (int32 Pass = 0; Pass < Iterations; ++Pass)
            {
                for (int32 Index = 0; Index < BodyCount; ++Index)
                {
                    const FOrbitGravitySample Sample = FOrbitFieldEvaluator::CombineForBody(
                        Fields, FVector(Index * 23.0, Index * 11.0, 1000.0),
                        FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
                    Checksum += Sample.Acceleration.X * 1.e-8;
                }
            }
            const double ElapsedMs = (FPlatformTime::Seconds() - Begin) * 1000.0;
            AddInfo(FString::Printf(TEXT("Orbit field evaluator: %d sources x %d bodies x %d passes = %.3f ms total (%.3f us/body-evaluation); excludes Chaos, contacts, and snapshot transfer"),
                SourceCount, BodyCount, Iterations, ElapsedMs,
                (ElapsedMs * 1000.0) / (BodyCount * Iterations)));
            TestTrue(TEXT("Evaluation completes in finite time"), FMath::IsFinite(ElapsedMs));
        }
    }
    TestTrue(TEXT("Benchmark outputs are finite"), FMath::IsFinite(static_cast<double>(Checksum)));
    return true;
}


// Iteration 039 keeps the solver implementation unchanged. These native Automation
// Framework tests exercise reproducible field transitions and backward-compatible
// selection. They are math/policy tests, NOT live Chaos sleep/contact tests.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMovedSourceReturnTest,
    "Orbit.Physics.GravityIntegration.MovedSourceReturnParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMovedSourceReturnTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition A = MakePointField();
    A.Center = FVector::ZeroVector;
    A.MaxDistance = 2000.0;
    A.Combine = EOrbitGravityCombine::Override;
    FOrbitFieldDefinition B = A;
    B.Center = FVector(-2500.0, 0.0, 0.0);
    TArray<FOrbitFieldDefinition> Fields{A, B};
    TArray<FOrbitFieldDefinition> Accepted;
    TestTrue(TEXT("Two static Points are eligible"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Accepted));

    const FVector Positions[] = {
        FVector(0, 1150, 0), FVector(-1150, 0, 0),
        FVector(0, 0, 1150), FVector(-1400, 550, 0)
    };
    FVector Initial[UE_ARRAY_COUNT(Positions)];
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Positions); ++Index)
    {
        Initial[Index] = FOrbitFieldEvaluator::CombineForBody(
            Fields, Positions[Index], FVector::ZeroVector, 1u,
            EOrbitOverrideResolution::BlendedInfluence).Acceleration;
    }

    // The same sequence used by the manual test: move at t=5, restore at t=10.
    Fields[0].Center = FVector(-1250, 0, 625);
    TestTrue(TEXT("Moved snapshot remains eligible"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Accepted));
    bool bSawChangedVector = false;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Positions); ++Index)
    {
        const FVector Moved = FOrbitFieldEvaluator::CombineForBody(
            Fields, Positions[Index], FVector::ZeroVector, 1u,
            EOrbitOverrideResolution::BlendedInfluence).Acceleration;
        bSawChangedVector |= !Moved.Equals(Initial[Index], 0.001);
    }
    TestTrue(TEXT("Changing source center changes gravity for at least one body"), bSawChangedVector);

    Fields[0].Center = FVector::ZeroVector;
    TestTrue(TEXT("Restored snapshot remains eligible"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Accepted));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Positions); ++Index)
    {
        const FVector Restored = FOrbitFieldEvaluator::CombineForBody(
            Fields, Positions[Index], FVector::ZeroVector, 1u,
            EOrbitOverrideResolution::BlendedInfluence).Acceleration;
        TestTrue(*FString::Printf(TEXT("Position %d returns to original field after source restoration"), Index),
            Restored.Equals(Initial[Index], 0.001));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosMixedSourceTransitionTest,
    "Orbit.Physics.GravityIntegration.MixedSourceTransitionPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosMixedSourceTransitionTest::RunTest(const FString& Parameters)
{
    using namespace OrbitPhysicsIntegrationMath;
    using EMode = EExperimentalSolverSelection;
    FOrbitFieldDefinition A = MakePointField();
    A.Center = FVector::ZeroVector;
    FOrbitFieldDefinition B = A;
    B.Center = FVector(-2500, 0, 0);
    FOrbitFieldDefinition D = MakeDirectionalField();
    TArray<FOrbitFieldDefinition> Fields{A, B, D};
    TArray<FOrbitFieldDefinition> Accepted;
    TestTrue(TEXT("Mixed Point + Directional is eligible"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Accepted));
    TestTrue(TEXT("Mixed opt-in selects mixed mode"),
        SelectExperimentalSolver(true, false, true, false, true, true) == EMode::MixedPointDirectional);
    TestTrue(TEXT("Without mixed opt-in, Standard remains selected"),
        SelectExperimentalSolver(true, false, true, false, false, true) == EMode::None);

    Fields[2].bPhysics = false;
    TestTrue(TEXT("Disabling Directional permits two Point source eligibility"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Accepted));
    TestFalse(TEXT("Disabled Directional no longer creates mixed field"),
        FOrbitChaosGravityBridge::TryGetMixedPointDirectionalFields(Fields, Accepted));
    TestTrue(TEXT("Opted-in multiple Point retains existing selection"),
        SelectExperimentalSolver(true, false, true, true, true, false) == EMode::MultiplePoint);

    Fields[1].bPhysics = false;
    FOrbitFieldDefinition Single;
    FVector Uniform;
    bool bPoint = false;
    TestTrue(TEXT("Disabling a second Point recovers the single Point solver"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Single, Uniform, bPoint));
    TestTrue(TEXT("Single solver remains per-particle Point"), bPoint);
    TestTrue(TEXT("Single source has highest selection priority"),
        SelectExperimentalSolver(true, true, true, true, true, true) == EMode::Single);

    Fields[0].bPhysics = false;
    TestFalse(TEXT("Disabling every physics source cannot select single mode"),
        FOrbitChaosGravityBridge::TryGetSolverField(Fields, Single, Uniform, bPoint));
    TestFalse(TEXT("Disabling every physics source cannot select multi Point"),
        FOrbitChaosGravityBridge::TryGetMultiplePointFields(Fields, Accepted));
    TestTrue(TEXT("No enabled source falls back to Standard"),
        SelectExperimentalSolver(true, false, true, false, true, false) == EMode::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosRangeEdgeTest,
    "Orbit.Physics.GravityIntegration.PointRangeBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosRangeEdgeTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Point = MakePointField();
    Point.Center = FVector::ZeroVector;
    Point.MaxDistance = 2000.0;
    FVector Acceleration;
    TestTrue(TEXT("Inside finite source range receives constant Point gravity"),
        FOrbitFieldEvaluator::Evaluate(Point, FVector(1999, 0, 0), Acceleration));
    TestTrue(TEXT("Inside finite source has expected acceleration"),
        Acceleration.Equals(FVector(-980, 0, 0), 0.001));
    TestFalse(TEXT("Outside range source no longer influences body"),
        FOrbitFieldEvaluator::Evaluate(Point, FVector(2001, 0, 0), Acceleration));
    Point.Falloff = EOrbitGravityFalloff::Linear;
    TestTrue(TEXT("Linear Point field accepts position within range"),
        FOrbitFieldEvaluator::Evaluate(Point, FVector(1000, 0, 0), Acceleration));
    TestTrue(TEXT("Linear falloff at half range is half strength"),
        Acceleration.Equals(FVector(-490, 0, 0), 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosReceiverIsolationTest,
    "Orbit.Physics.GravityIntegration.MultiSourceReceiverProtection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosReceiverIsolationTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Default Channel 0 receiver is eligible for multi-source solver"),
        FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(true, 1u));
    for (uint8 Mask : {uint8(0), uint8(2), uint8(3), uint8(4), uint8(7)})
    {
        TestFalse(TEXT("Unsupported non-default or multi-channel receiver stays Standard"),
            FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(true, Mask));
    }
    TestTrue(TEXT("Single-source receiver policy is not changed by multi-source gate"),
        FOrbitChaosGravityBridge::CanAssignMultiPointReceiver(false, 3u));
    return true;
}


// Iteration 040 compatibility tests use the SAME field evaluator as Standard.
// These are policy/maths tests, not solver-integration or contact tests.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosExpandedOptInTest,
    "Orbit.Physics.GravityIntegration.ExpandedOptInAndPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosExpandedOptInTest::RunTest(const FString& Parameters)
{
    using namespace OrbitPhysicsIntegrationMath;
    using ESelection = EExperimentalSolverSelection;
    TestTrue(TEXT("No opt-in keeps unsupported fields on Standard"),
        SelectExperimentalSolver(true, false, false, false, false, false, false, true) == ESelection::None);
    TestTrue(TEXT("Expanded field requires both opt-in and eligible snapshot"),
        SelectExperimentalSolver(true, false, false, false, false, false, true, false) == ESelection::None);
    TestTrue(TEXT("Expanded mode selected for new unsupported field subset"),
        SelectExperimentalSolver(true, false, false, false, false, false, true, true) == ESelection::ExpandedFields);
    TestTrue(TEXT("Standard method cannot activate expanded fields"),
        SelectExperimentalSolver(false, false, false, false, false, false, true, true) == ESelection::None);
    TestTrue(TEXT("Existing single-source policy wins if already eligible"),
        SelectExperimentalSolver(true, true, false, false, false, false, true, true) == ESelection::Single);
    TestTrue(TEXT("Existing multi-Point policy wins before expanded field eligibility"),
        SelectExperimentalSolver(true, false, true, true, false, false, true, true) == ESelection::MultiplePoint);
    TestTrue(TEXT("Existing mixed-source policy wins before expanded field eligibility"),
        SelectExperimentalSolver(true, false, false, false, true, true, true, true) == ESelection::MixedPointDirectional);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosExpandedShapeEligibilityTest,
    "Orbit.Physics.GravityIntegration.ExpandedShapeEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosExpandedShapeEligibilityTest::RunTest(const FString& Parameters)
{
    TArray<FOrbitFieldDefinition> Fields;
    TArray<FOrbitFieldDefinition> Selected;
    FOrbitFieldDefinition Field = MakePointField();
    Field.Shape = EOrbitGravityShape::Axis;
    Field.Axis = FVector::UpVector;
    Fields.Add(Field);
    TestTrue(TEXT("Single Axis source can enter expanded solver"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0] = MakeDirectionalField();
    Fields[0].MaxDistance = 2000.0;
    Fields[0].Falloff = EOrbitGravityFalloff::Linear;
    Fields[0].bLimitToBox = true;
    TestTrue(TEXT("Finite bounded Directional field is eligible"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    for (EOrbitVolumeShape VolumeType : {EOrbitVolumeShape::Box, EOrbitVolumeShape::Sphere,
        EOrbitVolumeShape::Cylinder, EOrbitVolumeShape::Capsule})
    {
        Fields[0] = MakeDirectionalField();
        Fields[0].Shape = EOrbitGravityShape::Volume;
        Fields[0].VolumeShape = VolumeType;
        Fields[0].MaxDistance = 1500.0;
        Fields[0].Falloff = EOrbitGravityFalloff::Linear;
        TestTrue(TEXT("All native Volume boundary shapes are eligible"),
            FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    }
    Fields[0] = MakePointField();
    Fields[0].Shape = EOrbitGravityShape::Spline;
    Fields[0].Curve = {FVector(0,0,0), FVector(100,0,0), FVector(100,100,0)};
    Fields[0].CurveUp = {FVector::UpVector, FVector::UpVector, FVector::UpVector};
    Fields[0].Direction = EOrbitGravityDirection::SplineUp;
    TestTrue(TEXT("Oriented Spline Up uses existing sampled spline fields"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0].Direction = EOrbitGravityDirection::SplineDown;
    TestTrue(TEXT("Oriented Spline Down uses existing sampled spline fields"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0].Direction = EOrbitGravityDirection::Inward;
    TestTrue(TEXT("Radial Spline source is eligible"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosExpandedSafetyTest,
    "Orbit.Physics.GravityIntegration.ExpandedSafetyFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosExpandedSafetyTest::RunTest(const FString& Parameters)
{
    TArray<FOrbitFieldDefinition> Fields{MakePointField()};
    TArray<FOrbitFieldDefinition> Selected;
    Fields[0].Direction = EOrbitGravityDirection::VortexInward;
    TestFalse(TEXT("Velocity-steered Vortex stays Standard"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0] = MakePointField();
    Fields[0].ChannelMask = 2u;
    TestFalse(TEXT("Non-default gravity channel stays Standard"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0] = MakePointField();
    Fields[0].ChannelMask = 3u;
    TestFalse(TEXT("Multi-channel source stays Standard"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0] = MakePointField();
    Fields[0].Shape = EOrbitGravityShape::Axis;
    Fields[0].Axis = FVector::ZeroVector;
    TestFalse(TEXT("Malformed Axis rejected before physics callback"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0] = MakePointField();
    Fields[0].Shape = EOrbitGravityShape::Spline;
    Fields[0].Curve.Empty();
    TestFalse(TEXT("Empty Spline rejected"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0].Curve = {FVector::ZeroVector, FVector::RightVector};
    Fields[0].Direction = EOrbitGravityDirection::SplineUp;
    TestFalse(TEXT("Incomplete Spline orientation vectors rejected"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0] = MakePointField();
    Fields[0].MaxDistance = -3.0;
    TestFalse(TEXT("Negative range rejected"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields[0] = MakePointField();
    Fields[0].Strength = -1.0;
    TestFalse(TEXT("Invalid negative strength rejected"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    Fields.Reset();
    for (int32 Index=0; Index < 33; ++Index) { Fields.Add(MakePointField()); }
    TestFalse(TEXT("Above source budget falls back rather than truncating fields"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Selected));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosExpandedAxisParityTest,
    "Orbit.Physics.GravityIntegration.ExpandedAxisMathParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosExpandedAxisParityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Field = MakePointField();
    Field.Shape = EOrbitGravityShape::Axis;
    Field.Center = FVector::ZeroVector;
    Field.Axis = FVector::UpVector;
    const TArray<FOrbitFieldDefinition> Fields{Field};
    FVector Direct;
    TestTrue(TEXT("Axis samples at displaced position"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(300, 0, 900), Direct));
    const FOrbitGravitySample Combined = FOrbitFieldEvaluator::CombineForBody(
        Fields, FVector(300,0,900), FVector::ZeroVector, 1u, EOrbitOverrideResolution::BlendedInfluence);
    TestTrue(TEXT("Solver evaluation matches Standard for Axis"),
        Combined.Acceleration.Equals(Direct, .001));
    TestTrue(TEXT("Axis is radial around the cylinder, not world-down"),
        Combined.Acceleration.Equals(FVector(-980,0,0), .001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosExpandedVolumeParityTest,
    "Orbit.Physics.GravityIntegration.ExpandedVolumeBoundaryParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosExpandedVolumeParityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Field = MakeDirectionalField();
    Field.Shape = EOrbitGravityShape::Volume;
    Field.VolumeShape = EOrbitVolumeShape::Box;
    Field.BoxExtents = FVector(100, 200, 300);
    Field.Rotation = FQuat(FVector::UpVector, PI * .5);
    const TArray<FOrbitFieldDefinition> Fields{Field};
    const FVector Inside = FVector(0, 50, 0);
    const FVector Outside = FVector(400, 0, 0);
    FVector Direct;
    TestTrue(TEXT("Rotated box has valid inside sample"),
        FOrbitFieldEvaluator::Evaluate(Field, Inside, Direct));
    TestTrue(TEXT("Physics combined sampler uses same rotated boundary"),
        FOrbitFieldEvaluator::CombineForBody(Fields, Inside, FVector::ZeroVector, 1u,
            EOrbitOverrideResolution::BlendedInfluence).Acceleration.Equals(Direct, .001));
    TestFalse(TEXT("Outside rotated box correctly rejected"),
        FOrbitFieldEvaluator::Evaluate(Field, Outside, Direct));
    TestFalse(TEXT("Physics combined sampler also rejects outside point"),
        FOrbitFieldEvaluator::CombineForBody(Fields, Outside, FVector::ZeroVector, 1u,
            EOrbitOverrideResolution::BlendedInfluence).bHasInfluence);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosExpandedSplineParityTest,
    "Orbit.Physics.GravityIntegration.ExpandedSplineMathParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosExpandedSplineParityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Field = MakePointField();
    Field.Shape = EOrbitGravityShape::Spline;
    Field.Center = FVector::ZeroVector;
    Field.Curve = {FVector(0,0,0), FVector(500,0,0), FVector(500,500,0)};
    Field.CurveUp = {FVector::UpVector, FVector::UpVector, FVector::UpVector};
    Field.Direction = EOrbitGravityDirection::SplineDown;
    const FVector Position(200,100,0);
    const TArray<FOrbitFieldDefinition> Fields{Field};
    FVector Direct;
    TestTrue(TEXT("Oriented Spline evaluates at position"),
        FOrbitFieldEvaluator::Evaluate(Field, Position, Direct));
    const FOrbitGravitySample Combined = FOrbitFieldEvaluator::CombineForBody(
        Fields, Position, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Solver field-combine and Standard use identical spline evaluator"),
        Combined.Acceleration.Equals(Direct,.001));
    TestTrue(TEXT("SplineDown follows authored up samples"),
        Combined.Acceleration.Equals(FVector(0,0,-980),.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitChaosExpandedMixedParityTest,
    "Orbit.Physics.GravityIntegration.ExpandedMixedFieldParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitChaosExpandedMixedParityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Axis = MakePointField();
    Axis.Shape = EOrbitGravityShape::Axis;
    Axis.Axis = FVector::UpVector;
    Axis.Center = FVector::ZeroVector;
    Axis.Combine = EOrbitGravityCombine::Additive;
    FOrbitFieldDefinition Dir = MakeDirectionalField();
    Dir.Strength = 100;
    Dir.Combine = EOrbitGravityCombine::Override;
    Dir.MaxDistance = 2000;
    Dir.Falloff = EOrbitGravityFalloff::Linear;
    TArray<FOrbitFieldDefinition> Fields{Axis,Dir};
    TArray<FOrbitFieldDefinition> Snapshot;
    TestTrue(TEXT("Mixed Axis and bounded Directional selects expanded path"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Snapshot));
    const FVector P(500,0,0);
    const FOrbitGravitySample FromStandard = FOrbitFieldEvaluator::CombineForBody(
        Fields, P, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    const FOrbitGravitySample FromSnapshot = FOrbitFieldEvaluator::CombineForBody(
        Snapshot, P, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Copied solver snapshot preserves complete mixed force"),
        FromSnapshot.Acceleration.Equals(FromStandard.Acceleration,.001));
    // A same-priority Override intentionally suppresses a same-priority Additive.
    // This is the long-established Standard field-combination contract, not a
    // Chaos integration difference. Do not expect the Axis to be summed here.
    TestTrue(TEXT("Equal-priority Override suppresses Axis Additive"),
        FromStandard.Acceleration.Equals(FVector(0,0,-75),.001));
    TestTrue(TEXT("Expanded solver snapshot preserves Override suppression"),
        FromSnapshot.Acceleration.Equals(FVector(0,0,-75),.001));

    // Higher-priority Additive is allowed to contribute alongside the lower-
    // priority Override. This separately validates the sum the old test
    // incorrectly expected for equal-priority fields.
    Fields[0].Priority = Dir.Priority + 1;
    TestTrue(TEXT("Higher-priority Axis + bounded Directional remains eligible"),
        FOrbitChaosGravityBridge::TryGetExpandedFields(Fields, Snapshot));
    const FOrbitGravitySample HigherPriorityStandard = FOrbitFieldEvaluator::CombineForBody(
        Fields, P, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    const FOrbitGravitySample HigherPrioritySnapshot = FOrbitFieldEvaluator::CombineForBody(
        Snapshot, P, FVector::ZeroVector, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestTrue(TEXT("Higher-priority Axis Additive contributes with bounded Directional"),
        HigherPriorityStandard.Acceleration.Equals(FVector(-980,0,-75),.001));
    TestTrue(TEXT("Expanded snapshot preserves higher-priority Additive contribution"),
        HigherPrioritySnapshot.Acceleration.Equals(HigherPriorityStandard.Acceleration,.001));
    return true;
}

#endif
