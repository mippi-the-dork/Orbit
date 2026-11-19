// Iteration 041: actual world/Chaos regression tests for the expanded gravity path.
// These are editor-only, independent of any other plugin and save no test assets.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/AutomationCommon.h"
#include "OrbitSettings.h"
#include "OrbitGravitySource.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitGravityTypes.h"
#include "OrbitBlueprintLibrary.h"
#include "OrbitGravityReceiverComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
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
        const bool bChannels = Settings->bExperimentalReceiverChannelCompatibility;
        const bool bVortex = Settings->bExperimentalVortexCompatibility;
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
            Settings->bExperimentalReceiverChannelCompatibility = false;
            Settings->bExperimentalVortexCompatibility = false;
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
            Settings->bExperimentalReceiverChannelCompatibility = bChannels;
            Settings->bExperimentalVortexCompatibility = bVortex;
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
            // Keep the test mesh movable even when not simulating; Static mobility
            // emits an unrelated SetStaticMesh warning during test setup.
            Mesh->SetMobility(EComponentMobility::Movable);
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
        Source->BoxCornerA.SetTranslation(FVector(-500.0));
        Source->BoxCornerB.SetTranslation(FVector(500.0));
        Source->bBoxCornersInitialized = true;
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
    // Orbit Directional gravity follows the source's NEGATIVE UP axis.
    // Yaw around world Z leaves this direction unchanged, so tilt by pitch.
    Source->SetActorRotation(FRotator(90.0, 0.0, 0.0));
    FOrbitFieldDefinition TiltedField;
    Source->BuildDefinition(TiltedField);
    const FVector TiltedDown = TiltedField.DirectionVector.GetSafeNormal();
    if (!TestTrue(TEXT("Pitch rotation changes Directional gravity from -Z to horizontal"),
        FMath::Abs(TiltedDown.X) > 0.99 && FMath::Abs(TiltedDown.Z) < 0.01))
    {
        return false;
    }
    if (UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>())
    {
        Subsystem->RefreshActiveSources();
        const FVector SampledGravity = Subsystem->GetGravityAcceleration(
            Body->GetCenterOfMass(NAME_None), EOrbitGravityChannel::Physics);
        if (!TestTrue(TEXT("Orbit field snapshot reports the newly tilted gravity"),
            FVector::DotProduct(SampledGravity.GetSafeNormal(), TiltedDown) > 0.99))
        {
            return false;
        }
    }
    if (!Fixture.Step(40)) { return false; }
    TestTrue(TEXT("Changing gravity wakes sleeping body"), Body->IsAnyRigidBodyAwake());
    TestTrue(TEXT("After wake, body accelerates along the new gravity direction"),
        FVector::DotProduct(Body->GetPhysicsLinearVelocity(), TiltedDown) > 15.0);
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


