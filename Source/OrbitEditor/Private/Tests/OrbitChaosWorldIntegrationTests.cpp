// Iteration 041: actual world/Chaos regression tests for the expanded gravity path.
// These are editor-only, independent of any other plugin and save no test assets.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/AutomationCommon.h"
#include "OrbitSettings.h"
#include "OrbitGravitySource.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravityReceiverComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UObjectGlobals.h"

namespace OrbitWorldIntegrationTests
{
    // Never call SaveConfig. The complete project setting state is restored when
    // the test exits, including when a test returns early after a failure.
    struct FScopedOrbitTestSettings
    {
        UOrbitSettings* Settings = GetMutableDefault<UOrbitSettings>();
        const bool bEnablePhysicsIntegration = Settings->bEnablePhysicsIntegration;
        const bool bAutomaticRigidBodyGravity = Settings->bAutomaticRigidBodyGravity;
        const EOrbitRigidBodyGravityMethod Method = Settings->RigidBodyGravityMethod;
        const bool bEarlyOwnership = Settings->bExperimentalPreBeginPlayGravityOwnership;
        const bool bMultiplePoint = Settings->bExperimentalMultiplePointSources;
        const bool bMixed = Settings->bExperimentalMixedPointDirectionalSources;
        const bool bExpanded = Settings->bExperimentalExpandedFieldCompatibility;
        const bool bIncludeDisabled = Settings->bIncludeInitiallyGravityDisabledBodies;
        const float DiscoveryInterval = Settings->PhysicsDiscoveryInterval;
        const bool bWorldFallback = Settings->bUseWorldGravityOutsideFields;
        const EOrbitOverrideResolution Resolution = Settings->OverrideResolution;
        const bool bRest = Settings->bStabilizeRestingRigidBodies;
        const bool bDamping = Settings->bDampSupportedRigidBodyMotion;
        const bool bNativeComparison = Settings->bCompareNativeWorldGravityWhenEquivalent;

        explicit FScopedOrbitTestSettings(bool bExperimental = true)
        {
            Settings->bEnablePhysicsIntegration = true;
            Settings->bAutomaticRigidBodyGravity = true;
            Settings->RigidBodyGravityMethod = bExperimental ?
                EOrbitRigidBodyGravityMethod::ExperimentalChaosUniform : EOrbitRigidBodyGravityMethod::Standard;
            Settings->bExperimentalPreBeginPlayGravityOwnership = bExperimental;
            Settings->bExperimentalMultiplePointSources = false;
            Settings->bExperimentalMixedPointDirectionalSources = false;
            Settings->bExperimentalExpandedFieldCompatibility = bExperimental;
            Settings->bIncludeInitiallyGravityDisabledBodies = false;
            Settings->PhysicsDiscoveryInterval = 0.0f;
            Settings->bUseWorldGravityOutsideFields = false;
            Settings->OverrideResolution = EOrbitOverrideResolution::StrongestInfluence;
            Settings->bStabilizeRestingRigidBodies = false;
            Settings->bDampSupportedRigidBodyMotion = false;
            Settings->bCompareNativeWorldGravityWhenEquivalent = false;
        }

        ~FScopedOrbitTestSettings()
        {
            Settings->bEnablePhysicsIntegration = bEnablePhysicsIntegration;
            Settings->bAutomaticRigidBodyGravity = bAutomaticRigidBodyGravity;
            Settings->RigidBodyGravityMethod = Method;
            Settings->bExperimentalPreBeginPlayGravityOwnership = bEarlyOwnership;
            Settings->bExperimentalMultiplePointSources = bMultiplePoint;
            Settings->bExperimentalMixedPointDirectionalSources = bMixed;
            Settings->bExperimentalExpandedFieldCompatibility = bExpanded;
            Settings->bIncludeInitiallyGravityDisabledBodies = bIncludeDisabled;
            Settings->PhysicsDiscoveryInterval = DiscoveryInterval;
            Settings->bUseWorldGravityOutsideFields = bWorldFallback;
            Settings->OverrideResolution = Resolution;
            Settings->bStabilizeRestingRigidBodies = bRest;
            Settings->bDampSupportedRigidBodyMotion = bDamping;
            Settings->bCompareNativeWorldGravityWhenEquivalent = bNativeComparison;
        }
    };

