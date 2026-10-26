#include "OrbitGravityTypes.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitPointGravityTest, "Orbit.Field.PointInwardOutward", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitPointGravityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Center = FVector::ZeroVector;
    Source.Shape = EOrbitGravityShape::Point;
    Source.Strength = 980.0;
    Source.Direction = EOrbitGravityDirection::Inward;
    FVector Force;
    TestTrue(TEXT("Point field influences location"), FOrbitFieldEvaluator::Evaluate(Source, FVector(200.0, 0.0, 0.0), Force));
    TestTrue(TEXT("Point inward points toward origin"), Force.Equals(FVector(-980.0, 0.0, 0.0), 0.0001));
    Source.Direction = EOrbitGravityDirection::Outward;
    TestTrue(TEXT("Point outward influences location"), FOrbitFieldEvaluator::Evaluate(Source, FVector(200.0, 0.0, 0.0), Force));
    TestTrue(TEXT("Point outward points away from origin"), Force.Equals(FVector(980.0, 0.0, 0.0), 0.0001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAxisGravityTest, "Orbit.Field.AxisRadial", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAxisGravityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Shape = EOrbitGravityShape::Axis;
    Source.Axis = FVector::UpVector;
    Source.Direction = EOrbitGravityDirection::Outward;
    FVector Force;
    TestTrue(TEXT("Axis field influences location"), FOrbitFieldEvaluator::Evaluate(Source, FVector(300.0, 400.0, 700.0), Force));
    TestTrue(TEXT("Axis ignores position along its length"), Force.Equals(FVector(588.0, 784.0, 0.0), 0.0001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitSplineGravityTest, "Orbit.Field.SplineNearestSegment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitSplineGravityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Shape = EOrbitGravityShape::Spline;
    Source.Direction = EOrbitGravityDirection::Inward;
    Source.Curve = { FVector(0.0, 0.0, 0.0), FVector(1000.0, 0.0, 0.0) };
    FVector Force;
    TestTrue(TEXT("Spline segment influences location"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500.0, 200.0, 0.0), Force));
    TestTrue(TEXT("Spline attraction is perpendicular to its closest segment"), Force.Equals(FVector(0.0, -980.0, 0.0), 0.0001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitNullGravityTest, "Orbit.Field.NullAndInfluence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitNullGravityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Shape = EOrbitGravityShape::Point;
    Source.Strength = 980.0;
    TArray<FOrbitFieldDefinition> Sources = { Source };
    const FOrbitGravitySample Sample = FOrbitFieldEvaluator::Combine(Sources, FVector::ZeroVector);
    TestTrue(TEXT("Source claims influence at center"), Sample.bHasInfluence);
    TestFalse(TEXT("Null direction is invalid"), Sample.bHasValidDirection);
    TestTrue(TEXT("Null acceleration remains zero"), Sample.Acceleration.IsNearlyZero());
    Source.MaxDistance = 100.0;
    Sources = { Source };
    TestFalse(TEXT("No influence outside radius"), FOrbitFieldEvaluator::Combine(Sources, FVector(200.0, 0.0, 0.0)).bHasInfluence);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitPriorityGravityTest, "Orbit.Field.PriorityAndAdditive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitPriorityGravityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition A;
    A.Shape = EOrbitGravityShape::Directional;
    A.DirectionVector = FVector::DownVector;
    A.Direction = EOrbitGravityDirection::Inward;
    A.Priority = 1;
    A.Strength = 200.0;
    FOrbitFieldDefinition B = A;
    B.Direction = EOrbitGravityDirection::Outward;
    B.Priority = 5;
    B.Strength = 300.0;
    TArray<FOrbitFieldDefinition> Fields = { A, B };
    FOrbitGravitySample Sample = FOrbitFieldEvaluator::Combine(Fields, FVector::ZeroVector);
    TestTrue(TEXT("Higher-priority override wins"), Sample.Acceleration.Equals(FVector(0.0, 0.0, 300.0), 0.0001));
    A.Combine = EOrbitGravityCombine::Additive;
    B.Combine = EOrbitGravityCombine::Additive;
    Fields = { A, B };
    Sample = FOrbitFieldEvaluator::Combine(Fields, FVector::ZeroVector);
    TestTrue(TEXT("Additive fields sum their vectors"), Sample.Acceleration.Equals(FVector(0.0, 0.0, 100.0), 0.0001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitFalloffGravityTest, "Orbit.Field.LinearAndInverseSquare", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitFalloffGravityTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Shape = EOrbitGravityShape::Point;
    Source.Strength = 1000.0;
    Source.MaxDistance = 1000.0;
    Source.Falloff = EOrbitGravityFalloff::Linear;
    FVector Force;
    TestTrue(TEXT("Linear falloff sample valid"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500.0, 0.0, 0.0), Force));
    TestTrue(TEXT("Linear falloff halves strength halfway"), FMath::IsNearlyEqual(Force.Size(), 500.0, 0.001));
    Source.Falloff = EOrbitGravityFalloff::InverseSquare;
    Source.ReferenceDistance = 100.0;
    TestTrue(TEXT("Inverse-square sample valid"), FOrbitFieldEvaluator::Evaluate(Source, FVector(200.0, 0.0, 0.0), Force));
    TestTrue(TEXT("Inverse-square strength quarters at twice reference distance"), FMath::IsNearlyEqual(Force.Size(), 250.0, 0.001));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitVolumeShapesTest, "Orbit.Field.VolumeShapes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitVolumeShapesTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Field;
    Field.Shape = EOrbitGravityShape::Volume;
    Field.VolumeShape = EOrbitVolumeShape::Box;
    Field.BoxExtents = FVector(100.0, 200.0, 300.0);
    FVector Acceleration;
    TestTrue(TEXT("Box interior"), FOrbitFieldEvaluator::Evaluate(Field, FVector(90.0, 190.0, 290.0), Acceleration));
    TestTrue(TEXT("Volume inward points along actor -Z"), Acceleration.Equals(FVector(0.0, 0.0, -980.0), 0.001));
    Field.Direction = EOrbitGravityDirection::Outward;
    TestTrue(TEXT("Volume outward is valid"), FOrbitFieldEvaluator::Evaluate(Field, FVector(90.0, 190.0, 290.0), Acceleration));
    TestTrue(TEXT("Volume outward points along actor +Z"), Acceleration.Equals(FVector(0.0, 0.0, 980.0), 0.001));
    Field.Direction = EOrbitGravityDirection::Inward;
    TestFalse(TEXT("Box exterior"), FOrbitFieldEvaluator::Evaluate(Field, FVector(110.0, 0.0, 0.0), Acceleration));

    Field.VolumeShape = EOrbitVolumeShape::Sphere;
    Field.VolumeRadius = 200.0;
    TestTrue(TEXT("Sphere interior"), FOrbitFieldEvaluator::Evaluate(Field, FVector(100.0, 100.0, 0.0), Acceleration));
    TestFalse(TEXT("Sphere exterior"), FOrbitFieldEvaluator::Evaluate(Field, FVector(150.0, 150.0, 0.0), Acceleration));

    Field.VolumeShape = EOrbitVolumeShape::Cylinder;
    Field.VolumeRadius = 100.0;
    Field.VolumeHalfHeight = 300.0;
    Field.Rotation = FQuat(FVector::RightVector, PI / 2.0);
    TestTrue(TEXT("Rotated cylinder interior"), FOrbitFieldEvaluator::Evaluate(Field, Field.Rotation.RotateVector(FVector(70.0, 60.0, 290.0)), Acceleration));
    TestFalse(TEXT("Rotated cylinder radial exterior"), FOrbitFieldEvaluator::Evaluate(Field, Field.Rotation.RotateVector(FVector(110.0, 0.0, 0.0)), Acceleration));
    TestFalse(TEXT("Rotated cylinder end exterior"), FOrbitFieldEvaluator::Evaluate(Field, Field.Rotation.RotateVector(FVector(0.0, 0.0, 310.0)), Acceleration));

    Field.VolumeShape = EOrbitVolumeShape::Capsule;
    TestTrue(TEXT("Capsule cap interior"), FOrbitFieldEvaluator::Evaluate(Field, Field.Rotation.RotateVector(FVector(0.0, 0.0, 290.0)), Acceleration));
    TestFalse(TEXT("Capsule cap exterior"), FOrbitFieldEvaluator::Evaluate(Field, Field.Rotation.RotateVector(FVector(0.0, 0.0, 310.0)), Acceleration));
    TestFalse(TEXT("Capsule radial exterior"), FOrbitFieldEvaluator::Evaluate(Field, Field.Rotation.RotateVector(FVector(110.0, 0.0, 0.0)), Acceleration));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitSplineDirectionTest, "Orbit.Field.SplineOrientedDirections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitSplineDirectionTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Field;
    Field.Shape = EOrbitGravityShape::Spline;
    Field.Curve = { FVector(0.0, 0.0, 0.0), FVector(1000.0, 0.0, 0.0) };
    Field.CurveUp = { FVector::UpVector, FVector::UpVector };
    FVector Acceleration;
    Field.Direction = EOrbitGravityDirection::SplineDown;
    TestTrue(TEXT("Spline Down influences"), FOrbitFieldEvaluator::Evaluate(Field, FVector(500.0, 120.0, 0.0), Acceleration));
    TestTrue(TEXT("Spline Down opposes authored Up"), Acceleration.Equals(FVector(0.0, 0.0, -980.0), 0.001));
    Field.Direction = EOrbitGravityDirection::SplineUp;
    TestTrue(TEXT("Spline Up influences"), FOrbitFieldEvaluator::Evaluate(Field, FVector(500.0, 120.0, 0.0), Acceleration));
    TestTrue(TEXT("Spline Up follows authored Up"), Acceleration.Equals(FVector(0.0, 0.0, 980.0), 0.001));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitGravityChannelsTest, "Orbit.Field.GravityChannels", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitGravityChannelsTest::RunTest(const FString& Parameters)
{
    FOrbitGravityChannels SourceChannels;
    FOrbitGravityChannels ReceiverChannels;
    TestEqual(TEXT("New source and receiver default to channel 0"), static_cast<int32>(SourceChannels.ToMask()), 1);
    TestEqual(TEXT("New receiver defaults to channel 0"), static_cast<int32>(ReceiverChannels.ToMask()), 1);

    FOrbitFieldDefinition Channel0;
    Channel0.Shape = EOrbitGravityShape::Directional;
    Channel0.Strength = 100.0;
    Channel0.ChannelMask = SourceChannels.ToMask();
    Channel0.SourceId = 10;

    FOrbitFieldDefinition Channel1 = Channel0;
    Channel1.ChannelMask = 2u;
    Channel1.Strength = 300.0;
    Channel1.SourceId = 11;
    Channel1.Priority = 5;
    TArray<FOrbitFieldDefinition> Fields = { Channel0, Channel1 };

    FOrbitGravitySample Sample = FOrbitFieldEvaluator::Combine(Fields, FVector::ZeroVector);
    TestTrue(TEXT("Implicit channel 0 gets influence"), Sample.bHasInfluence);
    TestEqual(TEXT("Unmatched high priority source does not override channel 0"), Sample.DominantSourceId, 10);
    TestTrue(TEXT("Only channel 0 contributes"), Sample.Acceleration.Equals(FVector(0,0,-100.0), 0.001));

    Sample = FOrbitFieldEvaluator::Combine(Fields, FVector::ZeroVector, EOrbitGravityChannel::Physics, 2u);
    TestTrue(TEXT("Channel 1 receiver gets channel 1 field"), Sample.Acceleration.Equals(FVector(0,0,-300.0), 0.001));
    Sample = FOrbitFieldEvaluator::Combine(Fields, FVector::ZeroVector, EOrbitGravityChannel::Physics, 4u);
    TestFalse(TEXT("Channel 2 receiver matches no source"), Sample.bHasInfluence);

    Channel0.Combine = EOrbitGravityCombine::Additive;
    Channel1.Combine = EOrbitGravityCombine::Additive;
    Fields = { Channel0, Channel1 };
    Sample = FOrbitFieldEvaluator::Combine(Fields, FVector::ZeroVector, EOrbitGravityChannel::Physics, 3u);
    TestTrue(TEXT("Multi-channel receiver sums matching additive sources"), Sample.Acceleration.Equals(FVector(0,0,-400.0), 0.001));

    Channel1.ChannelMask = 3u;
    Fields = { Channel1 };
    Sample = FOrbitFieldEvaluator::Combine(Fields, FVector::ZeroVector, EOrbitGravityChannel::Physics, 1u);
    TestTrue(TEXT("Multi-channel source matches a channel 0 receiver"), Sample.bHasInfluence);

    Fields[0].ChannelMask = 0u;
    Sample = FOrbitFieldEvaluator::Combine(Fields, FVector::ZeroVector);
    TestFalse(TEXT("Source with every channel off affects nobody"), Sample.bHasInfluence);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitMaxDistanceShapesTest, "Orbit.Field.MaxDistanceShapes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitMaxDistanceShapesTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.MaxDistance = 500.0;
    Source.Shape = EOrbitGravityShape::Point;
    FVector Force;
    TestTrue(TEXT("Point includes samples within its spherical radius"),
        FOrbitFieldEvaluator::Evaluate(Source, FVector(300, 0, 0), Force));
    TestFalse(TEXT("Point excludes samples outside its spherical radius"),
        FOrbitFieldEvaluator::Evaluate(Source, FVector(0, 0, 600), Force));

    Source.Shape = EOrbitGravityShape::Axis;
    Source.Axis = FVector::UpVector;
    TestTrue(TEXT("Axis has unlimited length along its axis"),
        FOrbitFieldEvaluator::Evaluate(Source, FVector(300, 0, 100000), Force));
    TestFalse(TEXT("Axis radius clips influence regardless of height"),
        FOrbitFieldEvaluator::Evaluate(Source, FVector(600, 0, 0), Force));

    Source.Shape = EOrbitGravityShape::Spline;
    Source.Curve = { FVector(-1000, 0, 0), FVector(1000, 0, 0) };
    TestTrue(TEXT("Spline accepts within tube radius"),
        FOrbitFieldEvaluator::Evaluate(Source, FVector(0, 350, 0), Force));
    TestFalse(TEXT("Spline rejects outside tube radius"),
        FOrbitFieldEvaluator::Evaluate(Source, FVector(0, 600, 0), Force));

    Source.Shape = EOrbitGravityShape::Directional;
    TestTrue(TEXT("Directional Max Distance is centered on the actor"),
        FOrbitFieldEvaluator::Evaluate(Source, FVector(0, 0, 400), Force));
    TestFalse(TEXT("Directional Max Distance cuts off radially"),
        FOrbitFieldEvaluator::Evaluate(Source, FVector(0, 0, 600), Force));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAxisVortexTest, "Orbit.Field.AxisVortex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAxisVortexTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Shape = EOrbitGravityShape::Axis;
    Source.Direction = EOrbitGravityDirection::VortexInward;
    Source.Axis = FVector::UpVector;
    Source.Strength = 1000.0;
    FVector Acceleration;

    Source.VortexSpin = 0.0;
    TestTrue(TEXT("Zero spin evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500, 0, 100), Acceleration));
    TestTrue(TEXT("Zero spin is pure inward gravity"), Acceleration.Equals(FVector(-1000, 0, 0), 0.001));

    Source.VortexSpin = 1.0;
    TestTrue(TEXT("Clockwise axis vortex evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500, 0, 100), Acceleration));
    TestTrue(TEXT("Positive spin is clockwise viewed from local +Z"), Acceleration.X < 0.0 && Acceleration.Y < 0.0 && FMath::Abs(Acceleration.Z) < 0.001);
    TestTrue(TEXT("Spin preserves total strength"), FMath::IsNearlyEqual(Acceleration.Size(), 1000.0, 0.001));
    TestTrue(TEXT("Unit spin makes 45-degree acceleration"), FMath::IsNearlyEqual(Acceleration.X, Acceleration.Y, 0.001));

    Source.VortexSpin = -1.0;
    TestTrue(TEXT("Counterclockwise axis vortex evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500, 0, 100), Acceleration));
    TestTrue(TEXT("Negative spin is counterclockwise"), Acceleration.X < 0.0 && Acceleration.Y > 0.0);

    Source.Direction = EOrbitGravityDirection::VortexOutward;
    TestTrue(TEXT("Outward vortex evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500, 0, 100), Acceleration));
    TestTrue(TEXT("Outward is radial-out with counterclockwise spin"), Acceleration.X > 0.0 && Acceleration.Y > 0.0);

    Source.Axis = FVector::ForwardVector; // local axis rotated from +Z to +X
    Source.VortexSpin = 1.0;
    TestTrue(TEXT("Rotated axis vortex evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(300, 500, 0), Acceleration));
    TestTrue(TEXT("Rotated clockwise tangent follows new axis"), Acceleration.X == 0.0 && Acceleration.Y > 0.0 && Acceleration.Z < 0.0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitPointVortexTest, "Orbit.Field.PointVortex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitPointVortexTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Shape = EOrbitGravityShape::Point;
    Source.Axis = FVector::UpVector;
    Source.Direction = EOrbitGravityDirection::VortexInward;
    Source.VortexSpin = 1.0;
    FVector Acceleration;
    TestTrue(TEXT("Point vortex evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(100, 0, 0), Acceleration));
    TestTrue(TEXT("Point vortex spins clockwise on its equator"), Acceleration.X < 0.0 && Acceleration.Y < 0.0);
    TestTrue(TEXT("Point vortex has original strength"), FMath::IsNearlyEqual(Acceleration.Size(), Source.Strength, 0.001));
    TestTrue(TEXT("At the point-vortex pole evaluation remains valid"), FOrbitFieldEvaluator::Evaluate(Source, FVector(0, 0, 100), Acceleration));
    TestTrue(TEXT("At the pole there is only radial gravity"), Acceleration.Equals(FVector(0, 0, -980), 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitSplineVortexTest, "Orbit.Field.SplineVortex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitSplineVortexTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Shape = EOrbitGravityShape::Spline;
    Source.Direction = EOrbitGravityDirection::VortexInward;
    Source.Curve = { FVector(0, 0, 0), FVector(1000, 0, 0) };
    Source.VortexSpin = 1.0;
    FVector Acceleration;
    TestTrue(TEXT("Spline vortex evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500, 200, 0), Acceleration));
    TestTrue(TEXT("Spline +spin curves toward clockwise around +spline tangent"), Acceleration.Y < 0.0 && Acceleration.Z < 0.0 && FMath::Abs(Acceleration.X) < 0.001);
    Source.VortexSpin = -1.0;
    TestTrue(TEXT("Spline reverse evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500, 200, 0), Acceleration));
    TestTrue(TEXT("Spline -spin reverses tangential acceleration"), Acceleration.Y < 0.0 && Acceleration.Z > 0.0);
    Source.Direction = EOrbitGravityDirection::VortexOutward;
    TestTrue(TEXT("Spline outward vortex evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500, 200, 0), Acceleration));
    TestTrue(TEXT("Spline outward keeps its tangential spin"), Acceleration.Y > 0.0 && Acceleration.Z > 0.0);
    Source.Direction = EOrbitGravityDirection::Inward;
    TestTrue(TEXT("Ordinary spline inward evaluates"), FOrbitFieldEvaluator::Evaluate(Source, FVector(500, 200, 0), Acceleration));
    TestTrue(TEXT("Non-vortex direction ignores spin"), Acceleration.Equals(FVector(0,-980,0), 0.001));
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitStreamlineTracingTest, "Orbit.Field.StreamlineTracing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitStreamlineTracingTest::RunTest(const FString& Parameters)
{
    TArray<FVector> Points;
    FOrbitFieldDefinition Field;
    Field.Shape = EOrbitGravityShape::Directional;
    Field.Direction = EOrbitGravityDirection::Inward;
    Field.DirectionVector = FVector::DownVector;
    Field.MaxDistance = 500.0;

    const int32 StraightSegments = FOrbitFieldEvaluator::TraceStreamline(Field, FVector::ZeroVector, 50.0, 20, Points);
    TestEqual(TEXT("Directional streamline stops at field boundary"), StraightSegments, 10);
    TestEqual(TEXT("Directional streamline includes the seed"), Points.Num(), StraightSegments + 1);
    for (int32 Index = 1; Index < Points.Num(); ++Index)
    {
        TestTrue(TEXT("Directional streamline is straight and gravity-aligned"),
            Points[Index].Equals(FVector(0.0, 0.0, -50.0 * Index), 0.001));
    }

    Field.Shape = EOrbitGravityShape::Axis;
    Field.Axis = FVector::UpVector;
    Field.Direction = EOrbitGravityDirection::VortexInward;
    Field.VortexSpin = 1.0;
    Field.MaxDistance = 1000.0;
    const int32 SpiralSegments = FOrbitFieldEvaluator::TraceStreamline(Field, FVector(400.0, 0.0, 0.0), 10.0, 30, Points);
    TestTrue(TEXT("Inward vortex traces more than a few segments"), SpiralSegments >= 10);
    if (Points.Num() > 1)
    {
        TestTrue(TEXT("Vortex curves clockwise relative to +Z axis"), Points.Last().Y < -10.0);
        TestTrue(TEXT("Inward vortex decreases radial distance"),
            FVector2D(Points.Last().X, Points.Last().Y).Size() < 400.0);
    }

    Field.Shape = EOrbitGravityShape::Point;
    Field.Direction = EOrbitGravityDirection::Inward;
    TestEqual(TEXT("Null center creates no streamline"),
        FOrbitFieldEvaluator::TraceStreamline(Field, FVector::ZeroVector, 10.0, 64, Points), 0);
    TestTrue(TEXT("Null center leaves no drawable points"), Points.IsEmpty());
    TestEqual(TEXT("Invalid integration step is rejected"),
        FOrbitFieldEvaluator::TraceStreamline(Field, FVector(100.0, 0.0, 0.0), 0.0, 64, Points), 0);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitVortexSpiralCaptureTest, "Orbit.Field.VortexSpiralCapture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitVortexSpiralCaptureTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Source;
    Source.Shape = EOrbitGravityShape::Axis;
    Source.Axis = FVector::UpVector;
    Source.Direction = EOrbitGravityDirection::VortexInward;
    Source.VortexSpin = 1.0;
    Source.Strength = 980.0;
    Source.VortexPhysicsMode = EOrbitVortexPhysicsMode::SpiralCapture;
    FVector Acceleration;
    const FVector Position(1000.0, 0.0, 500.0);

    TestTrue(TEXT("Capture acceleration evaluates"),
        FOrbitFieldEvaluator::EvaluateForBody(Source, Position, FVector::ZeroVector, Acceleration));
    TestTrue(TEXT("Capture initially pulls inward and clockwise"),
        Acceleration.X < 0.0 && Acceleration.Y < 0.0);
    TestTrue(TEXT("No unwanted axial steering on an Axis source"),
        FMath::IsNearlyZero(Acceleration.Z, 1e-3));

    TestTrue(TEXT("Capture evaluates existing excess tangential velocity"),
        FOrbitFieldEvaluator::EvaluateForBody(Source, Position, FVector(0.0, -5000.0, 600.0), Acceleration));
    TestTrue(TEXT("Capture brakes excessive clockwise velocity instead of accelerating it"), Acceleration.Y > 0.0);
    TestTrue(TEXT("Axial travel is left unmodified"), FMath::IsNearlyZero(Acceleration.Z, 1e-3));

    Source.VortexSpin = -1.0;
    TestTrue(TEXT("Reverse spin evaluates"),
        FOrbitFieldEvaluator::EvaluateForBody(Source, Position, FVector::ZeroVector, Acceleration));
    TestTrue(TEXT("Reverse spin reverses tangential acceleration"), Acceleration.Y > 0.0);

    Source.VortexSpin = 0.0;
    FVector LegacyAcceleration;
    TestTrue(TEXT("Zero spin still evaluates"),
        FOrbitFieldEvaluator::EvaluateForBody(Source, Position, FVector::ZeroVector, Acceleration));
    TestTrue(TEXT("Zero spin uses original radial gravity"),
        FOrbitFieldEvaluator::Evaluate(Source, Position, LegacyAcceleration) &&
        Acceleration.Equals(LegacyAcceleration, 0.001));

    Source.VortexSpin = 1.0;
    // Singularities must still brake an object rather than treating the core
    // as a zero-force hole that can eject a fast-moving body.
    TestTrue(TEXT("Capture evaluates source centerline"),
        FOrbitFieldEvaluator::EvaluateForBody(Source, FVector::ZeroVector, FVector(400.0, 0.0, 0.0), Acceleration));
    TestTrue(TEXT("Capture brakes centerline crossing"), Acceleration.X < -1.0);

    Source.VortexPhysicsMode = EOrbitVortexPhysicsMode::ContinuousForce;
    TestTrue(TEXT("Legacy force mode evaluates"),
        FOrbitFieldEvaluator::EvaluateForBody(Source, Position, FVector(0.0, -5000.0, 600.0), Acceleration));
    TestTrue(TEXT("Legacy mode preserves original instantaneous acceleration"),
        FOrbitFieldEvaluator::Evaluate(Source, Position, LegacyAcceleration) &&
        Acceleration.Equals(LegacyAcceleration, 0.001));

    Source.VortexPhysicsMode = EOrbitVortexPhysicsMode::SpiralCapture;
    Source.MaxDistance = 500.0;
    TestFalse(TEXT("Capture does not influence a body outside Max Distance"),
        FOrbitFieldEvaluator::EvaluateForBody(Source, Position, FVector::ZeroVector, Acceleration));
    Source.MaxDistance = 0.0;

    // Simplified numerical regression with no ground or collision: the controlled
    // inward spiral should steadily converge instead of ejection from runaway spin.
    FVector SimPosition(1000.0, 0.0, 0.0);
    FVector SimVelocity = FVector::ZeroVector;
    constexpr double Dt = 1.0 / 120.0;
    double LargestRadius = 0.0;
    bool bValid = true;
    for (int32 Step = 0; Step < 960; ++Step)
    {
        if (!FOrbitFieldEvaluator::EvaluateForBody(Source, SimPosition, SimVelocity, Acceleration))
        {
            bValid = false;
            break;
        }
        SimVelocity += Acceleration * Dt;
        SimPosition += SimVelocity * Dt;
        if (SimPosition.ContainsNaN() || SimVelocity.ContainsNaN())
        {
            bValid = false;
            break;
        }
        LargestRadius = FMath::Max(LargestRadius, FVector2D(SimPosition.X, SimPosition.Y).Size());
    }
    TestTrue(TEXT("Spiral remains numerically valid for eight simulated seconds"), bValid);
    TestTrue(TEXT("Spiral does not eject beyond its starting radius"), LargestRadius < 1200.0);
    TestTrue(TEXT("Spiral converges close to its axis"), FVector2D(SimPosition.X, SimPosition.Y).Size() < 250.0);

    TArray<FOrbitFieldDefinition> Fields{ Source };
    const FOrbitGravitySample Channel0 = FOrbitFieldEvaluator::CombineForBody(Fields, Position, FVector::ZeroVector, 1u);
    const FOrbitGravitySample Channel1 = FOrbitFieldEvaluator::CombineForBody(Fields, Position, FVector::ZeroVector, 2u);
    TestTrue(TEXT("Capture respects matching gravity channels"), Channel0.bHasInfluence);
    TestFalse(TEXT("Capture ignores unmatched gravity channels"), Channel1.bHasInfluence);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