namespace OrbitWorldIntegrationTests
{
    UOrbitGravityReceiverComponent* AttachReceiver(FAutomationTestBase& Test, UStaticMeshComponent* Body,
        bool bChannel0, bool bChannel1, bool bChannel2)
    {
        if (!Body || !Body->GetOwner()) { return nullptr; }
        UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(Body->GetOwner());
        if (!Test.TestNotNull(TEXT("Receiver component created"), Receiver)) { return nullptr; }
        Receiver->GravityChannels.bChannel0 = bChannel0;
        Receiver->GravityChannels.bChannel1 = bChannel1;
        Receiver->GravityChannels.bChannel2 = bChannel2;
        Body->GetOwner()->AddInstanceComponent(Receiver);
        Receiver->RegisterComponent();
        return Receiver;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit042LiveReceiverIsolation,
    "Orbit.Physics.IntegrationWorld.ChannelsIndependentBodiesAndUnion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit042LiveReceiverIsolation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    Settings.Settings->bExperimentalReceiverChannelCompatibility = true;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Down = Fixture.Source();
    AOrbitGravitySource* Side = Fixture.Source();
    if (!Down || !Side) { return false; }
    Down->Shape = Side->Shape = EOrbitGravityShape::Directional;
    Down->Combine = Side->Combine = EOrbitGravityCombine::Additive;
    Down->GravityChannels.bChannel0 = false;
    Down->GravityChannels.bChannel1 = true;
    Side->GravityChannels.bChannel0 = false;
    Side->GravityChannels.bChannel2 = true;
    Side->SetActorRotation(FRotator(90.0,0.0,0.0));
    FOrbitFieldDefinition SideField;
    Side->BuildDefinition(SideField);
    const FVector SideDirection = SideField.DirectionVector.GetSafeNormal();
    UStaticMeshComponent* Only1 = Fixture.Cube(FVector(0,0,700));
    UStaticMeshComponent* Only2 = Fixture.Cube(FVector(1500,0,700));
    UStaticMeshComponent* Both = Fixture.Cube(FVector(3000,0,700));
    UStaticMeshComponent* Neither = Fixture.Cube(FVector(4500,0,700));
    if (!Only1 || !Only2 || !Both || !Neither ||
        !AttachReceiver(*this,Only1,false,true,false) ||
        !AttachReceiver(*this,Only2,false,false,true) ||
        !AttachReceiver(*this,Both,false,true,true)) { return false; }
    if (!Fixture.Begin() || !Fixture.Step(36)) { return false; }
    Fixture.VerifyExpanded(Only1);
    Fixture.VerifyExpanded(Only2);
    Fixture.VerifyExpanded(Both);
    // The Blueprint/C++ sampling surface must agree with the solver's channels.
    if (UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>())
    {
        const auto A = Subsystem->SampleGravityForActor(Only1->GetOwner(),
            Only1->GetCenterOfMass(NAME_None), EOrbitGravityChannel::Physics);
        const auto B = Subsystem->SampleGravityForActor(Only2->GetOwner(),
            Only2->GetCenterOfMass(NAME_None), EOrbitGravityChannel::Physics);
        const auto AB = Subsystem->SampleGravityForActor(Both->GetOwner(),
            Both->GetCenterOfMass(NAME_None), EOrbitGravityChannel::Physics);
        TestTrue(TEXT("Shared actor API samples only channel 1 for receiver 1"),
            A.Acceleration.Equals(FVector(0,0,-980), 0.01));
        TestTrue(TEXT("Shared actor API samples only channel 2 for receiver 2"),
            B.Acceleration.Equals(SideDirection * 980.0, 0.01));
        TestTrue(TEXT("Shared actor API sums both channels for union receiver"),
            AB.Acceleration.Equals(FVector(0,0,-980)+SideDirection*980.0, 0.01));
    }
    const FVector V1 = Only1->GetPhysicsLinearVelocity();
    const FVector V2 = Only2->GetPhysicsLinearVelocity();
    const FVector VBoth = Both->GetPhysicsLinearVelocity();
    TestTrue(TEXT("Channel 1 rigid body accelerates downward"), V1.Z < -100.0);
    TestTrue(TEXT("Channel 1 rigid body has no channel 2 horizontal acceleration"), FMath::Abs(V1.X) < 35.0);
    TestTrue(TEXT("Channel 2 rigid body accelerates horizontally"), FVector::DotProduct(V2,SideDirection) > 100.0);
    TestTrue(TEXT("Channel 2 rigid body has no channel 1 downward acceleration"), FMath::Abs(V2.Z) < 35.0);
    TestTrue(TEXT("Multichannel body receives channel 1"), VBoth.Z < -100.0);
    TestTrue(TEXT("Multichannel body receives channel 2"), FVector::DotProduct(VBoth,SideDirection) > 100.0);
    TestTrue(TEXT("Default channel 0 body does not inherit either source"),
        Neither->GetPhysicsLinearVelocity().Size() < 10.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit042LiveReceiverMutation,
    "Orbit.Physics.IntegrationWorld.ChannelsRuntimeReceiverMaskChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit042LiveReceiverMutation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    Settings.Settings->bExperimentalReceiverChannelCompatibility = true;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Down = Fixture.Source();
    AOrbitGravitySource* Side = Fixture.Source();
    if (!Down || !Side) { return false; }
    Down->Shape = Side->Shape = EOrbitGravityShape::Directional;
    Down->Combine = Side->Combine = EOrbitGravityCombine::Additive;
    Down->GravityChannels.bChannel0 = false;
    Down->GravityChannels.bChannel1 = true;
    Side->GravityChannels.bChannel0 = false;
    Side->GravityChannels.bChannel2 = true;
    Side->SetActorRotation(FRotator(90,0,0));
    FOrbitFieldDefinition SideField;
    Side->BuildDefinition(SideField);
    const FVector SideDirection = SideField.DirectionVector.GetSafeNormal();
    UStaticMeshComponent* Body = Fixture.Cube(FVector(0,0,700));
    UOrbitGravityReceiverComponent* Receiver = AttachReceiver(*this,Body,false,true,false);
    if (!Receiver || !Fixture.Begin() || !Fixture.Step(24)) { return false; }
    Fixture.VerifyExpanded(Body);
    const FVector Before = Body->GetPhysicsLinearVelocity();
    TestTrue(TEXT("Before change, body is pulled down"), Before.Z < -80.0);
    Receiver->GravityChannels.bChannel1 = false;
    Receiver->GravityChannels.bChannel2 = true;
    if (!Fixture.Step(36)) { return false; }
    Fixture.VerifyExpanded(Body);
    const FVector After = Body->GetPhysicsLinearVelocity();
    TestTrue(TEXT("Changing receiver mask updates active Chaos gravity without new component"),
        FVector::DotProduct(After - Before, SideDirection) > 100.0);
    TestTrue(TEXT("Old gravity channel stops accelerating after mask switch"),
        FMath::Abs(After.Z - Before.Z) < 55.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit042LiveVortexContinuous,
    "Orbit.Physics.IntegrationWorld.VortexContinuousForceChaos",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit042LiveVortexContinuous::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    Settings.Settings->bExperimentalVortexCompatibility = true;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Point;
    Source->Direction = EOrbitGravityDirection::VortexInward;
    Source->VortexPhysicsMode = EOrbitVortexPhysicsMode::ContinuousForce;
    Source->VortexSpin = 1.0;
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500,0,0));
    if (!Body || !Fixture.Begin() || !Fixture.Step(24)) { return false; }
    Fixture.VerifyExpanded(Body);
    const FVector Velocity = Body->GetPhysicsLinearVelocity();
    TestTrue(TEXT("Continuous Vortex in solver accelerates inward"), Velocity.X < -45.0);
    TestTrue(TEXT("Continuous Vortex in solver adds tangential acceleration"), Velocity.Y < -45.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit042LiveVortexSpiral,
    "Orbit.Physics.IntegrationWorld.VortexSpiralCaptureVelocityResponse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit042LiveVortexSpiral::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    Settings.Settings->bExperimentalVortexCompatibility = true;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Point;
    Source->Direction = EOrbitGravityDirection::VortexInward;
    Source->VortexPhysicsMode = EOrbitVortexPhysicsMode::SpiralCapture;
    Source->VortexSpin = 1.0;
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500,0,0));
    if (!Body || !Fixture.Begin() || !Fixture.Step(4)) { return false; }
    Fixture.VerifyExpanded(Body);
    FOrbitFieldDefinition Field;
    Source->BuildDefinition(Field);
    for (const FVector InitialVelocity : {FVector(0,900,0), FVector(0,-900,0)})
    {
        Body->SetPhysicsLinearVelocity(InitialVelocity);
        const FVector Position = Body->GetCenterOfMass(NAME_None);
        const FVector Before = Body->GetPhysicsLinearVelocity();
        FVector ExpectedAcceleration;
        TestTrue(TEXT("Vortex captures this body state"),
            FOrbitFieldEvaluator::EvaluateForBody(Field,Position,Before,ExpectedAcceleration));
        if (!Fixture.Step(7)) { return false; }
        const FVector Delta = Body->GetPhysicsLinearVelocity() - Before;
        TestTrue(TEXT("Spiral capture accelerates toward the velocity-dependent target"),
            FVector::DotProduct(Delta,ExpectedAcceleration.GetSafeNormal()) > 10.0);
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit042RuntimeSpawnChannel,
    "Orbit.Physics.IntegrationWorld.RuntimeSpawnedChannelReceiverDiscovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit042RuntimeSpawnChannel::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    Settings.Settings->bExperimentalReceiverChannelCompatibility = true;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    Source->GravityChannels.bChannel0 = false;
    Source->GravityChannels.bChannel1 = true;
    if (!Fixture.Begin() || !Fixture.Step(3)) { return false; }
    // A component created after world BeginPlay cannot be preclaimed by the
    // normal world-start hook; discovery must register it safely at runtime.
    UStaticMeshComponent* Body = Fixture.Cube(FVector(500,0,700));
    if (!Body || !AttachReceiver(*this,Body,false,true,false) || !Fixture.Step(40)) { return false; }
    Fixture.VerifyExpanded(Body);
    TestTrue(TEXT("Runtime-spawned channel-1 receiver accelerates under Chaos after discovery"),
        Body->GetPhysicsLinearVelocity().Z < -100.0);
    return true;
}

// Iteration 043: explicit registration contract for bodies configured after BeginPlay.
// This checks that solver ownership is established synchronously, before any Step().
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit043ExplicitSpawnOwnership,
    "Orbit.Physics.IntegrationWorld.ExplicitSpawnOwnershipBeforeFirstStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit043ExplicitSpawnOwnership::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Body = Fixture.Cube(FVector(200,0,600));
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Runtime subsystem"), Subsystem) || !Body) { return false; }
    TestTrue(TEXT("Explicit claim succeeds before world ticks the spawned body"),
        Subsystem->PrepareSpawnedPhysicsBody(Body));
    TestTrue(TEXT("Body immediately appears in managed registry"),
        Subsystem->IsOrbitManagedPhysicsBody(Body));
    TestTrue(TEXT("Second preparation is idempotent"),
        Subsystem->PrepareSpawnedPhysicsBody(Body));
    if (!Fixture.Step(30)) { return false; }
    TestTrue(TEXT("Prepared body remains solver-managed after stepping"),
        Subsystem->IsOrbitManagedPhysicsBody(Body));
    TestTrue(TEXT("Prepared body accelerates in the intended direction"),
        Body->GetPhysicsLinearVelocity().Z < -100.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit043ExplicitSpawnIsolation,
    "Orbit.Physics.IntegrationWorld.ExplicitSpawnChannelIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit043ExplicitSpawnIsolation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    Settings.Settings->bExperimentalReceiverChannelCompatibility = true;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    Source->GravityChannels.bChannel0 = false;
    Source->GravityChannels.bChannel1 = true;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Matching = Fixture.Cube(FVector(200,0,700));
    UStaticMeshComponent* Mismatch = Fixture.Cube(FVector(-200,0,700));
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Matching || !Mismatch || !TestNotNull(TEXT("Runtime subsystem"), Subsystem) ||
        !AttachReceiver(*this,Matching,false,true,false)) { return false; }
    TestTrue(TEXT("Matching channel claimed immediately"),
        Subsystem->PrepareSpawnedPhysicsBody(Matching));
    TestFalse(TEXT("Mismatched channel not claimed"),
        Subsystem->PrepareSpawnedPhysicsBody(Mismatch));
    TestFalse(TEXT("Nonmatching body remains unmanaged"),
        Subsystem->IsOrbitManagedPhysicsBody(Mismatch));
    if (!Fixture.Step(30)) { return false; }
    TestTrue(TEXT("Matched body responds to channel 1"),
        Matching->GetPhysicsLinearVelocity().Z < -100.0);
    TestTrue(TEXT("Mismatched body does not receive channel 1 gravity"),
        FMath::Abs(Mismatch->GetPhysicsLinearVelocity().Z) < 15.0);
    return true;
}