    // FTestWorldWrapper is UE's built-in isolated world fixture. It owns the
    // world and tears it down; these tests never touch the open Editor level.
    struct FFixture
    {
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        FAutomationTestBase& Test;

        explicit FFixture(FAutomationTestBase& InTest) : Test(InTest) {}

        bool Create()
        {
            if (!Test.TestTrue(TEXT("Create isolated game test world"),
                TestWorld.CreateTestWorld(EWorldType::Game)))
            {
                return false;
            }
            World = TestWorld.GetTestWorld();
            if (!Test.TestNotNull(TEXT("Isolated test world exists"), World))
            {
                return false;
            }
            AWorldSettings* WorldSettings = World->GetWorldSettings();
            if (!Test.TestNotNull(TEXT("Isolated world settings exist"), WorldSettings))
            {
                return false;
            }
            // Eliminate unwanted initial native world-Z acceleration. All force
            // measured by the test bodies must come from the Orbit source.
            WorldSettings->bGlobalGravitySet = true;
            WorldSettings->GlobalGravityZ = 0.0f;
            return true;
        }

        AOrbitGravitySource* Source(const FVector& Position = FVector::ZeroVector)
        {
            if (!World) { return nullptr; }
            AOrbitGravitySource* Actor = World->SpawnActor<AOrbitGravitySource>(Position, FRotator::ZeroRotator);
            if (Actor)
            {
                Actor->bEnabled = true;
                Actor->bAffectPhysics = true;
                Actor->Strength = 980.0;
                Actor->Falloff = EOrbitGravityFalloff::Constant;
                Actor->MaxDistance = 0.0;
                Actor->bLimitToBox = false;
                Actor->Combine = EOrbitGravityCombine::Override;
                Actor->Priority = 0;
                Actor->Direction = EOrbitGravityDirection::Inward;
                Actor->GravityChannels.bChannel0 = true;
                Actor->GravityChannels.bChannel1 = false;
                Actor->GravityChannels.bChannel2 = false;
            }
            return Actor;
        }

        UStaticMeshComponent* Cube(const FVector& Position, bool bSimulating = true,
            const FVector& Scale = FVector(0.5, 0.5, 0.5))
        {
            if (!World) { return nullptr; }
            UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            if (!Test.TestNotNull(TEXT("Engine basic cube mesh exists"), CubeMesh)) { return nullptr; }
            AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
            if (!Test.TestNotNull(TEXT("Spawn test static-mesh actor"), Actor)) { return nullptr; }
            UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
            Mesh->SetMobility(bSimulating ? EComponentMobility::Movable : EComponentMobility::Static);
            Mesh->SetStaticMesh(CubeMesh);
            Mesh->SetWorldScale3D(Scale);
            Mesh->SetCollisionProfileName(bSimulating ? FName(TEXT("PhysicsActor")) : FName(TEXT("BlockAll")));
            Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Mesh->SetSimulatePhysics(bSimulating);
            if (bSimulating)
            {
                Mesh->SetEnableGravity(true);
                Mesh->SetLinearDamping(0.0f);
                Mesh->SetAngularDamping(0.05f);
            }
            return Mesh;
        }

        bool Begin()
        {
            return Test.TestTrue(TEXT("Begin play in isolated Chaos test world"),
                TestWorld.BeginPlayInTestWorld());
        }

        bool Step(int32 Frames = 30)
        {
            for (int32 Frame = 0; Frame < Frames; ++Frame)
            {
                if (!TestWorld.TickTestWorld(1.0f / 60.0f))
                {
                    Test.AddError(TEXT("FTestWorldWrapper failed while ticking physics"));
                    return false;
                }
            }
            return true;
        }

        bool VerifyExpanded(UStaticMeshComponent* Body)
        {
            UOrbitWorldSubsystem* Subsystem = World ? World->GetSubsystem<UOrbitWorldSubsystem>() : nullptr;
            if (!Test.TestNotNull(TEXT("Orbit runtime subsystem exists"), Subsystem)) { return false; }
            FOrbitPhysicsBodyDiagnostics Diagnostics;
            const bool bRecorded = Subsystem->GetPhysicsBodyDiagnostics(Body, Diagnostics);
            Test.TestTrue(TEXT("Live body has Orbit diagnostics"), bRecorded);
            Test.TestTrue(TEXT("Expanded Chaos mode owns the test body"),
                bRecorded && Diagnostics.bExperimentalChaosExpandedFieldGravity);
            return bRecorded && Diagnostics.bExperimentalChaosExpandedFieldGravity;
        }

