#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitGravityDebugVolumeActor.h"
#include "OrbitGravityDebugDrawComponent.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitGravitySource.h"
#include "Components/BoxComponent.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitDebugVolumeGrid,
    "Orbit.Editor.DebugVolume.GridDensityBoundsAndBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitDebugVolumeGrid::RunTest(const FString& Parameters)
{
    const FIntVector Small = AOrbitGravityDebugVolumeActor::ComputeGridCounts(FVector(100,100,100), 1.f, 2048);
    TestEqual(TEXT("One meter per axis at one sample per meter"), Small, FIntVector(1,1,1));
    const FIntVector Larger = AOrbitGravityDebugVolumeActor::ComputeGridCounts(FVector(800,800,800), 1.f, 2048);
    TestTrue(TEXT("Increasing volume increases uniform samples"), Larger.X > Small.X && Larger.Y > Small.Y && Larger.Z > Small.Z);
    const FIntVector Denser = AOrbitGravityDebugVolumeActor::ComputeGridCounts(FVector(800,800,800), 2.f, 2048);
    TestTrue(TEXT("Increasing density increases samples"), Denser.X > Larger.X);
    const FIntVector Capped = AOrbitGravityDebugVolumeActor::ComputeGridCounts(FVector(200000,200000,200000), 10.f, 256);
    TestTrue(TEXT("Huge volume budget respected"), static_cast<int64>(Capped.X)*Capped.Y*Capped.Z <= 256);
    const FIntVector Flat = AOrbitGravityDebugVolumeActor::ComputeGridCounts(FVector(500,500,0), 1.f, 2048);
    TestEqual(TEXT("Flat box samples one Z layer"), Flat.Z, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitDebugVolumeNative,
    "Orbit.Editor.DebugVolume.NativeVisibilityAndNoCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitDebugVolumeNative::RunTest(const FString& Parameters)
{
    const AOrbitGravityDebugVolumeActor* Defaults = GetDefault<AOrbitGravityDebugVolumeActor>();
    if (!TestNotNull(TEXT("Actor defaults"), Defaults)) return false;
    TestNotNull(TEXT("Bounds box"), Defaults->BoundsBox.Get());
    TestNotNull(TEXT("Arrow primitive"), Defaults->GravityArrows.Get());
    if (Defaults->BoundsBox)
    {
        TestEqual(TEXT("Volume has no collision"), Defaults->BoundsBox->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestTrue(TEXT("Volume shape hidden in game"), Defaults->BoundsBox->bHiddenInGame);
    }
    if (Defaults->GravityArrows)
    {
        TestEqual(TEXT("Arrows do not collide"), Defaults->GravityArrows->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestFalse(TEXT("Arrows are visible in game unless hidden natively"), Defaults->GravityArrows->bHiddenInGame);
        TestTrue(TEXT("Arrow component native visibility starts on"), Defaults->GravityArrows->IsVisible());
    }
    TestTrue(TEXT("Uniform grid uses valid density"), Defaults->Density > 0.0f);
    TestTrue(TEXT("Nonzero drawing budget"), Defaults->MaxArrows > 0);
    TestTrue(TEXT("Positive arrow thickness default"), Defaults->ArrowThickness > 0.0f);
    TestEqual(TEXT("Existing actors remain automatic by default"), Defaults->EvaluationMode, EOrbitDebugVolumeEvaluationMode::Automatic);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitDebugVolumeWorld,
    "Orbit.Editor.DebugVolume.WorldFieldSamplingAndCornerResize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitDebugVolumeWorld::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create test world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    if (!TestNotNull(TEXT("World"), World)) return false;
    AOrbitGravityDebugVolumeActor* DebugActor = World->SpawnActor<AOrbitGravityDebugVolumeActor>();
    if (!TestNotNull(TEXT("Debug actor"), DebugActor)) return false;
    DebugActor->CornerA = FVector(-100);
    DebugActor->CornerB = FVector(100);
    DebugActor->Density = 1.0f;
    DebugActor->RefreshGravityPreview();
    TestEqual(TEXT("One meter cube samples eight cells"), DebugActor->LastSampleCount, 8);
    DebugActor->CornerB = FVector(300,100,100);
    DebugActor->OnConstruction(DebugActor->GetActorTransform());
    TestEqual(TEXT("Resizing corner updates sample count"), DebugActor->LastSampleCount, 16);
    TestTrue(TEXT("Bounds center follows edited corners"), DebugActor->BoundsBox->GetRelativeLocation().Equals(FVector(100,0,0), 0.01));
    TestTrue(TEXT("Bounds extent follows edited corners"), DebugActor->BoundsBox->GetUnscaledBoxExtent().Equals(FVector(200,100,100), 0.01));
    DebugActor->SetActorHiddenInGame(true);
    TestTrue(TEXT("Native Hidden In Game remains available"), DebugActor->IsHidden());
    DebugActor->SetActorHiddenInGame(false);
    DebugActor->GravityArrows->SetVisibility(false);
    TestFalse(TEXT("Native primitive visibility controls debug arrows"), DebugActor->GravityArrows->IsVisible());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitDebugVolumeManualMode,
    "Orbit.Editor.DebugVolume.ManualEvaluationAndExplicitRefresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitDebugVolumeManualMode::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create manual volume world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    if (!TestNotNull(TEXT("World"), World)) return false;
    AOrbitGravityDebugVolumeActor* Actor = World->SpawnActor<AOrbitGravityDebugVolumeActor>();
    if (!TestNotNull(TEXT("Volume"), Actor)) return false;
    Actor->EvaluationMode = EOrbitDebugVolumeEvaluationMode::Manual;
    const int32 Before = Actor->EvaluationCount;
    Actor->Tick(30.0f);
    Actor->CornerB = FVector(250, 250, 250);
    Actor->OnConstruction(Actor->GetActorTransform());
    TestEqual(TEXT("Manual tick and reconstruction do not evaluate"), Actor->EvaluationCount, Before);
    Actor->RefreshGravityPreview();
    TestEqual(TEXT("Explicit refresh evaluates exactly once"), Actor->EvaluationCount, Before + 1);
    const int32 SampleCount = Actor->LastSampleCount;
    TestTrue(TEXT("Explicit refresh captures samples"), SampleCount > 0);
    Actor->Tick(30.0f);
    TestEqual(TEXT("Manual mode does not run interval after refresh"), Actor->EvaluationCount, Before + 1);
    Actor->RedrawCachedPreview();
    TestEqual(TEXT("Redraw does not re-evaluate gravity"), Actor->EvaluationCount, Before + 1);
    TestEqual(TEXT("Redraw preserves snapshot sample count"), Actor->LastSampleCount, SampleCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitDebugVolumeAppearance,
    "Orbit.Editor.DebugVolume.CachedAppearanceRedrawAndThickness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitDebugVolumeAppearance::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create appearance world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    if (!TestNotNull(TEXT("World"), World)) return false;
    AOrbitGravityDebugVolumeActor* Actor = World->SpawnActor<AOrbitGravityDebugVolumeActor>();
    if (!TestNotNull(TEXT("Volume"), Actor)) return false;
    Actor->EvaluationMode = EOrbitDebugVolumeEvaluationMode::Manual;
    // Even without an Orbit gravity source, show-zero markers allow line geometry coverage.
    Actor->bShowZeroGravitySamples = true;
    Actor->GravityChannels = FOrbitGravityChannels();
    Actor->RefreshGravityPreview();
    const int32 Evaluations = Actor->EvaluationCount;
    const int32 SampleCount = Actor->LastSampleCount;
    Actor->ArrowThickness = 5.5f;
    Actor->ArrowColor = FLinearColor::Red;
    Actor->ArrowScale = 2.0f;
    Actor->RedrawCachedPreview();
    TestEqual(TEXT("Appearance redraw avoids field queries"), Actor->EvaluationCount, Evaluations);
    TestEqual(TEXT("Appearance redraw preserves field samples"), Actor->LastSampleCount, SampleCount);
    TestEqual(TEXT("Diagnostics match generated line count"), Actor->LastLineCount, Actor->GravityArrows->GetDebugLines().Num());
    TestTrue(TEXT("Appearance test generated line geometry"), Actor->LastLineCount > 0);
    for (const FOrbitGravityDebugLine& Line : Actor->GravityArrows->GetDebugLines())
    {
        TestTrue(TEXT("Every line receives editable arrow thickness"), FMath::IsNearlyEqual(Line.Thickness, 5.5f));
        TestEqual(TEXT("Every line receives editable arrow color"), Line.Color, FLinearColor::Red.ToFColor(true));
    }
    TestTrue(TEXT("Capturing a nonempty uniform grid"), Actor->LastSampleCount > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitDebugVolumeAutoMode,
    "Orbit.Editor.DebugVolume.AutomaticIntervalRemainsAvailable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitDebugVolumeAutoMode::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!TestTrue(TEXT("Create automatic world"), Wrapper.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = Wrapper.GetTestWorld();
    if (!TestNotNull(TEXT("World"), World)) return false;
    AOrbitGravityDebugVolumeActor* Actor = World->SpawnActor<AOrbitGravityDebugVolumeActor>();
    if (!TestNotNull(TEXT("Volume"), Actor)) return false;
    Actor->EvaluationMode = EOrbitDebugVolumeEvaluationMode::Automatic;
    Actor->RefreshInterval = 0.25f;
    const int32 Before = Actor->EvaluationCount;
    Actor->Tick(0.3f);
    TestEqual(TEXT("Automatic mode evaluates when interval expires"), Actor->EvaluationCount, Before + 1);
    Actor->Tick(0.05f);
    TestEqual(TEXT("Automatic mode does not evaluate before interval"), Actor->EvaluationCount, Before + 1);
    return true;
}
#endif