// Iteration 044: a single Actor can own multiple independent Chaos rigid bodies.
// Explicit Actor preparation must register every component before any Step().
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit044ActorMultipleBodies,
    "Orbit.Physics.IntegrationWorld.ActorMultipleBodiesExplicitOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit044ActorMultipleBodies::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }

    UStaticMeshComponent* First = Fixture.Cube(FVector(200,0,700));
    if (!First) { return false; }
    AActor* Owner = First->GetOwner();
    UStaticMesh* CubeMesh = First->GetStaticMesh();
    if (!TestNotNull(TEXT("First body owner"), Owner) ||
        !TestNotNull(TEXT("Cube mesh"), CubeMesh)) { return false; }

    UStaticMeshComponent* Second = NewObject<UStaticMeshComponent>(Owner, TEXT("OrbitSecondSimulatedBody"));
    if (!TestNotNull(TEXT("Second body component"), Second)) { return false; }
    Owner->AddInstanceComponent(Second);
    Second->SetMobility(EComponentMobility::Movable);
    Second->SetStaticMesh(CubeMesh);
    Second->SetWorldScale3D(FVector(0.5,0.5,0.5));
    Second->SetCollisionProfileName(FName(TEXT("PhysicsActor")));
    Second->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Second->SetupAttachment(First);
    Second->SetRelativeLocation(FVector(300,0,0));
    Second->RegisterComponent();
    Second->SetSimulatePhysics(true);
    Second->SetEnableGravity(true);
    Second->SetLinearDamping(0.0f);

    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Runtime subsystem"), Subsystem)) { return false; }
    TestEqual(TEXT("Actor prepares both independent physics components immediately"),
        Subsystem->PrepareSpawnedPhysicsActor(Owner), 2);
    TestTrue(TEXT("First body managed"), Subsystem->IsOrbitManagedPhysicsBody(First));
    TestTrue(TEXT("Second body managed"), Subsystem->IsOrbitManagedPhysicsBody(Second));
    TestEqual(TEXT("Preparing an already claimed Actor is idempotent"),
        Subsystem->PrepareSpawnedPhysicsActor(Owner), 2);
    if (!Fixture.Step(30)) { return false; }
    TestTrue(TEXT("First body accelerates"), First->GetPhysicsLinearVelocity().Z < -100.0);
    TestTrue(TEXT("Second body accelerates independently"), Second->GetPhysicsLinearVelocity().Z < -100.0);
    return true;
}

// Non-matching channel receivers and non-simulating components must not be
// counted as successfully prepared Chaos bodies.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit044ActorPreparationEligibility,
    "Orbit.Physics.IntegrationWorld.ActorPreparationEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit044ActorPreparationEligibility::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    Settings.Settings->bExperimentalReceiverChannelCompatibility = true;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    Source->GravityChannels.bChannel0 = false;
    Source->GravityChannels.bChannel1 = true;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Matching = Fixture.Cube(FVector(100,0,700));
    UStaticMeshComponent* Mismatch = Fixture.Cube(FVector(-100,0,700));
    UStaticMeshComponent* NonSimulating = Fixture.Cube(FVector(400,0,700), false);
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Matching || !Mismatch || !NonSimulating ||
        !TestNotNull(TEXT("Runtime subsystem"), Subsystem) ||
        !AttachReceiver(*this,Matching,false,true,false)) { return false; }
    TestEqual(TEXT("Matching receiver Actor claims exactly one body"),
        Subsystem->PrepareSpawnedPhysicsActor(Matching->GetOwner()), 1);
    TestEqual(TEXT("Channel mismatch claims zero bodies"),
        Subsystem->PrepareSpawnedPhysicsActor(Mismatch->GetOwner()), 0);
    TestEqual(TEXT("Non-simulating Actor claims zero bodies"),
        Subsystem->PrepareSpawnedPhysicsActor(NonSimulating->GetOwner()), 0);
    TestFalse(TEXT("Channel mismatch remains unmanaged"), Subsystem->IsOrbitManagedPhysicsBody(Mismatch));
    TestFalse(TEXT("Non-simulating remains unmanaged"), Subsystem->IsOrbitManagedPhysicsBody(NonSimulating));
    return true;
}