        bool VerifyStandardFallback(UStaticMeshComponent* Body)
        {
            UOrbitWorldSubsystem* Subsystem = World ? World->GetSubsystem<UOrbitWorldSubsystem>() : nullptr;
            if (!Test.TestNotNull(TEXT("Orbit runtime subsystem exists"), Subsystem)) { return false; }
            FOrbitPhysicsBodyDiagnostics Diagnostics;
            const bool bRecorded = Subsystem->GetPhysicsBodyDiagnostics(Body, Diagnostics);
            Test.TestTrue(TEXT("Fallback body has Orbit diagnostics"), bRecorded);
            Test.TestTrue(TEXT("Unsupported Vortex does not use ANY experimental Chaos mode"),
                bRecorded && !Diagnostics.bExperimentalChaosExpandedFieldGravity &&
                !Diagnostics.bExperimentalChaosUniformGravity &&
                !Diagnostics.bExperimentalChaosPointGravity &&
                !Diagnostics.bExperimentalChaosMultiPointGravity &&
                !Diagnostics.bExperimentalChaosMixedPointDirectionalGravity);
            return bRecorded && !Diagnostics.bExperimentalChaosExpandedFieldGravity;
        }
    };

    bool RunSplineDirection(FAutomationTestBase& Test, EOrbitGravityDirection Direction)
    {
        FScopedOrbitTestSettings Settings;
        FFixture Fixture(Test);
        if (!Fixture.Create()) { return false; }
        AOrbitGravitySource* Source = Fixture.Source();
        if (!Test.TestNotNull(TEXT("Spline source exists"), Source)) { return false; }
        Source->Shape = EOrbitGravityShape::Spline;
        Source->Direction = Direction;
        Source->SplineSampleSpacing = 100.0;
        if (!Test.TestNotNull(TEXT("Authorable gravity spline component exists"), Source->GravitySpline.Get())) { return false; }
        TArray<FVector> Points;
        Points.Add(FVector::ZeroVector);
        Points.Add(FVector(1000.0, 0.0, 0.0));
        Source->GravitySpline->SetSplinePoints(Points, ESplineCoordinateSpace::Local, true);
        Source->GravitySpline->SetClosedLoop(false, true);
        UStaticMeshComponent* Body = Fixture.Cube(FVector(500.0, 100.0, 300.0));
        if (!Test.TestNotNull(TEXT("Spline test body exists"), Body)) { return false; }
        if (!Fixture.Begin() || !Fixture.Step(40)) { return false; }
        Fixture.VerifyExpanded(Body);
        const double VerticalSpeed = Body->GetPhysicsLinearVelocity().Z;
        if (Direction == EOrbitGravityDirection::SplineDown)
        {
            Test.TestTrue(TEXT("Spline Down accelerates in local oriented Down direction"), VerticalSpeed < -30.0);
        }
        else
        {
            Test.TestTrue(TEXT("Spline Up accelerates in local oriented Up direction"), VerticalSpeed > 30.0);
        }
        return true;
    }

