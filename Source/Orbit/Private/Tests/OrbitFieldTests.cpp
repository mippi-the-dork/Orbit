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