// Iteration 045: releasing an explicitly prepared body when simulation is
// disabled must allow clean reacquisition when simulation resumes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit045SimulationToggle,
    "Orbit.Physics.IntegrationWorld.SimulationToggleOwnershipLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit045SimulationToggle::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Body = Fixture.Cube(FVector(0,0,700));
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Body || !TestNotNull(TEXT("Subsystem"), Subsystem)) { return false; }
    TestTrue(TEXT("Initial explicit claim"), Subsystem->PrepareSpawnedPhysicsBody(Body));
    Body->SetSimulatePhysics(false);
    if (!Fixture.Step(2)) { return false; }
    TestFalse(TEXT("Disabled body released"), Subsystem->IsOrbitManagedPhysicsBody(Body));
    Body->SetSimulatePhysics(true);
    Body->SetEnableGravity(true);
    TestTrue(TEXT("Reenabled body explicitly reclaimed"), Subsystem->PrepareSpawnedPhysicsBody(Body));
    if (!Fixture.Step(15)) { return false; }
    TestTrue(TEXT("Reenabled body responds to gravity"), Body->GetPhysicsLinearVelocity().Z < -30.0);
    return true;
}

// A new physics state on the same component must not retain the old
// particle's group/mask record, and explicit preparation must be reusable.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit045RecreatePhysicsState,
    "Orbit.Physics.IntegrationWorld.RecreatedParticleIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit045RecreatePhysicsState::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Body = Fixture.Cube(FVector(0,0,700));
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Body || !TestNotNull(TEXT("Subsystem"), Subsystem)) { return false; }
    TestTrue(TEXT("Initial Chaos claim"), Subsystem->PrepareSpawnedPhysicsBody(Body));
    Body->RecreatePhysicsState();
    if (!Fixture.Step(3)) { return false; }
    TestTrue(TEXT("Replacement particle can be claimed"), Subsystem->PrepareSpawnedPhysicsBody(Body));
    if (!Fixture.Step(15)) { return false; }
    TestTrue(TEXT("Recreated body responds to gravity"), Body->GetPhysicsLinearVelocity().Z < -30.0);
    return true;
}

// Iteration 048: explicit preparation must not reuse an obsolete particle record
// when the physics state is recreated *without* an intervening Orbit tick.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit048ImmediateParticleRecreation,
    "Orbit.Physics.IntegrationWorld.ImmediateRecreatedParticlePreparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit048ImmediateParticleRecreation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Body = Fixture.Cube(FVector(0,0,700));
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Body || !TestNotNull(TEXT("Subsystem"), Subsystem)) { return false; }
    TestTrue(TEXT("Initial explicit claim succeeds"), Subsystem->PrepareSpawnedPhysicsBody(Body));
    Body->RecreatePhysicsState();
    TestTrue(TEXT("Replacement explicitly prepared without a world tick"),
        Subsystem->PrepareSpawnedPhysicsBody(Body));
    TestTrue(TEXT("Repeat preparation remains idempotent"),
        Subsystem->PrepareSpawnedPhysicsBody(Body));
    if (!Fixture.Step(15)) { return false; }
    TestTrue(TEXT("Replacement receives gravity"), Body->GetPhysicsLinearVelocity().Z < -30.0);
    return true;
}

// A disabled body must not be reported as Chaos-owned just because a record
// remains from an earlier successful explicit preparation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit048NoStaleDisabledClaim,
    "Orbit.Physics.IntegrationWorld.DisabledBodyPreparationRejectsStaleClaim",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit048NoStaleDisabledClaim::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!Source) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Body = Fixture.Cube(FVector(0,0,700));
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Body || !TestNotNull(TEXT("Subsystem"), Subsystem)) { return false; }
    TestTrue(TEXT("Initial explicit claim succeeds"), Subsystem->PrepareSpawnedPhysicsBody(Body));
    Body->SetSimulatePhysics(false);
    TestFalse(TEXT("Disabled body cannot reuse a prior ownership claim"),
        Subsystem->PrepareSpawnedPhysicsBody(Body));
    TestFalse(TEXT("Disabled body has no managed record"),
        Subsystem->IsOrbitManagedPhysicsBody(Body));
    Body->SetSimulatePhysics(true);
    Body->SetEnableGravity(true);
    TestTrue(TEXT("Reenabled body can be prepared before a world tick"),
        Subsystem->PrepareSpawnedPhysicsBody(Body));
    return true;
}

// Iteration 050: the optional Receiver offers an explicit, bounded automatic
// early-ownership lifecycle at its BeginPlay (not arbitrary component spawns).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit050ReceiverBeginPlayOwnership,
    "Orbit.Physics.IntegrationWorld.ReceiverBeginPlayAutomaticPreparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit050ReceiverBeginPlayOwnership::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Directional source"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }

    UStaticMeshComponent* Mesh = Fixture.Cube(FVector(0,0,700));
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Mesh || !TestNotNull(TEXT("World subsystem"), Subsystem)) { return false; }
    // No Step() between mesh creation and Receiver registration. The actor
    // is already playing, so RegisterComponent invokes Receiver BeginPlay.
    TestFalse(TEXT("Fresh mesh not claimed before receiver opt-in"),
        Subsystem->IsOrbitManagedPhysicsBody(Mesh));
    AActor* Owner = Mesh->GetOwner();
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(
        Owner, TEXT("OrbitAutoPreparationReceiver"));
    if (!TestNotNull(TEXT("Receiver"), Receiver)) { return false; }
    Receiver->bPreparePhysicsBodiesOnBeginPlay = true;
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    TestTrue(TEXT("Receiver began play"), Receiver->HasBegunPlay());
    TestTrue(TEXT("Receiver synchronously prepared physics body without a world tick"),
        Subsystem->IsOrbitManagedPhysicsBody(Mesh));
    TestEqual(TEXT("Repreparation is idempotent"), Receiver->PreparePhysicsBodiesNow(), 1);
    if (!Fixture.Step(25)) { return false; }
    TestTrue(TEXT("Auto-prepared mesh accelerates in Chaos"),
        Mesh->GetPhysicsLinearVelocity().Z < -30.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit050ReceiverDefaultAndLatePreparation,
    "Orbit.Physics.IntegrationWorld.ReceiverDefaultOffAndLatePreparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit050ReceiverDefaultAndLatePreparation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Directional source"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Mesh = Fixture.Cube(FVector(0,0,700));
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Mesh || !TestNotNull(TEXT("World subsystem"), Subsystem)) { return false; }
    AActor* Owner = Mesh->GetOwner();
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(
        Owner, TEXT("OrbitManualPreparationReceiver"));
    if (!TestNotNull(TEXT("Receiver"), Receiver)) { return false; }
    TestFalse(TEXT("Receiver automatic ownership is opt-in"),
        Receiver->bPreparePhysicsBodiesOnBeginPlay);
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    TestTrue(TEXT("Receiver began play"), Receiver->HasBegunPlay());
    TestFalse(TEXT("Default receiver doesn't change physics ownership"),
        Subsystem->IsOrbitManagedPhysicsBody(Mesh));
    TestEqual(TEXT("Explicit late preparation works without a tick"),
        Receiver->PreparePhysicsBodiesNow(), 1);
    TestTrue(TEXT("Late prepared mesh is owned"),
        Subsystem->IsOrbitManagedPhysicsBody(Mesh));
    Mesh->SetSimulatePhysics(false);
    TestEqual(TEXT("Disabled body cannot use stale ownership"),
        Receiver->PreparePhysicsBodiesNow(), 0);
    return true;
}