    bool RunVolumeBoundary(FAutomationTestBase& Test, EOrbitVolumeShape Shape)
    {
        FScopedOrbitTestSettings Settings;
        FFixture Fixture(Test);
        if (!Fixture.Create()) { return false; }
        AOrbitGravitySource* Source = Fixture.Source();
        if (!Test.TestNotNull(TEXT("Volume source exists"), Source)) { return false; }
        Source->Shape = EOrbitGravityShape::Volume;
        Source->VolumeShape = Shape;
        Source->BoxHalfExtents = FVector(500.0);
        Source->VolumeRadius = 500.0;
        Source->VolumeHalfHeight = 500.0;
        UStaticMeshComponent* Inside = Fixture.Cube(FVector(0.0, 0.0, 200.0));
        UStaticMeshComponent* Outside = Fixture.Cube(FVector(800.0, 0.0, 200.0));
        if (!Inside || !Outside) { return false; }
        if (!Fixture.Begin() || !Fixture.Step(25)) { return false; }
        Fixture.VerifyExpanded(Inside);
        const double InsideVZ = Inside->GetPhysicsLinearVelocity().Z;
        const double OutsideVZ = Outside->GetPhysicsLinearVelocity().Z;
        Test.TestTrue(TEXT("Inside volume receives downward Chaos acceleration"), InsideVZ < -30.0);
        Test.TestTrue(TEXT("Outside volume remains in zero world gravity"), FMath::Abs(OutsideVZ) < 2.0);
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldExpandedAxisMotion,
    "Orbit.Physics.IntegrationWorld.ExpandedAxisMotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldExpandedAxisMotion::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Axis gravity source"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Axis;
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500.0, 0.0, 300.0));
    if (!Body || !Fixture.Begin() || !Fixture.Step(30)) { return false; }
    Fixture.VerifyExpanded(Body);
    const FVector Velocity = Body->GetPhysicsLinearVelocity();
    TestTrue(TEXT("Chaos Axis accelerates cube toward -X"), Velocity.X < -40.0);
    TestTrue(TEXT("Axis produces no unintended world-Z impulse"), FMath::Abs(Velocity.Z) < 5.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldExpandedSplineMotion,
    "Orbit.Physics.IntegrationWorld.ExpandedSplineUpDown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldExpandedSplineMotion::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    const bool bDown = RunSplineDirection(*this, EOrbitGravityDirection::SplineDown);
    const bool bUp = RunSplineDirection(*this, EOrbitGravityDirection::SplineUp);
    return bDown && bUp;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldExpandedVolumeMotion,
    "Orbit.Physics.IntegrationWorld.ExpandedVolumeBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldExpandedVolumeMotion::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    bool bAll = true;
    for (EOrbitVolumeShape Shape : { EOrbitVolumeShape::Box, EOrbitVolumeShape::Sphere,
                                    EOrbitVolumeShape::Cylinder, EOrbitVolumeShape::Capsule })
    {
        bAll = RunVolumeBoundary(*this, Shape) && bAll;
    }
    return bAll;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldExpandedMixedFieldMotion,
    "Orbit.Physics.IntegrationWorld.ExpandedMixedPriorityMotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldExpandedMixedFieldMotion::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Axis = Fixture.Source();
    AOrbitGravitySource* Directional = Fixture.Source();
    if (!Axis || !Directional) { return false; }
    Axis->Shape = EOrbitGravityShape::Axis;
    Axis->Combine = EOrbitGravityCombine::Additive;
    Axis->Priority = 1;
    Directional->Shape = EOrbitGravityShape::Directional;
    Directional->Combine = EOrbitGravityCombine::Override;
    Directional->Strength = 75.0;
    Directional->Priority = 0;
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500.0, 0.0, 300.0));
    if (!Body || !Fixture.Begin() || !Fixture.Step(30)) { return false; }
    Fixture.VerifyExpanded(Body);
    const FVector V = Body->GetPhysicsLinearVelocity();
    TestTrue(TEXT("High-priority Additive Axis contributes -X"), V.X < -40.0);
    TestTrue(TEXT("Lower-priority Override Directional contributes -Z"), V.Z < -5.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldExpandedVortexFallback,
    "Orbit.Physics.IntegrationWorld.VortexFallsBackToStandard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldExpandedVortexFallback::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Point;
    Source->Direction = EOrbitGravityDirection::VortexInward;
    Source->VortexPhysicsMode = EOrbitVortexPhysicsMode::ContinuousForce;
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500.0, 0.0, 300.0));
    if (!Body || !Fixture.Begin() || !Fixture.Step(30)) { return false; }
    Fixture.VerifyStandardFallback(Body);
    TestTrue(TEXT("Vortex remains physically active through Standard AddForce"),
        Body->GetPhysicsLinearVelocity().Size() > 30.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldExpandedSourceMovement,
    "Orbit.Physics.IntegrationWorld.SourceMovementReversesAcceleration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldExpandedSourceMovement::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Axis;
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500.0, 0.0, 300.0));
    if (!Body || !Fixture.Begin() || !Fixture.Step(15)) { return false; }
    Fixture.VerifyExpanded(Body);
    const double FirstVX = Body->GetPhysicsLinearVelocity().X;
    TestTrue(TEXT("Initial Axis direction points toward -X"), FirstVX < -20.0);
    Source->SetActorLocation(FVector(1000.0, 0.0, 0.0));
    if (UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>())
    {
        Subsystem->RefreshActiveSources();
    }
    if (!Fixture.Step(35)) { return false; }
    const double SecondVX = Body->GetPhysicsLinearVelocity().X;
    TestTrue(TEXT("Source relocation reverses the body's acceleration"), SecondVX > FirstVX + 100.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldNativeSleepWake,
    "Orbit.Physics.IntegrationWorld.DirectionalNaturalSleepAndWake",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldNativeSleepWake::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    // Top of floor at Z=0; 50-cm test cube comes to rest with center at ~25.
    UStaticMeshComponent* Floor = Fixture.Cube(FVector(0.0, 0.0, -50.0), false, FVector(10.0, 10.0, 1.0));
    UStaticMeshComponent* Body = Fixture.Cube(FVector(0.0, 0.0, 150.0));
    if (!Floor || !Body || !Fixture.Begin()) { return false; }
    bool bFell = false;
    bool bSlept = false;
    for (int32 Frame = 0; Frame < 600; ++Frame)
    {
        if (!Fixture.Step(1)) { return false; }
        bFell |= Body->GetComponentLocation().Z < 70.0;
        if (bFell && !Body->IsAnyRigidBodyAwake()) { bSlept = true; break; }
    }
    TestTrue(TEXT("Directional Chaos body actually fell toward the support"), bFell);
    TestTrue(TEXT("Supported cube naturally slept without Orbit rest intervention"), bSlept);
    if (!bSlept) { return false; }
    Source->SetActorRotation(FRotator(0.0, 90.0, 0.0));
    if (UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>())
    {
        Subsystem->RefreshActiveSources();
    }
    if (!Fixture.Step(40)) { return false; }
    TestTrue(TEXT("Changing gravity wakes sleeping body"), Body->IsAnyRigidBodyAwake());
    TestTrue(TEXT("After wake, body responds horizontally to new gravity"),
        FMath::Abs(Body->GetPhysicsLinearVelocity().X) > 15.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldNonDefaultChannelFallback,
    "Orbit.Physics.IntegrationWorld.ReceiverChannelFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldNonDefaultChannelFallback::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Axis;
    Source->GravityChannels.bChannel0 = false;
    Source->GravityChannels.bChannel1 = true;
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500.0, 0.0, 300.0));
    if (!Body) { return false; }
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(Body->GetOwner());
    if (!TestNotNull(TEXT("Runtime gravity receiver created"), Receiver)) { return false; }
    Receiver->GravityChannels.bChannel0 = false;
    Receiver->GravityChannels.bChannel1 = true;
    Body->GetOwner()->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    if (!Fixture.Begin() || !Fixture.Step(30)) { return false; }
    Fixture.VerifyStandardFallback(Body);
    TestTrue(TEXT("Matching non-default channel still accelerates through Standard"),
        Body->GetPhysicsLinearVelocity().X < -40.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitWorldSourceRemovalCleanup,
    "Orbit.Physics.IntegrationWorld.SourceRemovalRestoresNativeGravity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbitWorldSourceRemovalCleanup::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Axis;
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500.0, 0.0, 300.0));
    if (!Body || !Fixture.Begin() || !Fixture.Step(25)) { return false; }
    Fixture.VerifyExpanded(Body);
    const double BeforeRemovalVelocity = Body->GetPhysicsLinearVelocity().X;
    TestTrue(TEXT("Body accelerated under active Axis source"), BeforeRemovalVelocity < -40.0);
    Source->Destroy();
    if (!Fixture.Step(25)) { return false; }
    const double AfterRemovalVelocity = Body->GetPhysicsLinearVelocity().X;
    TestTrue(TEXT("Removed source stops applying Axis acceleration"),
        FMath::Abs(AfterRemovalVelocity - BeforeRemovalVelocity) < 25.0);
    TestTrue(TEXT("Native gravity-enabled property restored on source removal"),
        Body->IsGravityEnabled());
    if (UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>())
    {
        TestFalse(TEXT("Removed source releases Orbit managed-body diagnostics"),
            Subsystem->IsOrbitManagedPhysicsBody(Body));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
