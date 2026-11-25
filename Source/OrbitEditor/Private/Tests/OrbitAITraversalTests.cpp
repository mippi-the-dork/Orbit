#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitAIPathFollowerComponent.h"
#include "OrbitAISurfaceNavigation.h"
#include "Tests/AutomationCommon.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

namespace OrbitAITraversalTest
{
    struct FFixture
    {
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        ACharacter* Character = nullptr;
        UOrbitAIPathFollowerComponent* Follower = nullptr;
        UCharacterMovementComponent* Movement = nullptr;
        const FVector Center = FVector::ZeroVector;
        static constexpr double SurfaceRadius = 1000.0;

        bool Initialize()
        {
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) return false;
            World = TestWorld.GetTestWorld();
            if (!World) return false;
            AActor* Planet = World->SpawnActor<AActor>();
            if (!Planet) return false;
            USphereComponent* Surface = NewObject<USphereComponent>(Planet, TEXT("OrbitTraversalPlanet"));
            Planet->AddInstanceComponent(Surface);
            Planet->SetRootComponent(Surface);
            Surface->InitSphereRadius(SurfaceRadius);
            Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Surface->SetCollisionResponseToAllChannels(ECR_Block);
            Surface->RegisterComponent();
            Character = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(1200, 0, 0), FRotator::ZeroRotator);
            if (!Character) return false;
            Movement = Character->GetCharacterMovement();
            if (!Movement) return false;
            Movement->bRunPhysicsWithNoController = true;
            Movement->MaxWalkSpeed = 240.0f;
            Movement->MaxAcceleration = 1500.0f;
            Movement->BrakingDecelerationWalking = 1400.0f;
            Movement->SetGravityDirection(FVector(-1, 0, 0));
            Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, FVector::ForwardVector));
            Character->SetActorLocation(FVector(SurfaceRadius + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0, 0, 0),
                false, nullptr, ETeleportType::TeleportPhysics);
            Movement->SetMovementMode(MOVE_Walking);
            Follower = NewObject<UOrbitAIPathFollowerComponent>(Character, TEXT("OrbitTraversalFollower"));
            Character->AddInstanceComponent(Follower);
            Follower->RegisterComponent();
            Follower->AcceptanceRadius = 60.0f;
            Follower->bRequireGrounded = true;
            Follower->MovementStallTimeout = 3.0f;
            return true;
        }

        void Step(float Dt)
        {
            // The real native CharacterMovement component owns collision and displacement.
            // Radial gravity/orientation is supplied deterministically by the fixture so
            // this test isolates path-following from gravity-source registration.
            const FVector Up = (Character->GetActorLocation() - Center).GetSafeNormal();
            Movement->SetGravityDirection(-Up);
            Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, Up));
            Follower->TickComponent(Dt, LEVELTICK_All, nullptr);
            Movement->TickComponent(Dt, LEVELTICK_All, nullptr);
        }

        FVector SurfacePoint(double Angle) const
        {
            return FVector(SurfaceRadius * FMath::Cos(Angle), SurfaceRadius * FMath::Sin(Angle), 0.0);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIPhysicsSteppedSurfaceArc,
    "Orbit.AI.Traversal.IntegrationWorld.PhysicsSteppedSurfaceArc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAIPhysicsSteppedSurfaceArc::RunTest(const FString& Parameters)
{
    OrbitAITraversalTest::FFixture F;
    if (!TestTrue(TEXT("Construct native Character traversal fixture"), F.Initialize())) return false;
    const FVector Goal = F.SurfacePoint(PI / 5.0);
    if (!TestTrue(TEXT("Follower accepts curved surface waypoints"),
        F.Follower->FollowSphericalArc(Goal, FVector::ZeroVector, 110.0f))) return false;

    const FVector Start = F.Character->GetActorLocation();
    int32 GroundFrames = 0;
    int32 SampledFrames = 0;
    double MaximumRadialDrift = 0.0;
    constexpr float Dt = 1.0f / 60.0f;
    for (int32 Frame = 0; Frame < 600 && F.Follower->bFollowingPath; ++Frame)
    {
        F.Step(Dt);
        const FVector Location = F.Character->GetActorLocation();
        const double ExpectedRadius = F.SurfaceRadius + F.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        MaximumRadialDrift = FMath::Max(MaximumRadialDrift, FMath::Abs(Location.Size() - ExpectedRadius));
        if (Frame > 15)
        {
            ++SampledFrames;
            if (F.Movement->IsMovingOnGround()) ++GroundFrames;
        }
    }
    const FVector End = F.Character->GetActorLocation();
    TestTrue(TEXT("Native CharacterMovement actually displaced capsule"), FVector::Dist(Start, End) > 150.0);
    TestTrue(FString::Printf(TEXT("Native gravity-relative walking maintains ground contact (%d/%d frames)"),
        GroundFrames, SampledFrames), SampledFrames > 0 && GroundFrames >= SampledFrames * 9 / 10);
    TestTrue(FString::Printf(TEXT("Walking remains near spherical surface (max drift %.1f cm)"), MaximumRadialDrift),
        MaximumRadialDrift < 65.0);
    TestFalse(FString::Printf(TEXT("Follower completes by physical movement (%s)"), *F.Follower->LastPathStatus),
        F.Follower->bFollowingPath);
    TestEqual(TEXT("Follower reports destination reached"), F.Follower->LastPathStatus, FString(TEXT("Reached destination")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAITraversalWatchdog,
    "Orbit.AI.Traversal.IntegrationWorld.StationaryRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAITraversalWatchdog::RunTest(const FString& Parameters)
{
    OrbitAITraversalTest::FFixture F;
    if (!TestTrue(TEXT("Construct stationary Character fixture"), F.Initialize())) return false;
    F.Follower->bRequireGrounded = false;
    F.Follower->MovementStallTimeout = 0.25f;
    F.Follower->bReplanStalledSurfaceRoutes = false;
    F.Follower->MinimumMovementSpeed = 5.0f;
    const FVector Goal = F.SurfacePoint(0.6);
    if (!TestTrue(TEXT("Accept stationary test waypoint"), F.Follower->FollowLocation(Goal))) return false;
    // Intentionally do not step CharacterMovement. A follower issuing movement input
    // cannot count that input as progress when its capsule stays stationary.
    for (int32 Index = 0; Index < 35 && F.Follower->bFollowingPath; ++Index)
    {
        F.Follower->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    }
    TestFalse(TEXT("Stationary follower terminates instead of issuing input forever"), F.Follower->bFollowingPath);
    TestEqual(TEXT("Stationary failure is actionable"), F.Follower->LastPathStatus,
        FString(TEXT("Stalled: Character did not move")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAIPhysicsSteppedReplan,
    "Orbit.AI.Traversal.IntegrationWorld.MovingCharacterReplan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAIPhysicsSteppedReplan::RunTest(const FString& Parameters)
{
    OrbitAITraversalTest::FFixture F;
    if (!TestTrue(TEXT("Construct moving replan fixture"), F.Initialize())) return false;
    FOrbitSphericalNavigationSettings Settings;
    Settings.LongitudeSamples = 24;
    Settings.LatitudeSamples = 13;
    F.Follower->bReplanBlockedSurfaceRoutes = false; // Manually control exactly when terrain changes.
    if (!TestTrue(TEXT("Character accepts capsule-aware movement route"),
        F.Follower->FollowSphericalSurfacePath(F.SurfacePoint(PI / 2.0), FVector::ZeroVector,
            OrbitAITraversalTest::FFixture::SurfaceRadius, Settings))) return false;
    const FVector InitialPosition = F.Character->GetActorLocation();
    constexpr float Dt = 1.0f / 60.0f;
    for (int32 Step = 0; Step < 100 && F.Follower->bFollowingPath; ++Step) F.Step(Dt);
    const FVector BeforeObstacle = F.Character->GetActorLocation();
    if (!TestTrue(TEXT("Native Character moved before obstacle was introduced"),
        FVector::Dist(InitialPosition, BeforeObstacle) > 40.0)) return false;

    AActor* ObstacleActor = F.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Dynamic obstacle Actor"), ObstacleActor)) return false;
    USphereComponent* Obstacle = NewObject<USphereComponent>(ObstacleActor);
    ObstacleActor->AddInstanceComponent(Obstacle);
    ObstacleActor->SetRootComponent(Obstacle);
    Obstacle->InitSphereRadius(135.0f);
    Obstacle->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Obstacle->SetCollisionResponseToAllChannels(ECR_Block);
    Obstacle->RegisterComponent();
    ObstacleActor->SetActorLocation(F.SurfacePoint(PI / 4.0));
    if (!TestTrue(FString::Printf(TEXT("Moving Character replans around new obstacle (%s)"),
        *F.Follower->LastPathStatus), F.Follower->ReplanSphericalSurfacePathNow())) return false;
    TestEqual(TEXT("One successful runtime replan"), F.Follower->SurfaceReplanCount, 1);
    for (int32 Step = 0; Step < 180 && F.Follower->bFollowingPath; ++Step) F.Step(Dt);
    TestTrue(FString::Printf(TEXT("Character resumes physical movement after replan (%s)"),
        *F.Follower->LastPathStatus), FVector::Dist(BeforeObstacle, F.Character->GetActorLocation()) > 40.0);
    return true;
}

#endif