// Iteration 051: opt-in Pre-Physics monitoring for late body lifecycle changes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit051ReceiverLateAddition,
    "Orbit.Physics.IntegrationWorld.ReceiverAutoPrepareLateAddition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit051ReceiverLateAddition::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Directional source"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Mesh = Fixture.Cube(FVector(0,0,700));
    if (!TestNotNull(TEXT("Late mesh"), Mesh)) { return false; }
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("World subsystem"), Subsystem)) { return false; }
    AActor* Owner = Mesh->GetOwner();
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(
        Owner, TEXT("OrbitLifecycleReceiver"));
    if (!TestNotNull(TEXT("Receiver"), Receiver)) { return false; }
    Receiver->bAutoPrepareChangedPhysicsBodies = true;
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    TestFalse(TEXT("BeginPlay preparation stays independently off"),
        Receiver->bPreparePhysicsBodiesOnBeginPlay);
    TestFalse(TEXT("Late body not yet claimed"), Subsystem->IsOrbitManagedPhysicsBody(Mesh));
    // Directly exercise the Pre-Physics callback without a world step. A normal
    // fixture tick cannot distinguish this path from the periodic discoverer.
    Receiver->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Lifecycle watch claims late body immediately on its tick"),
        Subsystem->IsCurrentExperimentalBodyOwned(Mesh));
    Receiver->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Stable body remains owned after repeated watch"),
        Subsystem->IsCurrentExperimentalBodyOwned(Mesh));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit051ReceiverSimulationRecreate,
    "Orbit.Physics.IntegrationWorld.ReceiverAutoPrepareSimulationRecreate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit051ReceiverSimulationRecreate::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Directional source"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Mesh = Fixture.Cube(FVector(0,0,700));
    if (!TestNotNull(TEXT("Mesh"), Mesh)) { return false; }
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("World subsystem"), Subsystem)) { return false; }
    AActor* Owner = Mesh->GetOwner();
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(
        Owner, TEXT("OrbitRecreationReceiver"));
    if (!TestNotNull(TEXT("Receiver"), Receiver)) { return false; }
    Receiver->bAutoPrepareChangedPhysicsBodies = true;
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    Receiver->TickComponent(1.0f/60.0f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Initially claimed"), Subsystem->IsCurrentExperimentalBodyOwned(Mesh));
    Mesh->SetSimulatePhysics(false);
    Mesh->SetSimulatePhysics(true);
    Mesh->SetEnableGravity(true);
    Receiver->TickComponent(1.0f/60.0f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Recreated body is owned before normal world tick"),
        Subsystem->IsCurrentExperimentalBodyOwned(Mesh));
    Receiver->bAutoPrepareChangedPhysicsBodies = false;
    TestFalse(TEXT("Lifecycle watch can be disabled independently"),
        Receiver->bAutoPrepareChangedPhysicsBodies);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Iterations 060-062 must also be excluded from non-automation builds.
#if WITH_DEV_AUTOMATION_TESTS

// Iteration 060: explicit, receiver-scoped preparation lets Blueprint spawn
// workflows claim a newly enabled rigid body before the next physics step.
// It must not allow a receiver to claim a mesh belonging to another Actor.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit060ReceiverScopedBodyPreparation,
    "Orbit.Physics.IntegrationWorld.ReceiverScopedBodyPreparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit060ReceiverScopedBodyPreparation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Directional source"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    UStaticMeshComponent* Mesh = Fixture.Cube(FVector(0,0,700));
    UStaticMeshComponent* Foreign = Fixture.Cube(FVector(400,0,700));
    if (!TestNotNull(TEXT("Owned mesh"), Mesh) ||
        !TestNotNull(TEXT("Unrelated mesh"), Foreign)) { return false; }
    AActor* Owner = Mesh->GetOwner();
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(
        Owner, TEXT("OrbitScopedPreparationReceiver"));
    if (!TestNotNull(TEXT("Receiver"), Receiver)) { return false; }
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Subsystem"), Subsystem)) { return false; }

    TestFalse(TEXT("Cannot prepare unrelated Actor's component"),
        Receiver->PreparePhysicsBodyNow(Foreign));
    TestFalse(TEXT("Cannot prepare null component"),
        Receiver->PreparePhysicsBodyNow(nullptr));
    TestFalse(TEXT("Neither rejection claims foreign body"),
        Subsystem->IsCurrentExperimentalBodyOwned(Foreign));
    TestTrue(TEXT("Explicit owner-scoped preparation succeeds"),
        Receiver->PreparePhysicsBodyNow(Mesh));
    TestTrue(TEXT("Current Chaos particle is claimed immediately"),
        Subsystem->IsCurrentExperimentalBodyOwned(Mesh));
    TestTrue(TEXT("Repeated request is idempotent"),
        Receiver->PreparePhysicsBodyNow(Mesh));
    Mesh->SetSimulatePhysics(false);
    TestFalse(TEXT("Disabled body cannot be prepared"),
        Receiver->PreparePhysicsBodyNow(Mesh));
    Mesh->SetSimulatePhysics(true);
    Mesh->SetEnableGravity(true);
    TestTrue(TEXT("Re-enabled body is prepared on explicit request"),
        Receiver->PreparePhysicsBodyNow(Mesh));
    return true;
}

// Iteration 061: safe opt-in boundary. This intentionally does not manufacture
// physics-asset particles; multi-body ownership needs real ragdoll fixture validation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit061ArticulatedOwnershipGate,
    "Orbit.Physics.IntegrationWorld.ArticulatedOwnershipGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit061ArticulatedOwnershipGate::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Source"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin()) { return false; }
    AActor* Owner = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(Owner);
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner);
    Owner->AddInstanceComponent(Mesh);
    Mesh->RegisterComponent();
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Subsystem"), Subsystem)) { return false; }
    TestFalse(TEXT("Existing receivers do not opt in automatically"),
        Receiver->bEnableArticulatedBodyOwnership);
    TestEqual(TEXT("Default-off receiver refuses skeletal ownership"),
        Receiver->PrepareArticulatedPhysicsBodiesNow(Mesh), 0);
    Receiver->bEnableArticulatedBodyOwnership = true;
    TestEqual(TEXT("No physics asset means no bodies claimed"),
        Receiver->PrepareArticulatedPhysicsBodiesNow(Mesh), 0);
    TestEqual(TEXT("Null skeletal component safely rejected"),
        Receiver->PrepareArticulatedPhysicsBodiesNow(nullptr), 0);
    TestEqual(TEXT("No body records created for invalid asset"),
        Subsystem->PrepareArticulatedPhysicsBodies(Mesh), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit061ArticulatedStandardIsolation,
    "Orbit.Physics.IntegrationWorld.ArticulatedStandardIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit061ArticulatedStandardIsolation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings(false);
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Source"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    if (!Fixture.Begin()) { return false; }
    AActor* Owner = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner);
    Owner->AddInstanceComponent(Mesh);
    Mesh->RegisterComponent();
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(Owner);
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    Receiver->bEnableArticulatedBodyOwnership = true;
    TestEqual(TEXT("Standard mode always rejects articulated Experimental ownership"),
        Receiver->PrepareArticulatedPhysicsBodiesNow(Mesh), 0);
    return true;
}

