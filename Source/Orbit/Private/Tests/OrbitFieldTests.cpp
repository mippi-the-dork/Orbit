#include "OrbitGravityTypes.h"
#include "OrbitCharacterControlsComponent.h"
#include "OrbitGravityAnimInstance.h"
#include "OrbitPhysicsRestMath.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitVolumeShapeDistanceTest,
    "Orbit.Field.VolumeMaxDistanceRespectsShape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitVolumeShapeDistanceTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Field;
    Field.Shape = EOrbitGravityShape::Volume;
    Field.MaxDistance = 500.0;
    Field.Strength = 980.0;
    FVector Acceleration;

    // A 500-unit cuboid reaches outside a 500-unit sphere in its corners.
    Field.VolumeShape = EOrbitVolumeShape::Box;
    Field.BoxExtents = FVector(800.0, 1200.0, 900.0);
    const FOrbitEffectiveVolumeBounds Box = FOrbitFieldEvaluator::GetEffectiveVolumeBounds(Field);
    TestTrue(TEXT("Box half-extents cap on all axes"), Box.BoxHalfExtents.Equals(FVector(500.0)));
    TestTrue(TEXT("Box accepts corner beyond the old spherical cutoff"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(475.0, 475.0, 475.0), Acceleration));
    TestFalse(TEXT("Box rejects outside the capped local X dimension"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(501.0, 0.0, 0.0), Acceleration));
    Field.Rotation = FQuat(FVector::UpVector, PI / 4.0);
    Field.Center = FVector(2000.0, 1000.0, 500.0);
    TestTrue(TEXT("Box caps operate in rotated source-local space"),
        FOrbitFieldEvaluator::Evaluate(Field,
            Field.Center + Field.Rotation.RotateVector(FVector(475.0, 475.0, 475.0)), Acceleration));
    TestFalse(TEXT("Rotated Box rejects outside capped local Y"),
        FOrbitFieldEvaluator::Evaluate(Field,
            Field.Center + Field.Rotation.RotateVector(FVector(0.0, 510.0, 0.0)), Acceleration));
    Field.Rotation = FQuat::Identity;
    Field.Center = FVector::ZeroVector;

    Field.Falloff = EOrbitGravityFalloff::Linear;
    TestTrue(TEXT("Box linear fade also samples beyond old sphere"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(250.0, 250.0, 250.0), Acceleration));
    TestTrue(TEXT("Box linear fade uses box-relative fraction, not Euclidean radius"),
        FMath::IsNearlyEqual(Acceleration.Size(), 490.0, 0.01));
    Field.Falloff = EOrbitGravityFalloff::Constant;

    Field.VolumeShape = EOrbitVolumeShape::Sphere;
    Field.VolumeRadius = 900.0;
    TestTrue(TEXT("Sphere includes inside 500 radius"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(300.0, 300.0, 0.0), Acceleration));
    TestFalse(TEXT("Sphere excludes outside 500 radius"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(400.0, 400.0, 0.0), Acceleration));

    Field.VolumeShape = EOrbitVolumeShape::Cylinder;
    Field.VolumeRadius = 1000.0;
    Field.VolumeHalfHeight = 1200.0;
    TestTrue(TEXT("Cylinder accepts a corner of its capped radial and axial dimensions"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(400.0, 0.0, 400.0), Acceleration));
    TestFalse(TEXT("Cylinder rejects beyond capped Z"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(0.0, 0.0, 501.0), Acceleration));
    TestFalse(TEXT("Cylinder rejects beyond capped radial distance"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(400.0, 350.0, 0.0), Acceleration));

    Field.VolumeShape = EOrbitVolumeShape::Capsule;
    Field.VolumeRadius = 150.0;
    Field.VolumeHalfHeight = 600.0;
    Field.MaxDistance = 200.0;
    const FOrbitEffectiveVolumeBounds Capsule = FOrbitFieldEvaluator::GetEffectiveVolumeBounds(Field);
    TestTrue(TEXT("Capsule radius is unchanged when already smaller"),
        FMath::IsNearlyEqual(Capsule.Radius, 150.0));
    TestTrue(TEXT("Capsule cap height is limited to 200"),
        FMath::IsNearlyEqual(Capsule.HalfHeight, 200.0));
    TestTrue(TEXT("Capsule permits Z end despite being outside its radius"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(0.0, 0.0, 195.0), Acceleration));
    TestFalse(TEXT("Capsule rejects beyond Z end"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(0.0, 0.0, 205.0), Acceleration));
    TestTrue(TEXT("Capsule accepts rounded shoulder inside the cap"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(140.0, 0.0, 100.0), Acceleration));
    TestFalse(TEXT("Capsule rejects outside rounded shoulder"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(145.0, 0.0, 100.0), Acceleration));
    Field.Falloff = EOrbitGravityFalloff::Linear;
    TestTrue(TEXT("Capsule linear fade uses its local tip-to-center distance"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(0.0, 0.0, 100.0), Acceleration));
    TestTrue(TEXT("Capsule linear fade is halfway up its central axis"),
        FMath::IsNearlyEqual(Acceleration.Size(), 490.0, 0.01));

    Field.MaxDistance = 0.0;
    Field.Falloff = EOrbitGravityFalloff::Constant;
    TestTrue(TEXT("Unlimited MaxDistance leaves authored capsule height intact"),
        FOrbitFieldEvaluator::Evaluate(Field, FVector(0.0, 0.0, 590.0), Acceleration));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitControlsDefaultActivationTest,
    "Orbit.Character.ControlsAutoActivationDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitControlsDefaultActivationTest::RunTest(const FString& Parameters)
{
    const UOrbitCharacterControlsComponent* Template =
        GetDefault<UOrbitCharacterControlsComponent>();
    TestNotNull(TEXT("Controls component CDO"), Template);
    if (!Template) { return false; }
    // UActorComponent exposes the reflected bAutoActivate property, not a
    // GetAutoActivate() accessor. Read the bool property on the component CDO.
    const FBoolProperty* AutoActivateProperty = FindFProperty<FBoolProperty>(
        UActorComponent::StaticClass(), TEXT("bAutoActivate"));
    TestNotNull(TEXT("Actor Component has an Auto Activate property"), AutoActivateProperty);
    if (AutoActivateProperty)
    {
        TestTrue(TEXT("New Orbit Controls components Auto Activate"),
            AutoActivateProperty->GetPropertyValue_InContainer(Template));
    }
    TestTrue(TEXT("Movement-facing remains enabled by default"), Template->bFaceMovementDirection);
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

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitGravityFrame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitGravityFrameBasisTest,
    "Orbit.Character.GravityRelativeBasis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitGravityFrameBasisTest::RunTest(const FString& Parameters)
{
    FVector Forward, Right;
    TestTrue(TEXT("Tangent basis exists for inverted gravity"),
        FOrbitGravityFrame::MakeTangentBasis(FVector::DownVector, FVector::ForwardVector, Forward, Right));
    TestTrue(TEXT("Forward is tangent to new ground"), FMath::Abs(FVector::DotProduct(Forward, FVector::DownVector)) < 1e-6);
    TestTrue(TEXT("Right is tangent to new ground"), FMath::Abs(FVector::DotProduct(Right, FVector::DownVector)) < 1e-6);
    TestTrue(TEXT("Correct-handed tangent frame"), FVector::CrossProduct(Forward, Right).Equals(FVector::DownVector, 1e-5));

    TestTrue(TEXT("Parallel heading uses safe fallback"),
        FOrbitGravityFrame::MakeTangentBasis(FVector::ForwardVector, FVector::ForwardVector, Forward, Right));
    TestTrue(TEXT("Fallback forward is perpendicular to Up"),
        FMath::Abs(FVector::DotProduct(Forward, FVector::ForwardVector)) < 1e-6);
    TestFalse(TEXT("No basis for zero Up"),
        FOrbitGravityFrame::MakeTangentBasis(FVector::ZeroVector, FVector::ForwardVector, Forward, Right));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitGravityFrameRotationTest,
    "Orbit.Character.GravityRelativeRotation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitGravityFrameRotationTest::RunTest(const FString& Parameters)
{
    const FQuat Starting = FQuat::Identity;
    FQuat Aligned;
    const FVector Up = FVector(1.0, 1.0, 1.0).GetSafeNormal();
    TestTrue(TEXT("Can align arbitrary spherical Up"),
        FOrbitGravityFrame::MakeAlignedRotation(Up, Starting, Aligned));
    TestTrue(TEXT("Local Z matches target Up"), Aligned.GetUpVector().Equals(Up, 1e-5));
    TestTrue(TEXT("Rotation is normalized"), Aligned.IsNormalized());
    TestFalse(TEXT("Cannot align to zero Up"),
        FOrbitGravityFrame::MakeAlignedRotation(FVector::ZeroVector, Starting, Aligned));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitGravityMagnitudeTest,
    "Orbit.Character.GravityMagnitude", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitGravityMagnitudeTest::RunTest(const FString& Parameters)
{
    float Scale = -1.0f;
    TestTrue(TEXT("Earth gravity can be mapped"), FOrbitGravityFrame::MakeGravityScale(980.0, 980.0, 100.0, Scale));
    TestTrue(TEXT("Matching default gravity gives scale 1"), FMath::IsNearlyEqual(Scale, 1.0f));
    TestTrue(TEXT("Reduced field gravity can be mapped"), FOrbitGravityFrame::MakeGravityScale(245.0, 980.0, 100.0, Scale));
    TestTrue(TEXT("Quarter gravity gives scale 0.25"), FMath::IsNearlyEqual(Scale, 0.25f));
    TestTrue(TEXT("Explicit zero-g can be mapped"), FOrbitGravityFrame::MakeGravityScale(0.0, 980.0, 100.0, Scale));
    TestTrue(TEXT("Zero-g gives scale 0"), FMath::IsNearlyZero(Scale));
    TestTrue(TEXT("Upper safety clamp works"), FOrbitGravityFrame::MakeGravityScale(20000.0, 980.0, 2.0, Scale));
    TestTrue(TEXT("Safety clamp gives scale 2"), FMath::IsNearlyEqual(Scale, 2.0f));
    TestFalse(TEXT("Zero native gravity cannot supply an Orbit magnitude"),
        FOrbitGravityFrame::MakeGravityScale(980.0, 0.0, 100.0, Scale));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitGravityFrame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitNearFloorSafetyTest,
    "Orbit.Character.NearFloorSafety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitNearFloorSafetyTest::RunTest(const FString& Parameters)
{
    const auto Eligible = [](bool bFalling, double VelUp, double Sideways, double Floor)
    {
        return FOrbitGravityFrame::ShouldAttemptNearFloorLanding(bFalling, VelUp, Sideways, 100.0, Floor, 10.0);
    };
    TestTrue(TEXT("Slow descent above valid sweep range is eligible"), Eligible(true, -20.0, 5.0, 6.0));
    TestFalse(TEXT("Walking never triggers assist"), Eligible(false, -20.0, 5.0, 6.0));
    TestFalse(TEXT("Rising jump never triggers assist"), Eligible(true, 20.0, 5.0, 6.0));
    TestFalse(TEXT("Fast fall must use native landing"), Eligible(true, -500.0, 5.0, 6.0));
    TestFalse(TEXT("Fast lateral motion must use native landing"), Eligible(true, -20.0, 150.0, 6.0));
    TestFalse(TEXT("Missing ground cannot trigger assist"), Eligible(true, -20.0, 5.0, -1.0));
    TestFalse(TEXT("Out of range ground cannot trigger assist"), Eligible(true, -20.0, 5.0, 11.0));
    TestFalse(TEXT("NaN velocity must be rejected"),
        FOrbitGravityFrame::ShouldAttemptNearFloorLanding(true, std::numeric_limits<double>::quiet_NaN(), 0., 100., 5., 10.));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEquatorTangentTest,
    "Orbit.Character.EquatorTangentMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEquatorTangentTest::RunTest(const FString& Parameters)
{
    for (const FVector& Up : TArray<FVector>{ FVector::ForwardVector, -FVector::ForwardVector,
        FVector::RightVector, -FVector::RightVector, FVector::UpVector, -FVector::UpVector })
    {
        FVector Forward, Right;
        TestTrue(TEXT("Valid basis at every cardinal planet location"),
            FOrbitGravityFrame::MakeTangentBasis(Up, FVector::ForwardVector, Forward, Right));
        TestTrue(TEXT("Forward movement remains tangent"), FMath::Abs(FVector::DotProduct(Up, Forward)) < 1e-5);
        TestTrue(TEXT("Right movement remains tangent"), FMath::Abs(FVector::DotProduct(Up, Right)) < 1e-5);
        TestTrue(TEXT("Both input directions stay nonzero"), Forward.SizeSquared() > .99 && Right.SizeSquared() > .99);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitCameraTransportTest,
    "Orbit.Character.GravityCameraTransport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitCameraTransportTest::RunTest(const FString& Parameters)
{
    FVector Up = FVector::UpVector;
    FVector Heading = FVector::ForwardVector;
    for (int32 Step = 1; Step <= 180; ++Step)
    {
        const double Angle = FMath::DegreesToRadians(static_cast<double>(Step));
        const FVector NextUp(FMath::Sin(Angle), 0.0, FMath::Cos(Angle));
        const FQuat Transport = FQuat::FindBetweenNormals(Up, NextUp);
        FVector Right;
        TestTrue(TEXT("Basis remains valid through pole and equator"),
            FOrbitGravityFrame::MakeTangentBasis(NextUp, Transport.RotateVector(Heading), Heading, Right));
        TestTrue(TEXT("No tangent NaN"), !Heading.ContainsNaN() && !Right.ContainsNaN());
        TestTrue(TEXT("Tangent perpendicular to transported Up"), FMath::Abs(FVector::DotProduct(Heading, NextUp)) < 1e-5);
        Up = NextUp;
    }
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitBlueprintLibrary.h"
#include "Math/RotationMatrix.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitGravityFrameRoundTripTest,
    "Orbit.Character.GravityFrameTransformRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitGravityFrameRoundTripTest::RunTest(const FString& Parameters)
{
    for (const FVector& GravityUp : TArray<FVector>{FVector::ForwardVector,
        -FVector::ForwardVector, FVector::RightVector, -FVector::RightVector,
        FVector::UpVector, -FVector::UpVector, FVector(0.4, 0.2, 0.7).GetSafeNormal()})
    {
        FTransform Frame;
        TestTrue(TEXT("Build non-world-up frame"), UOrbitBlueprintLibrary::MakeGravityFrame(
            FVector(1000., -1200., 2000.), GravityUp, FVector::ForwardVector, Frame));
        TestTrue(TEXT("Frame local Z equals gravity Up"),
            Frame.GetUnitAxis(EAxis::Z).Equals(GravityUp, 1.e-4));
        const FTransform Local(FQuat(FVector::UpVector, 0.4), FVector(15., 22., 35.), FVector::OneVector);
        const FTransform World = UOrbitBlueprintLibrary::GravityLocalToWorldTransform(Local, Frame);
        const FTransform Back = UOrbitBlueprintLibrary::GravityWorldToLocalTransform(World, Frame);
        TestTrue(TEXT("Transform local/world round-trip location"),
            Back.GetLocation().Equals(Local.GetLocation(), 1.e-3));
        TestTrue(TEXT("Transform local/world round-trip rotation"),
            Back.GetRotation().Equals(Local.GetRotation(), 1.e-3));
    }
    FTransform Invalid;
    TestFalse(TEXT("Reject zero-Up frame"), UOrbitBlueprintLibrary::MakeGravityFrame(
        FVector::ZeroVector, FVector::ZeroVector, FVector::ForwardVector, Invalid));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitMovementFacingTangentTest,
    "Orbit.Character.MovementFacingTangent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitMovementFacingTangentTest::RunTest(const FString& Parameters)
{
    for (const FVector& Up : TArray<FVector>{FVector::ForwardVector, FVector::UpVector,
        -FVector::ForwardVector, -FVector::UpVector})
    {
        FVector Forward, Right;
        TestTrue(TEXT("Valid surface basis"), FOrbitGravityFrame::MakeTangentBasis(
            Up, FVector(1., 0.5, 0.1), Forward, Right));
        const FVector Desired = (Forward * 0.7 + Right * 0.4).GetSafeNormal();
        const FQuat Facing = FRotationMatrix::MakeFromXZ(Desired, Up).ToQuat();
        TestTrue(TEXT("Facing forward matches tangent movement"),
            Facing.GetForwardVector().Equals(Desired, 1.e-4));
        TestTrue(TEXT("Facing Up matches gravity Up"),
            Facing.GetUpVector().Equals(Up, 1.e-4));
    }
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitGravityFrame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitCharacterGravityLocomotionTest,
    "Orbit.Character.GravityRelativeLocomotion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitCharacterGravityLocomotionTest::RunTest(const FString& Parameters)
{
    for (const FVector Up : TArray<FVector>{ FVector::UpVector, -FVector::UpVector,
         FVector::ForwardVector, -FVector::ForwardVector, FVector::RightVector, -FVector::RightVector })
    {
        FVector Forward, Right;
        TestTrue(TEXT("Gravity tangent basis is available"), FOrbitGravityFrame::MakeTangentBasis(
            Up, FVector::ForwardVector, Forward, Right));
        double Speed, Fwd, Side, Dir;
        TestTrue(TEXT("Forward velocity calculates"), FOrbitGravityFrame::CalculateGravityRelativeLocomotion(
            Forward * 250.0, Up, Forward, Speed, Fwd, Side, Dir));
        TestTrue(TEXT("Forward motion remains 250 cm/s even when world Z changes"), FMath::IsNearlyEqual(Speed, 250.0, 0.001));
        TestTrue(TEXT("Forward is positive"), FMath::IsNearlyEqual(Fwd, 250.0, 0.001));
        TestTrue(TEXT("Direction is zero"), FMath::Abs(Dir) < 0.001);
        TestTrue(TEXT("Sideways velocity calculates"), FOrbitGravityFrame::CalculateGravityRelativeLocomotion(
            Right * 300.0, Up, Forward, Speed, Fwd, Side, Dir));
        TestTrue(TEXT("Right motion is 90 degrees"), FMath::IsNearlyEqual(Dir, 90.0, 0.001));
        TestTrue(TEXT("Sideways is positive"), FMath::IsNearlyEqual(Side, 300.0, 0.001));
        TestTrue(TEXT("Pure gravity-direction velocity gives zero ground speed"), FOrbitGravityFrame::CalculateGravityRelativeLocomotion(
            Up * 900.0, Up, Forward, Speed, Fwd, Side, Dir));
        TestTrue(TEXT("Vertical speed is removed"), Speed < 0.001);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitInitialDropApproachSpeedTest,
    "Orbit.Character.InitialDropAssistEligibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitInitialDropApproachSpeedTest::RunTest(const FString& Parameters)
{
    // 400 cm drop on a 196 cm/s^2 source => 396 cm/s approach, excluding old 100 cm/s gate.
    const double DropApproachSpeed = FMath::Sqrt(2.0 * 196.0 * 400.0);
    TestTrue(TEXT("400 cm drop exceeds original limit"), DropApproachSpeed > 100.0);
    TestTrue(TEXT("Updated speed limit permits source-derived initial drop"),
        FOrbitGravityFrame::ShouldAttemptNearFloorLanding(true, -DropApproachSpeed, 0.0,
            600.0, 5.0, 10.0));
    TestFalse(TEXT("Upward rebound or jump apex cannot trigger assist"),
        FOrbitGravityFrame::ShouldAttemptNearFloorLanding(true, DropApproachSpeed, 0.0,
            600.0, 5.0, 10.0));
    TestFalse(TEXT("Floor farther than user limit cannot trigger assist"),
        FOrbitGravityFrame::ShouldAttemptNearFloorLanding(true, -DropApproachSpeed, 0.0,
            600.0, 20.0, 10.0));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitPointSourceTransferTest,
    "Orbit.Field.EqualPriorityPlanetTransfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitPointSourceTransferTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition PlanetA;
    PlanetA.Shape = EOrbitGravityShape::Point;
    PlanetA.Center = FVector(-500.0, 0.0, 0.0);
    PlanetA.Strength = 980.0;
    PlanetA.MaxDistance = 1000.0;
    PlanetA.Direction = EOrbitGravityDirection::Inward;
    PlanetA.SourceId = 11;
    PlanetA.Combine = EOrbitGravityCombine::Override;
    FOrbitFieldDefinition PlanetB = PlanetA;
    PlanetB.Center = FVector(500.0, 0.0, 0.0);
    PlanetB.SourceId = 22;
    const FVector NearA(-200.0, 0.0, 0.0);
    const FVector NearB(200.0, 0.0, 0.0);
    for (int32 Order = 0; Order < 2; ++Order)
    {
        TArray<FOrbitFieldDefinition> Fields;
        Fields.Add(Order == 0 ? PlanetA : PlanetB);
        Fields.Add(Order == 0 ? PlanetB : PlanetA);
        const FOrbitGravitySample A = FOrbitFieldEvaluator::Combine(Fields, NearA);
        const FOrbitGravitySample B = FOrbitFieldEvaluator::Combine(Fields, NearB);
        TestEqual(TEXT("Nearest planet owns its side of overlap"), A.DominantSourceId, 11);
        TestEqual(TEXT("Other planet owns opposite side"), B.DominantSourceId, 22);
        TestTrue(TEXT("Near first planet pulls toward negative X"), A.Acceleration.X < 0.0);
        TestTrue(TEXT("Near second planet pulls toward positive X"), B.Acceleration.X > 0.0);
    }
    PlanetA.Priority = 10;
    TArray<FOrbitFieldDefinition> PriorityFields = {PlanetA, PlanetB};
    TestEqual(TEXT("Explicit priority still takes precedence"),
        FOrbitFieldEvaluator::Combine(PriorityFields, NearB).DominantSourceId, 11);
    PriorityFields[0].MaxDistance = 200.0;
    TestEqual(TEXT("Out-of-range high-priority source cannot take ownership"),
        FOrbitFieldEvaluator::Combine(PriorityFields, NearB).DominantSourceId, 22);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitBlueprintLibrary.h"
#include "OrbitGravitySourceComponent.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitInfluenceCompetitionTest,
    "Orbit.Field.InfluenceCompetition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitInfluenceCompetitionTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition A;
    A.Center = FVector(-500.0, 0.0, 0.0);
    A.MaxDistance = 1400.0;
    A.Strength = 980.0;
    A.SourceId = 100;
    FOrbitFieldDefinition B = A;
    B.Center = FVector(500.0, 0.0, 0.0);
    B.SourceId = 200;
    B.Strength = 300.0;
    TArray<FOrbitFieldDefinition> Fields = {A, B};

    const FVector Position(0.0, 0.0, 0.0);
    const FOrbitGravitySample Strongest = FOrbitFieldEvaluator::Combine(Fields, Position,
        EOrbitGravityChannel::Character, 1u, EOrbitOverrideResolution::StrongestInfluence);
    TestEqual(TEXT("Stronger source wins even when at same distance"), Strongest.DominantSourceId, 100);
    TestTrue(TEXT("Strongest field points left"), Strongest.Acceleration.X < -900.0);
    const FOrbitGravitySample Blend = FOrbitFieldEvaluator::Combine(Fields, Position,
        EOrbitGravityChannel::Character, 1u, EOrbitOverrideResolution::BlendedInfluence);
    TestTrue(TEXT("Blend differs from strongest"), FMath::Abs(Blend.Acceleration.X) < 980.0);
    TestTrue(TEXT("Blend retains stronger source direction"), Blend.Acceleration.X < 0.0);
    B.Strength = 980.0;
    Fields[1] = B;
    const FOrbitGravitySample Cancellation = FOrbitFieldEvaluator::Combine(Fields, Position,
        EOrbitGravityChannel::General, 1u, EOrbitOverrideResolution::BlendedInfluence);
    TestTrue(TEXT("Opposing equal fields cancel"), Cancellation.Acceleration.IsNearlyZero(0.01));
    TestTrue(TEXT("Cancellation still claims authored field influence"), Cancellation.bHasInfluence);
    TestFalse(TEXT("Null field has no valid Up"), Cancellation.bHasValidDirection);

    B.Priority = 10;
    Fields[1] = B;
    const FOrbitGravitySample Priority = FOrbitFieldEvaluator::Combine(Fields, Position,
        EOrbitGravityChannel::Character, 1u, EOrbitOverrideResolution::BlendedInfluence);
    TestEqual(TEXT("Explicit Priority overrides blend"), Priority.DominantSourceId, 200);
    TestTrue(TEXT("Higher priority points right"), Priority.Acceleration.X > 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitSourceHysteresisTest,
    "Orbit.Field.SourceHysteresis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitSourceHysteresisTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition A;
    A.Center = FVector(-500.0, 0.0, 0.0);
    A.SourceId = 100;
    A.MaxDistance = 2000.0;
    FOrbitFieldDefinition B = A;
    B.Center = FVector(500.0, 0.0, 0.0);
    B.SourceId = 200;
    TArray<FOrbitFieldDefinition> Fields = { A, B };
    const FVector SlightlyTowardB(40.0, 0.0, 0.0);
    TestEqual(TEXT("Without history B owns nearer side"),
        FOrbitFieldEvaluator::Combine(Fields, SlightlyTowardB, EOrbitGravityChannel::Character,
            1u, EOrbitOverrideResolution::StrongestInfluence).DominantSourceId, 200);
    TestEqual(TEXT("15 percent hysteresis retains previous source near midpoint"),
        FOrbitFieldEvaluator::Combine(Fields, SlightlyTowardB, EOrbitGravityChannel::Character,
            1u, EOrbitOverrideResolution::StrongestInfluence, 100, 0.15).DominantSourceId, 100);
    TestEqual(TEXT("Sufficiently stronger B overcomes hysteresis"),
        FOrbitFieldEvaluator::Combine(Fields, FVector(350.0, 0.0, 0.0), EOrbitGravityChannel::Character,
            1u, EOrbitOverrideResolution::StrongestInfluence, 100, 0.15).DominantSourceId, 200);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitSignedAxisOrientationTest,
    "Orbit.Orientation.SignedAxes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitSignedAxisOrientationTest::RunTest(const FString& Parameters)
{
    const FVector ExpectedUp = FVector(0.0, 1.0, 0.0);
    const TArray<FVector> Axes = { FVector::ForwardVector, -FVector::ForwardVector,
        FVector::RightVector, -FVector::RightVector, FVector::UpVector, -FVector::UpVector };
    for (int32 Index = 0; Index < Axes.Num(); ++Index)
    {
        FRotator Rotation;
        TestTrue(TEXT("Signed axis yields a gravity-aligned rotation"),
            UOrbitBlueprintLibrary::MakeOrbitAxisAlignedRotation(FRotator::ZeroRotator,
                ExpectedUp, static_cast<EOrbitLocalAxis>(Index), Rotation));
        TestTrue(TEXT("Selected local axis maps to desired gravity Up"),
            Rotation.Quaternion().RotateVector(Axes[Index]).Equals(ExpectedUp, 0.001));
    }
    FRotator Rotation;
    TestFalse(TEXT("Zero gravity Up must not change orientation"),
        UOrbitBlueprintLibrary::MakeOrbitAxisAlignedRotation(FRotator::ZeroRotator,
            FVector::ZeroVector, EOrbitLocalAxis::PositiveZ, Rotation));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitEmbeddedSourceDefinitionTest,
    "Orbit.Field.EmbeddedComponentDefinition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitEmbeddedSourceDefinitionTest::RunTest(const FString& Parameters)
{
    UOrbitGravitySourceComponent* Component = NewObject<UOrbitGravitySourceComponent>();
    TestNotNull(TEXT("Source component constructed without an Actor"), Component);
    if (!Component) { return false; }
    Component->Strength = 350.0;
    Component->GravityChannels.bChannel0 = false;
    Component->GravityChannels.bChannel2 = true;
    FOrbitFieldDefinition Field;
    Component->BuildDefinition(Field);
    TestTrue(TEXT("Embedded strength copied"), FMath::IsNearlyEqual(Field.Strength, 350.0));
    TestEqual(TEXT("Embedded source uses its own unique ID"), Field.SourceId, Component->GetUniqueID());
    TestEqual(TEXT("Embedded channels copied"), Field.ChannelMask, static_cast<uint8>(4u));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitPlanetaryGaitInvarianceTest,
    "Orbit.Character.Animation.PlanetaryGroundSpeedInvariance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitPlanetaryGaitInvarianceTest::RunTest(const FString& Parameters)
{
    struct FCase { FVector Up; FVector Forward; FVector Velocity; };
    // Forward running is the SAME animation state on any side of the sphere,
    // including when the running velocity is vertical in world coordinates.
    const FCase Cases[] = {
        { FVector::UpVector, FVector::ForwardVector, FVector(300, 0, 0) },
        { FVector::ForwardVector, FVector::UpVector, FVector(0, 0, 300) },
        { -FVector::ForwardVector, FVector::UpVector, FVector(0, 0, 300) },
        { FVector::RightVector, FVector::UpVector, FVector(0, 0, 300) },
        { -FVector::RightVector, FVector::UpVector, FVector(0, 0, 300) },
        { -FVector::UpVector, FVector::ForwardVector, FVector(300, 0, 0) }
    };
    for (const FCase& Case : Cases)
    {
        double Speed, ForwardSpeed, RightSpeed, Degrees;
        TestTrue(TEXT("Valid gravity-local running frame"),
            FOrbitGravityFrame::CalculateGravityRelativeLocomotion(Case.Velocity, Case.Up,
                Case.Forward, Speed, ForwardSpeed, RightSpeed, Degrees));
        TestTrue(TEXT("Ground Speed 300 independent of world up"), FMath::IsNearlyEqual(Speed, 300.0, 0.001));
        TestTrue(TEXT("Forward gait speed 300"), FMath::IsNearlyEqual(ForwardSpeed, 300.0, 0.001));
        TestTrue(TEXT("Zero strafe in forward gait"), FMath::IsNearlyZero(RightSpeed, 0.001));
        TestTrue(TEXT("Zero signed forward gait direction"), FMath::IsNearlyZero(Degrees, 0.001));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitPlanetarySidewaysGaitTest,
    "Orbit.Character.Animation.SignedStrafeAndBackwards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitPlanetarySidewaysGaitTest::RunTest(const FString& Parameters)
{
    double Speed, Forward, Side, Direction;
    const FVector Up = FVector::ForwardVector;
    const FVector Heading = FVector::UpVector;
    const FVector Right = FVector::CrossProduct(Up, Heading);
    TestTrue(TEXT("Right is valid on a sphere equator"),
        FOrbitGravityFrame::CalculateGravityRelativeLocomotion(Right * 180.0, Up, Heading,
            Speed, Forward, Side, Direction));
    TestTrue(TEXT("Rightward animation is +90"), FMath::IsNearlyEqual(Direction, 90.0, 0.001));
    TestTrue(TEXT("Side speed is positive"), FMath::IsNearlyEqual(Side, 180.0, 0.001));
    TestTrue(TEXT("Leftward animation sample"),
        FOrbitGravityFrame::CalculateGravityRelativeLocomotion(-Right * 180.0, Up, Heading,
            Speed, Forward, Side, Direction));
    TestTrue(TEXT("Leftward animation is -90"), FMath::IsNearlyEqual(Direction, -90.0, 0.001));
    TestTrue(TEXT("Backwards animation sample"),
        FOrbitGravityFrame::CalculateGravityRelativeLocomotion(-Heading * 180.0, Up, Heading,
            Speed, Forward, Side, Direction));
    TestTrue(TEXT("Backwards angle magnitude is 180"), FMath::IsNearlyEqual(FMath::Abs(Direction), 180.0, 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitPlanetaryOrientationTransportTest,
    "Orbit.Character.Orientation.TransportAroundSphere",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitPlanetaryOrientationTransportTest::RunTest(const FString& Parameters)
{
    FVector Up = FVector::ForwardVector;
    FVector Heading = FVector::UpVector;
    for (int32 Index = 1; Index <= 72; ++Index)
    {
        const double Angle = static_cast<double>(Index) * 2.0 * PI / 72.0;
        const FVector NextUp(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
        FVector NextHeading;
        TestTrue(TEXT("Transport succeeded on planet orbit"),
            FOrbitGravityFrame::TransportTangentHeading(Up, Heading, NextUp, NextHeading));
        TestTrue(TEXT("Transported heading is a unit vector"), FMath::IsNearlyEqual(NextHeading.Size(), 1.0, 0.001));
        TestTrue(TEXT("Transported heading remains tangent"), FMath::IsNearlyZero(FVector::DotProduct(NextHeading, NextUp), 0.001));
        TestTrue(TEXT("No sudden heading reversal as local Up changes"), FVector::DotProduct(Heading, NextHeading) > 0.99);
        Up = NextUp;
        Heading = NextHeading;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitPlanetaryOrientationAntipodalTest,
    "Orbit.Character.Orientation.AntipodalTransport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitPlanetaryOrientationAntipodalTest::RunTest(const FString& Parameters)
{
    FVector Heading;
    TestTrue(TEXT("Even exact opposing gravity Up directions produce a valid tangent frame"),
        FOrbitGravityFrame::TransportTangentHeading(FVector::UpVector, FVector::ForwardVector,
            FVector::DownVector, Heading));
    TestFalse(TEXT("No NaN for opposing Up vectors"), Heading.ContainsNaN());
    TestTrue(TEXT("Antipodal heading remains tangent"),
        FMath::IsNearlyZero(FVector::DotProduct(Heading, FVector::DownVector), 0.001));
    TestFalse(TEXT("Invalid zero gravity up is rejected"),
        FOrbitGravityFrame::TransportTangentHeading(FVector::UpVector, FVector::ForwardVector,
            FVector::ZeroVector, Heading));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitFacingVelocityWithoutInputTest,
    "Orbit.Character.Facing.VelocityWithoutOrbitInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitFacingVelocityWithoutInputTest::RunTest(const FString& Parameters)
{
    FVector Heading;
    const FVector Up = FVector::ForwardVector; // planet's +X equator
    TestTrue(TEXT("Native input movement can steer facing without an Orbit input callback"),
        FOrbitGravityFrame::ChooseGravityRelativeFacing(Up, FVector(0, 200, 0),
            FVector::ZeroVector, false, true, 30.0, Heading));
    TestTrue(TEXT("Facing chooses actual velocity in gravity tangent plane"),
        Heading.Equals(FVector::RightVector, 0.0001));
    TestTrue(TEXT("Vertical gravity velocity does not count as tangent motion"),
        !FOrbitGravityFrame::ChooseGravityRelativeFacing(Up, FVector(300, 0, 0),
            FVector::ZeroVector, false, true, 30.0, Heading));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitFacingFallbackToInputTest,
    "Orbit.Character.Facing.RequestedInputAndIdle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitFacingFallbackToInputTest::RunTest(const FString& Parameters)
{
    FVector Heading;
    const FVector Up = FVector::UpVector;
    TestTrue(TEXT("Recently received input supplies facing when velocity is too low"),
        FOrbitGravityFrame::ChooseGravityRelativeFacing(Up, FVector::ZeroVector,
            -FVector::ForwardVector, true, true, 30.0, Heading));
    TestTrue(TEXT("Requested input preserves backward yaw"), Heading.Equals(-FVector::ForwardVector, 0.0001));
    TestTrue(TEXT("Velocity overrides input when speed exceeds threshold"),
        FOrbitGravityFrame::ChooseGravityRelativeFacing(Up, FVector(250, 0, 300),
            -FVector::ForwardVector, true, true, 30.0, Heading));
    TestTrue(TEXT("Gravity-axis velocity is projected away before choosing heading"),
        Heading.Equals(FVector::ForwardVector, 0.0001));
    TestFalse(TEXT("No input or lateral velocity retains transported heading"),
        FOrbitGravityFrame::ChooseGravityRelativeFacing(Up, FVector::ZeroVector,
            FVector::ZeroVector, false, true, 30.0, Heading));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitFacingEveryGravityAxisTest,
    "Orbit.Character.Facing.DesiredYawEveryGravityAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitFacingEveryGravityAxisTest::RunTest(const FString& Parameters)
{
    const FVector Ups[] = {
        FVector::UpVector, FVector::DownVector, FVector::ForwardVector,
        -FVector::ForwardVector, FVector::RightVector, -FVector::RightVector
    };
    for (const FVector& Up : Ups)
    {
        const FVector Preferred = FMath::Abs(FVector::DotProduct(Up, FVector::ForwardVector)) < 0.5
            ? FVector::ForwardVector : FVector::RightVector;
        FVector Forward, Right;
        TestTrue(TEXT("Valid tangent basis"), FOrbitGravityFrame::MakeTangentBasis(
            Up, Preferred, Forward, Right));
        FQuat ForwardQuat, RightQuat, BackQuat;
        TestTrue(TEXT("Forward facing rotation"),
            FOrbitGravityFrame::MakeGravityFacingRotation(Up, Forward, ForwardQuat));
        TestTrue(TEXT("Rightward facing rotation"),
            FOrbitGravityFrame::MakeGravityFacingRotation(Up, Right, RightQuat));
        TestTrue(TEXT("Backward facing rotation"),
            FOrbitGravityFrame::MakeGravityFacingRotation(Up, -Forward, BackQuat));
        TestTrue(TEXT("Forward actor +X points along heading"),
            ForwardQuat.GetForwardVector().Equals(Forward, 0.001));
        TestTrue(TEXT("Right actor +X points along heading"),
            RightQuat.GetForwardVector().Equals(Right, 0.001));
        TestTrue(TEXT("Backward actor +X points along heading"),
            BackQuat.GetForwardVector().Equals(-Forward, 0.001));
        TestTrue(TEXT("All signed local Up orientations are preserved"),
            BackQuat.GetUpVector().Equals(Up, 0.001));
        TestTrue(TEXT("90 degree local yaw produces nonzero target angular error"),
            FMath::RadiansToDegrees(ForwardQuat.AngularDistance(RightQuat)) > 89.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitFacingSmoothingDoesNotFreezeTest,
    "Orbit.Character.Facing.SmoothingAdvancesTowardDesiredYaw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitFacingSmoothingDoesNotFreezeTest::RunTest(const FString& Parameters)
{
    for (const FVector Up : {FVector::UpVector, FVector::ForwardVector, FVector::RightVector})
    {
        FVector Forward, Right;
        TestTrue(TEXT("Valid orientation frame"),
            FOrbitGravityFrame::MakeTangentBasis(Up, FVector::UpVector, Forward, Right));
        FQuat Current, Target;
        TestTrue(TEXT("Initial rotation"),
            FOrbitGravityFrame::MakeGravityFacingRotation(Up, Forward, Current));
        TestTrue(TEXT("Turn target"),
            FOrbitGravityFrame::MakeGravityFacingRotation(Up, Right, Target));
        const double OriginalError = Current.AngularDistance(Target);
        const float DeltaTime = 1.0f / 60.0f;
        const float Alpha = 1.0f - FMath::Exp(-12.0f * DeltaTime);
        for (int32 Frame = 0; Frame < 12; ++Frame)
        {
            Current = FQuat::Slerp(Current, Target, Alpha).GetNormalized();
        }
        TestTrue(TEXT("Facing makes progress over 12 PostUpdateWork frames"),
            Current.AngularDistance(Target) < OriginalError * 0.15);
        TestTrue(TEXT("Smoothing preserves gravity Up"),
            Current.GetUpVector().Equals(Up, 0.001));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitFacingVelocitySelectsChangingYawTest,
    "Orbit.Character.Facing.TangentVelocitySteersYaw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitFacingVelocitySelectsChangingYawTest::RunTest(const FString& Parameters)
{
    const FVector Up = FVector::ForwardVector;
    FVector Heading;
    TestTrue(TEXT("First movement heading from velocity"),
        FOrbitGravityFrame::ChooseGravityRelativeFacing(Up, FVector(0, 220, 0),
            FVector::ZeroVector, false, true, 30.0, Heading));
    FQuat First, Next;
    TestTrue(TEXT("First facing rotation"),
        FOrbitGravityFrame::MakeGravityFacingRotation(Up, Heading, First));
    TestTrue(TEXT("Changed movement heading from velocity"),
        FOrbitGravityFrame::ChooseGravityRelativeFacing(Up, FVector(0, 0, 220),
            FVector::ZeroVector, false, true, 30.0, Heading));
    TestTrue(TEXT("Next facing rotation"),
        FOrbitGravityFrame::MakeGravityFacingRotation(Up, Heading, Next));
    TestTrue(TEXT("Different tangent velocity yields a different actor yaw"),
        FMath::RadiansToDegrees(First.AngularDistance(Next)) > 89.0);
    TestTrue(TEXT("Both headings retain planet outward Up"),
        First.GetUpVector().Equals(Up, 0.001) && Next.GetUpVector().Equals(Up, 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAnimationVerticalAxesTest,
    "Orbit.Character.Animation.GravityVerticalSpeedAllAxes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAnimationVerticalAxesTest::RunTest(const FString& Parameters)
{
    const FVector Ups[] = {
        FVector::UpVector, -FVector::UpVector,
        FVector::ForwardVector, -FVector::ForwardVector,
        FVector::RightVector, -FVector::RightVector,
        FVector(0.3, -0.8, 0.5).GetSafeNormal()
    };
    for (const FVector& Up : Ups)
    {
        FVector TangentForward, TangentRight;
        TestTrue(TEXT("Tangent basis is valid"),
            FOrbitGravityFrame::MakeTangentBasis(Up, FVector::ForwardVector, TangentForward, TangentRight));
        double VerticalSpeed = -1234.0;
        TestTrue(TEXT("Vertical speed succeeds for arbitrary Up"),
            FOrbitGravityFrame::CalculateGravityRelativeVerticalSpeed(
                Up * 240.0 + TangentForward * 600.0, Up, VerticalSpeed));
        TestTrue(TEXT("Rising velocity is +240 independent of world orientation"),
            FMath::IsNearlyEqual(VerticalSpeed, 240.0, 0.001));
        TestTrue(TEXT("Descending speed succeeds"),
            FOrbitGravityFrame::CalculateGravityRelativeVerticalSpeed(
                -Up * 150.0 + TangentRight * 500.0, Up, VerticalSpeed));
        TestTrue(TEXT("Descending velocity is -150 even on planet underside"),
            FMath::IsNearlyEqual(VerticalSpeed, -150.0, 0.001));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAnimationVerticalInvalidTest,
    "Orbit.Character.Animation.GravityVerticalSpeedInvalidInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAnimationVerticalInvalidTest::RunTest(const FString& Parameters)
{
    double VerticalSpeed = 987.0;
    TestFalse(TEXT("Invalid zero Up is rejected"),
        FOrbitGravityFrame::CalculateGravityRelativeVerticalSpeed(
            FVector::UpVector * 100.0, FVector::ZeroVector, VerticalSpeed));
    TestTrue(TEXT("Failure resets output to zero"), FMath::IsNearlyZero(VerticalSpeed));
    const double Bad = std::numeric_limits<double>::quiet_NaN();
    VerticalSpeed = 12.0;
    TestFalse(TEXT("NaN velocity is rejected"),
        FOrbitGravityFrame::CalculateGravityRelativeVerticalSpeed(
            FVector(Bad, 0, 0), FVector::UpVector, VerticalSpeed));
    TestTrue(TEXT("NaN failure also resets output"), FMath::IsNearlyZero(VerticalSpeed));
    float BlueprintSpeed = 999.0f;
    TestTrue(TEXT("Blueprint function gives positive speed on sideways planet"),
        UOrbitBlueprintLibrary::CalculateOrbitGravityVerticalSpeed(
            FVector(-220.0, 0.0, 300.0), FVector::ForwardVector, BlueprintSpeed));
    TestTrue(TEXT("Blueprint returns expected signed motion"),
        FMath::IsNearlyEqual(BlueprintSpeed, -220.0f, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAnimationInstanceDefaultsTest,
    "Orbit.Character.Animation.NativeInstanceDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitAnimationInstanceDefaultsTest::RunTest(const FString& Parameters)
{
    const UOrbitGravityAnimInstance* Instance = GetDefault<UOrbitGravityAnimInstance>();
    TestNotNull(TEXT("Orbit gravity AnimInstance CDO exists"), Instance);
    if (!Instance) { return false; }
    TestTrue(TEXT("Default ShouldMove ground speed threshold"),
        FMath::IsNearlyEqual(Instance->OrbitMinGroundSpeed, 3.0f));
    TestTrue(TEXT("Default ShouldMove acceleration threshold"),
        FMath::IsNearlyEqual(Instance->OrbitMinAcceleration, 0.1f));
    TestFalse(TEXT("Uninitialized sample should be invalid"), Instance->OrbitAnimationState.bValid);
    TestTrue(TEXT("Default signed vertical motion is zero"),
        FMath::IsNearlyZero(Instance->OrbitAnimationState.GravityRelativeVerticalSpeed));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitSupportedDampingTest,
    "Orbit.Physics.Rest.TangentialDamping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitSupportedDampingTest::RunTest(const FString& Parameters)
{
    const FVector GravityDown(0.0, 0.0, -1.0);
    const FVector Velocity(120.0, -80.0, -50.0);
    const FVector Lateral = OrbitPhysicsRestMath::TangentialVelocity(Velocity, GravityDown);
    TestTrue(TEXT("Gravity normal component excluded"), Lateral.Equals(FVector(120.0, -80.0, 0.0), 0.0001));
    const FVector Decel = OrbitPhysicsRestMath::TangentialDampingAcceleration(Velocity, GravityDown, 2.0);
    TestTrue(TEXT("Damping opposes tangential travel only"), Decel.Equals(FVector(-240.0, 160.0, 0.0), 0.0001));
    TestTrue(TEXT("Damping removes kinetic energy"), FVector::DotProduct(Decel, Velocity) < 0.0);
    TestTrue(TEXT("Angular damping opposes spin"),
        OrbitPhysicsRestMath::AngularDampingAcceleration(FVector(0.0, 0.0, 0.5), 2.0)
            .Equals(FVector(0.0, 0.0, -1.0), 0.0001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitRestAxisTest,
    "Orbit.Physics.Rest.ArbitraryGravityAxes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitRestAxisTest::RunTest(const FString& Parameters)
{
    const FVector Directions[] = { FVector::ForwardVector, FVector::BackwardVector,
        FVector::RightVector, FVector::LeftVector, FVector::UpVector, FVector::DownVector };
    for (const FVector& Down : Directions)
    {
        const FVector Velocity = 50.0 * Down + 60.0 * FVector::CrossProduct(Down, FVector::UpVector);
        const FVector Decel = OrbitPhysicsRestMath::TangentialDampingAcceleration(Velocity, Down, 1.5);
        TestTrue(TEXT("Damping always perpendicular to gravity"),
            FMath::Abs(FVector::DotProduct(Decel, Down)) < 0.0001);
        TestTrue(TEXT("Strong contact motion fails gentle-contact gate"),
            !OrbitPhysicsRestMath::IsGentleContact(Velocity, Down, 20.0));
        TestTrue(TEXT("Slow normal motion passes gentle-contact gate"),
            OrbitPhysicsRestMath::IsGentleContact(60.0 * FVector::CrossProduct(Down, FVector::UpVector), Down, 20.0));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitRestDisabledTest,
    "Orbit.Physics.Rest.DisabledAndInvalid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitRestDisabledTest::RunTest(const FString& Parameters)
{
    const FVector V(20.0, 40.0, 70.0);
    TestTrue(TEXT("Zero damping never applies a deceleration"),
        OrbitPhysicsRestMath::TangentialDampingAcceleration(V, FVector::DownVector, 0.0).IsNearlyZero());
    TestTrue(TEXT("Negative damping is clamped to zero"),
        OrbitPhysicsRestMath::AngularDampingAcceleration(V, -3.0).IsNearlyZero());
    TestTrue(TEXT("Undefined gravity does not add surface damping"),
        OrbitPhysicsRestMath::TangentialDampingAcceleration(V, FVector::ZeroVector, 3.0).IsNearlyZero());
    TestTrue(TEXT("Undefined gravity cannot claim gentle supporting contact"),
        !OrbitPhysicsRestMath::IsGentleContact(V, FVector::ZeroVector, 100.0));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitRestSupportRadialTest,
    "Orbit.Physics.Rest.RadialSupportAllLatitudes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitRestSupportRadialTest::RunTest(const FString& Parameters)
{
    FOrbitFieldDefinition Field;
    Field.Shape = EOrbitGravityShape::Point;
    Field.Direction = EOrbitGravityDirection::Inward;
    Field.Center = FVector(1200.0, -800.0, 450.0);
    Field.Strength = 980.0;
    const TArray<FVector> Directions = {
        FVector::ForwardVector, -FVector::ForwardVector,
        FVector::RightVector, -FVector::RightVector,
        FVector::UpVector, -FVector::UpVector,
        FVector(1.0, 1.0, 1.0).GetSafeNormal()
    };
    for (const FVector& Dir : Directions)
    {
        FVector Acceleration;
        const FVector Position = Field.Center + Dir * 950.0;
        TestTrue(TEXT("Point field evaluates off-origin and at equator"),
            FOrbitFieldEvaluator::Evaluate(Field, Position, Acceleration));
        const FVector Down = Acceleration.GetSafeNormal();
        TestTrue(TEXT("Point gravity is radial at any latitude"),
            Down.Equals(-Dir, 0.0001));
        TestTrue(TEXT("Surface normal and gravity Up agree at any latitude"),
            OrbitPhysicsRestMath::SupportUpAlignment(Down, Dir) > 0.9999);
        const FVector Tangent = FVector::CrossProduct(Dir,
            FMath::Abs(Dir.Z) < 0.9 ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
        const FVector Damp = OrbitPhysicsRestMath::TangentialDampingAcceleration(Tangent * 150.0, Down, 3.0);
        TestTrue(TEXT("Damping never has a radial/world-down component"),
            FMath::Abs(FVector::DotProduct(Damp, Dir)) < 0.0001);
        TestTrue(TEXT("Damping always opposes tangential movement"),
            FVector::DotProduct(Damp, Tangent) < 0.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitRestContactGraceTest,
    "Orbit.Physics.Rest.SupportGraceSafety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitRestContactGraceTest::RunTest(const FString& Parameters)
{
    const FVector Position(1000.0, 0.0, 0.0);
    const FVector Down(-1.0, 0.0, 0.0);
    const FVector Up(1.0, 0.0, 0.0);
    TestTrue(TEXT("Recent support tolerates very small normal drift"),
        OrbitPhysicsRestMath::MayRetainSupport(Position + Up * 0.2,
            Position, FVector::ZeroVector, Down, Up, 3.5, 5.0));
    TestFalse(TEXT("Recent support cannot mask a lifted-off body"),
        OrbitPhysicsRestMath::MayRetainSupport(Position + Up * 6.0,
            Position, FVector::ZeroVector, Down, Up, 3.5, 5.0));
    TestFalse(TEXT("Recent support cannot mask rapidly outward-travelling body"),
        OrbitPhysicsRestMath::MayRetainSupport(Position + Up * 0.2,
            Position, Up * 50.0, Down, Up, 3.5, 5.0));
    TestFalse(TEXT("Recent support cannot be retained after direction reversal"),
        OrbitPhysicsRestMath::MayRetainSupport(Position,
            Position, FVector::ZeroVector, -Down, Up, 3.5, 5.0));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