// Iteration 062: deterministic articulated ownership cleanup surface.
// These tests do not pretend to simulate a ragdoll without a Physics Asset.
// Real 22-body Chaos integration was manually validated in Iteration 061.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit062ArticulatedReleaseValidation,
    "Orbit.Physics.IntegrationWorld.ArticulatedReleaseValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit062ArticulatedReleaseValidation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    if (!TestNotNull(TEXT("Directional gravity source"), Fixture.Source())) { return false; }
    if (!Fixture.Begin()) { return false; }

    AActor* Owner = Fixture.World->SpawnActor<AActor>();
    AActor* Other = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Owner"), Owner) || !TestNotNull(TEXT("Other"), Other)) { return false; }
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(Owner);
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    Receiver->bEnableArticulatedBodyOwnership = true;
    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner);
    Owner->AddInstanceComponent(Mesh);
    Mesh->RegisterComponent();
    USkeletalMeshComponent* ForeignMesh = NewObject<USkeletalMeshComponent>(Other);
    Other->AddInstanceComponent(ForeignMesh);
    ForeignMesh->RegisterComponent();
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Subsystem"), Subsystem)) { return false; }

    TestEqual(TEXT("No initial articulated claims"), Receiver->GetArticulatedOwnedBodyCount(Mesh), 0);
    TestEqual(TEXT("Empty articulated bone query"), Receiver->GetArticulatedOwnedBoneNames(Mesh).Num(), 0);
    TestEqual(TEXT("Foreign mesh bone query stays isolated"),
        Receiver->GetArticulatedOwnedBoneNames(ForeignMesh).Num(), 0);
    TestEqual(TEXT("Runtime opt-out releases immediately"),
        Receiver->SetArticulatedOwnershipActive(false), 0);
    TestFalse(TEXT("Runtime opt-out updates receiver flag"), Receiver->bEnableArticulatedBodyOwnership);
    TestEqual(TEXT("Runtime opt-in without preparation is safe"),
        Receiver->SetArticulatedOwnershipActive(true, false), 0);
    TestTrue(TEXT("Runtime opt-in updates receiver flag"), Receiver->bEnableArticulatedBodyOwnership);
    TestEqual(TEXT("Runtime opt-in with no Physics Asset is safe"),
        Receiver->SetArticulatedOwnershipActive(true, true), 0);
    TestEqual(TEXT("Invalid skeletal input does not release claims"),
        Receiver->ReleaseArticulatedPhysicsBodiesNow(nullptr), 0);
    TestEqual(TEXT("Foreign skeletal mesh cannot be released by this receiver"),
        Receiver->ReleaseArticulatedPhysicsBodiesNow(ForeignMesh), 0);
    TestEqual(TEXT("Foreign skeletal mesh cannot be queried by this receiver"),
        Receiver->GetArticulatedOwnedBodyCount(ForeignMesh), 0);
    TestEqual(TEXT("Skeletal body without physics asset has no claims to release"),
        Receiver->ReleaseArticulatedPhysicsBodiesNow(Mesh), 0);
    TestEqual(TEXT("Repeated release remains safe"), Receiver->ReleaseArticulatedPhysicsBodiesNow(Mesh), 0);
    TestEqual(TEXT("Subsystem rejects null release"), Subsystem->ReleaseArticulatedPhysicsBodies(nullptr), 0);
    TestEqual(TEXT("Subsystem rejects null count"), Subsystem->GetArticulatedOwnedBodyCount(nullptr), 0);
    Receiver->UnregisterComponent();
    TestEqual(TEXT("Unregister cleanup remains safe"), Subsystem->GetArticulatedOwnedBodyCount(Mesh), 0);
    Subsystem->ReleaseArticulatedClaimsForActor(Owner);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit062ArticulatedPartialSimulationSafety,
    "Orbit.Physics.IntegrationWorld.ArticulatedPartialSimulationSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit062ArticulatedPartialSimulationSafety::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    if (!TestNotNull(TEXT("Directional gravity source"), Fixture.Source())) { return false; }
    if (!Fixture.Begin()) { return false; }
    AActor* Owner = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(Owner);
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    Receiver->bEnableArticulatedBodyOwnership = true;
    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner);
    Owner->AddInstanceComponent(Mesh);
    Mesh->RegisterComponent();
    TestEqual(TEXT("No bodies are claimed without simulated PhysicsAsset instances"),
        Receiver->PrepareArticulatedPhysicsBodiesNow(Mesh), 0);
    TestEqual(TEXT("A no-body prepare must not create ownership"),
        Receiver->GetArticulatedOwnedBodyCount(Mesh), 0);
    Mesh->SetSimulatePhysics(false);
    TestEqual(TEXT("Disabling simulation leaves no skeletal ownership"),
        Receiver->PrepareArticulatedPhysicsBodiesNow(Mesh), 0);
    TestEqual(TEXT("Explicit cleanup after partial/disabled simulation is safe"),
        Receiver->ReleaseArticulatedPhysicsBodiesNow(Mesh), 0);
    Receiver->bEnableArticulatedBodyOwnership = false;
    TestEqual(TEXT("Receiver opt-out cannot prepare articulated bodies"),
        Receiver->PrepareArticulatedPhysicsBodiesNow(Mesh), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit062ArticulatedStandardCleanupIsolation,
    "Orbit.Physics.IntegrationWorld.ArticulatedStandardCleanupIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit062ArticulatedStandardCleanupIsolation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings(false);
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    if (!TestNotNull(TEXT("Directional gravity source"), Fixture.Source())) { return false; }
    if (!Fixture.Begin()) { return false; }
    AActor* Owner = Fixture.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
    UOrbitGravityReceiverComponent* Receiver = NewObject<UOrbitGravityReceiverComponent>(Owner);
    Owner->AddInstanceComponent(Receiver);
    Receiver->RegisterComponent();
    Receiver->bEnableArticulatedBodyOwnership = true;
    USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner);
    Owner->AddInstanceComponent(Mesh);
    Mesh->RegisterComponent();
    TestEqual(TEXT("Standard mode still rejects articulated prepare"),
        Receiver->PrepareArticulatedPhysicsBodiesNow(Mesh), 0);
    TestEqual(TEXT("Standard mode reports no experimental skeletal claims"),
        Receiver->GetArticulatedOwnedBodyCount(Mesh), 0);
    TestEqual(TEXT("Standard mode release is a harmless no-op"),
        Receiver->ReleaseArticulatedPhysicsBodiesNow(Mesh), 0);
    return true;
}


// Iteration 063: solver-backed articulated *constraint* fixture. Two separate
// Chaos particles are constrained together and prepared independently. This
// exercises real Chaos dynamics and particle lifecycle, but deliberately does
// not claim to emulate a skeletal mesh PhysicsAsset.
namespace Orbit063
{
    struct FConstrainedPair
    {
        UStaticMeshComponent* First = nullptr;
        UStaticMeshComponent* Second = nullptr;
        UPhysicsConstraintComponent* Joint = nullptr;

        bool Create(OrbitWorldIntegrationTests::FFixture& Fixture, const FVector& Start)
        {
            if (!Fixture.World) { return false; }
            UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            if (!Fixture.Test.TestNotNull(TEXT("Engine fixture cube mesh"), Cube)) { return false; }

            AActor* Owner = Fixture.World->SpawnActor<AActor>(Start, FRotator::ZeroRotator);
            if (!Fixture.Test.TestNotNull(TEXT("Constrained fixture actor"), Owner)) { return false; }
            USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("Orbit063FixtureRoot"));
            Owner->AddInstanceComponent(Root);
            Owner->SetRootComponent(Root);
            Root->RegisterComponent();
            Root->SetWorldLocation(Start);

            auto MakeBody = [&](const FName Name, const FVector& Offset) -> UStaticMeshComponent*
            {
                UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Owner, Name);
                Owner->AddInstanceComponent(Body);
                Body->SetMobility(EComponentMobility::Movable);
                Body->SetupAttachment(Root);
                Body->SetRelativeLocation(Offset);
                Body->SetStaticMesh(Cube);
                Body->SetWorldScale3D(FVector(0.3, 0.3, 0.3));
                Body->SetCollisionProfileName(FName(TEXT("PhysicsActor")));
                Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                Body->RegisterComponent();
                Body->SetSimulatePhysics(true);
                Body->SetEnableGravity(true);
                Body->SetLinearDamping(0.0f);
                Body->SetAngularDamping(0.05f);
                return Body;
            };

            First = MakeBody(FName(TEXT("Orbit063BodyA")), FVector(-65.0, 0.0, 0.0));
            Second = MakeBody(FName(TEXT("Orbit063BodyB")), FVector(65.0, 0.0, 0.0));
            if (!Fixture.Test.TestNotNull(TEXT("First Chaos body"), First) ||
                !Fixture.Test.TestNotNull(TEXT("Second Chaos body"), Second)) { return false; }
            Joint = NewObject<UPhysicsConstraintComponent>(Owner, TEXT("Orbit063Joint"));
            Owner->AddInstanceComponent(Joint);
            Joint->SetupAttachment(Root);
            Joint->SetRelativeLocation(FVector::ZeroVector);
            Joint->SetDisableCollision(true);
            Joint->RegisterComponent();
            Joint->SetConstrainedComponents(First, NAME_None, Second, NAME_None);
            return Fixture.Test.TestTrue(TEXT("Live Chaos constraint created"),
                Joint->ConstraintInstance.IsValidConstraintInstance());
        }

        bool HasSimulatingBodies() const
        {
            return First && Second && First->IsSimulatingPhysics() && Second->IsSimulatingPhysics();
        }

        double Separation() const
        {
            return First && Second ? FVector::Dist(First->GetComponentLocation(), Second->GetComponentLocation()) : 1e12;
        }
    };

    bool Setup(FAutomationTestBase& Test, OrbitWorldIntegrationTests::FFixture& Fixture,
        AOrbitGravitySource*& Source, FConstrainedPair& Pair)
    {
        if (!Fixture.Create()) { return false; }
        Source = Fixture.Source();
        if (!Test.TestNotNull(TEXT("Directional Orbit source"), Source)) { return false; }
        Source->Shape = EOrbitGravityShape::Directional;
        if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
        if (!Pair.Create(Fixture, FVector(0.0, 0.0, 1000.0))) { return false; }
        return Test.TestTrue(TEXT("Both fixture Chaos bodies simulate"), Pair.HasSimulatingBodies());
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit063ConstrainedChaosGravity,
    "Orbit.Physics.IntegrationWorld.ConstrainedPairChaosGravity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit063ConstrainedChaosGravity::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    AOrbitGravitySource* Source = nullptr;
    Orbit063::FConstrainedPair Pair;
    if (!Orbit063::Setup(*this, Fixture, Source, Pair)) { return false; }
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Runtime subsystem"), Subsystem)) { return false; }
    TestTrue(TEXT("First Chaos particle explicitly owned"), Subsystem->PrepareSpawnedPhysicsBody(Pair.First));
    TestTrue(TEXT("Second Chaos particle explicitly owned"), Subsystem->PrepareSpawnedPhysicsBody(Pair.Second));
    if (!Fixture.Step(30)) { return false; }
    TestTrue(TEXT("First constrained body receives Orbit gravity"),
        Pair.First->GetPhysicsLinearVelocity().Z < -70.0);
    TestTrue(TEXT("Second constrained body receives Orbit gravity"),
        Pair.Second->GetPhysicsLinearVelocity().Z < -70.0);
    TestTrue(TEXT("Physical joint stays valid during Orbit acceleration"),
        Pair.Joint->ConstraintInstance.IsValidConstraintInstance());
    TestTrue(TEXT("Bodies remain near their initially constrained spacing"),
        Pair.Separation() < 300.0);
    TestTrue(TEXT("Both Chaos particles remain managed"),
        Subsystem->IsOrbitManagedPhysicsBody(Pair.First) &&
        Subsystem->IsOrbitManagedPhysicsBody(Pair.Second));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit063ConstrainedChaosRecreation,
    "Orbit.Physics.IntegrationWorld.ConstrainedPairParticleRecreation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit063ConstrainedChaosRecreation::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    AOrbitGravitySource* Source = nullptr;
    Orbit063::FConstrainedPair Pair;
    if (!Orbit063::Setup(*this, Fixture, Source, Pair)) { return false; }
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Runtime subsystem"), Subsystem)) { return false; }
    TestTrue(TEXT("Initially claim first particle"), Subsystem->PrepareSpawnedPhysicsBody(Pair.First));
    TestTrue(TEXT("Initially claim second particle"), Subsystem->PrepareSpawnedPhysicsBody(Pair.Second));
    Pair.Second->SetSimulatePhysics(false);
    // IsOrbitManagedPhysicsBody reports a tracked entry, which can remain until
    // the subsystem's next tick. The live Chaos particle ownership check must
    // reject a disabled component immediately, even before the stale record is swept.
    TestFalse(TEXT("Disabled particle cannot retain a valid live Chaos claim"),
        Subsystem->IsCurrentExperimentalBodyOwned(Pair.Second));
    TestFalse(TEXT("Preparing a disabled particle rejects and cleans stale ownership"),
        Subsystem->PrepareSpawnedPhysicsBody(Pair.Second));
    TestFalse(TEXT("Disabled particle record cleared after explicit preparation"),
        Subsystem->IsOrbitManagedPhysicsBody(Pair.Second));
    TestTrue(TEXT("Other constrained particle remains managed"),
        Subsystem->IsOrbitManagedPhysicsBody(Pair.First));
    Pair.Second->SetSimulatePhysics(true);
    Pair.Second->SetEnableGravity(true);
    Pair.Joint->SetConstrainedComponents(Pair.First, NAME_None, Pair.Second, NAME_None);
    TestTrue(TEXT("Reenabled particle reacquired without an intervening tick"),
        Subsystem->PrepareSpawnedPhysicsBody(Pair.Second));
    Pair.Second->RecreatePhysicsState();
    Pair.Joint->SetConstrainedComponents(Pair.First, NAME_None, Pair.Second, NAME_None);
    TestTrue(TEXT("Replacement Chaos particle reacquired before next tick"),
        Subsystem->PrepareSpawnedPhysicsBody(Pair.Second));
    TestTrue(TEXT("Reacquisition is idempotent"),
        Subsystem->PrepareSpawnedPhysicsBody(Pair.Second));
    if (!Fixture.Step(20)) { return false; }
    TestTrue(TEXT("Both recreated constrained Chaos bodies remain managed"),
        Subsystem->IsOrbitManagedPhysicsBody(Pair.First) &&
        Subsystem->IsOrbitManagedPhysicsBody(Pair.Second));
    TestTrue(TEXT("Recreated body's Chaos gravity remains functional"),
        Pair.Second->GetPhysicsLinearVelocity().Z < -25.0);
    TestTrue(TEXT("Constraint survives re-binding after particle recreation"),
        Pair.Joint->ConstraintInstance.IsValidConstraintInstance());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit063ConstrainedChaosSourceLifecycle,
    "Orbit.Physics.IntegrationWorld.ConstrainedPairSourceLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit063ConstrainedChaosSourceLifecycle::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    AOrbitGravitySource* Source = nullptr;
    Orbit063::FConstrainedPair Pair;
    if (!Orbit063::Setup(*this, Fixture, Source, Pair)) { return false; }
    UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Runtime subsystem"), Subsystem)) { return false; }
    TestTrue(TEXT("Claim first particle"), Subsystem->PrepareSpawnedPhysicsBody(Pair.First));
    TestTrue(TEXT("Claim second particle"), Subsystem->PrepareSpawnedPhysicsBody(Pair.Second));
    if (!Fixture.Step(12)) { return false; }
    TestTrue(TEXT("Orbit accelerates the constraint pair with active source"),
        Pair.First->GetPhysicsLinearVelocity().Z < -15.0 &&
        Pair.Second->GetPhysicsLinearVelocity().Z < -15.0);
    // The isolated world has zero native gravity and no Orbit fallback.
    // Reset velocities to distinguish stale gravity force from inertial motion.
    Source->bEnabled = false;
    Pair.First->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Pair.Second->SetPhysicsLinearVelocity(FVector::ZeroVector);
    if (!Fixture.Step(12)) { return false; }
    TestTrue(TEXT("Disabling source stops additional gravitational acceleration"),
        FMath::Abs(Pair.First->GetPhysicsLinearVelocity().Z) < 35.0 &&
        FMath::Abs(Pair.Second->GetPhysicsLinearVelocity().Z) < 35.0);
    Source->bEnabled = true;
    Pair.First->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Pair.Second->SetPhysicsLinearVelocity(FVector::ZeroVector);
    if (!Fixture.Step(15)) { return false; }
    TestTrue(TEXT("Reenabled source accelerates both constraint particles"),
        Pair.First->GetPhysicsLinearVelocity().Z < -30.0 &&
        Pair.Second->GetPhysicsLinearVelocity().Z < -30.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbit065ProjectilePredictionFieldTest,
    "Orbit.Projectile.IntegrationWorld.DirectionalTrajectory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOrbit065ProjectilePredictionFieldTest::RunTest(const FString& Parameters)
{
    using namespace OrbitWorldIntegrationTests;
    FScopedOrbitTestSettings Settings;
    FFixture Fixture(*this);
    if (!Fixture.Create()) { return false; }
    AOrbitGravitySource* Source = Fixture.Source();
    if (!TestNotNull(TEXT("Directional source spawned"), Source)) { return false; }
    Source->Shape = EOrbitGravityShape::Directional;
    Source->SetActorRotation(FRotator(90.0, 0.0, 0.0));
    if (!Fixture.Begin() || !Fixture.Step(2)) { return false; }
    const UOrbitWorldSubsystem* Subsystem = Fixture.World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!TestNotNull(TEXT("Source world has Orbit subsystem"), Subsystem)) { return false; }
    const FVector Start(0.0, 0.0, 500.0);
    const FOrbitGravitySample Sample = Subsystem->SampleGravityForActor(Source, Start, EOrbitGravityChannel::Physics);
    if (!TestTrue(TEXT("Runtime field has horizontal influence"), Sample.bHasInfluence &&
        FMath::Abs(Sample.Acceleration.X) > 100.0 && FMath::Abs(Sample.Acceleration.Z) < 1.0))
    {
        return false;
    }
    const TArray<FVector> Points = UOrbitBlueprintLibrary::PredictOrbitProjectileTrajectory(
        Fixture.World, Source, Start, FVector::ZeroVector, 0.5f, 0.05f, 1.0f);
    if (!TestEqual(TEXT("Prediction returns launch point plus ten simulated steps"), Points.Num(), 11))
    {
        return false;
    }
    TestTrue(TEXT("Prediction preserves launch point"), Points[0].Equals(Start, 0.01));
    TestTrue(TEXT("Trajectory accelerates along sampled field direction"),
        FVector::DotProduct(Points.Last() - Start, Sample.Acceleration) > 10.0);
    TestTrue(TEXT("Horizontal gravity does not incorrectly pull trajectory world-Z"),
        FMath::Abs(Points.Last().Z - Start.Z) < 1.0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS (Iterations 060-063)

