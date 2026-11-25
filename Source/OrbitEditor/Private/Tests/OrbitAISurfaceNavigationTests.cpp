#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "OrbitAISurfaceNavigation.h"
#include "OrbitAIPathFollowerComponent.h"
#include "Tests/AutomationCommon.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "CollisionShape.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAISurfaceGridRoutes,
    "Orbit.AI.Surface.GridDetoursAndUnreachable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAISurfaceGridRoutes::RunTest(const FString& Parameters)
{
    constexpr int32 Lon = 16;
    constexpr int32 Lat = 9;
    TArray<uint8> Blocked;
    Blocked.Init(0, Lon * Lat);
    const int32 Start = 4 * Lon + 2;
    const int32 Goal = 4 * Lon + 7;
    TArray<int32> Direct, Detour, Again;
    TestTrue(TEXT("Clear graph route"), UOrbitAISurfaceNavigation::FindGridRoute(Lon, Lat, Blocked, Start, Goal, Direct));
    TestTrue(TEXT("Clear route has two endpoints"), Direct.Num() >= 2);
    for (int32 Latitude = 2; Latitude <= 6; ++Latitude)
    {
        Blocked[Latitude * Lon + 5] = 1;
    }
    TestTrue(TEXT("Obstacles cause a detour"), UOrbitAISurfaceNavigation::FindGridRoute(Lon, Lat, Blocked, Start, Goal, Detour));
    TestTrue(TEXT("Detour differs from unobstructed path"), Detour != Direct);
    TestTrue(TEXT("Detour is reproducible"), UOrbitAISurfaceNavigation::FindGridRoute(Lon, Lat, Blocked, Start, Goal, Again));
    TestTrue(TEXT("A* tie breaks remain deterministic"), Again == Detour);
    for (int32 Index : Detour) TestFalse(TEXT("Route never enters a blocked cell"), !!Blocked[Index]);
    Blocked.Init(1, Lon * Lat);
    Blocked[Start] = 0;
    Blocked[Goal] = 0;
    TestFalse(TEXT("Disconnected graph fails"), UOrbitAISurfaceNavigation::FindGridRoute(Lon, Lat, Blocked, Start, Goal, Detour));
    TestTrue(TEXT("Failed route clears output"), Detour.IsEmpty());
    TestFalse(TEXT("Reject invalid mask"), UOrbitAISurfaceNavigation::FindGridRoute(Lon, Lat, TArray<uint8>(), Start, Goal, Detour));
    TestFalse(TEXT("Reject invalid start"), UOrbitAISurfaceNavigation::FindGridRoute(Lon, Lat, Blocked, -1, Goal, Detour));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAISurfaceWorld,
    "Orbit.AI.Surface.IntegrationWorld.SphereAndObstacles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAISurfaceWorld::RunTest(const FString& Parameters)
{
    FTestWorldWrapper TestWorld;
    if (!TestTrue(TEXT("Create isolated surface world"), TestWorld.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = TestWorld.GetTestWorld();
    if (!TestNotNull(TEXT("Surface test world"), World)) return false;
    AActor* SphereActor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Sphere actor"), SphereActor)) return false;
    USphereComponent* Sphere = NewObject<USphereComponent>(SphereActor, TEXT("OrbitNavigationSphere"));
    SphereActor->AddInstanceComponent(Sphere);
    SphereActor->SetRootComponent(Sphere);
    Sphere->InitSphereRadius(1000.0f);
    Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Sphere->SetCollisionResponseToAllChannels(ECR_Block);
    Sphere->RegisterComponent();

    FOrbitSphericalNavigationSettings Settings;
    Settings.LongitudeSamples = 24;
    Settings.LatitudeSamples = 13;
    TArray<FVector> Path;
    FString Reason;
    const FVector Start(1000, 0, 0);
    const FVector Goal(0, 1000, 0);
    TestTrue(TEXT("Spherical surface has a traversable route"),
        UOrbitAISurfaceNavigation::FindSphericalSurfacePath(World, Start, Goal, FVector::ZeroVector, 1000,
            Settings, Path, Reason));
    TestTrue(TEXT("Route includes samples"), Path.Num() >= 2);
    for (const FVector& Point : Path)
    {
        TestTrue(TEXT("Waypoints stay near queried sphere"), FMath::Abs(Point.Size() - 1000.0) < 3.0);
    }

    FOrbitSphericalNavigationSettings CapsuleSettings = Settings;
    CapsuleSettings.AgentRadius = 34.0f;
    CapsuleSettings.AgentHalfHeight = 88.0f;
    TestTrue(TEXT("Character-sized capsule can navigate empty sphere"),
        UOrbitAISurfaceNavigation::FindSphericalSurfacePath(World, Start, Goal, FVector::ZeroVector, 1000,
            CapsuleSettings, Path, Reason));
    TestTrue(TEXT("Empty spherical leg supports capsule clearance"),
        UOrbitAISurfaceNavigation::IsSphericalSurfaceLegTraversable(World, Start, Goal, FVector::ZeroVector, 1000,
            CapsuleSettings));
    FOrbitSphericalNavigationSettings InvalidCapsule = CapsuleSettings;
    InvalidCapsule.AgentRadius = -1.0f;
    TestFalse(TEXT("Negative agent radius rejected"),
        UOrbitAISurfaceNavigation::FindSphericalSurfacePath(World, Start, Goal, FVector::ZeroVector, 1000,
            InvalidCapsule, Path, Reason));
    TestTrue(TEXT("Invalid clearance query resets output"), Path.IsEmpty());

    AActor* BlockerActor = World->SpawnActor<AActor>(FVector(707.1, 707.1, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Obstacle actor"), BlockerActor)) return false;
    USphereComponent* Blocker = NewObject<USphereComponent>(BlockerActor, TEXT("OrbitNavigationObstacle"));
    BlockerActor->AddInstanceComponent(Blocker);
    BlockerActor->SetRootComponent(Blocker);
    Blocker->InitSphereRadius(210.0f);
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Blocker->SetCollisionResponseToAllChannels(ECR_Block);
    Blocker->RegisterComponent();
    BlockerActor->SetActorLocation(FVector(707.1, 707.1, 0.0));

    TestFalse(TEXT("Live obstacle invalidates previously clear direct capsule leg"),
        UOrbitAISurfaceNavigation::IsSphericalSurfaceLegTraversable(World, Start, Goal, FVector::ZeroVector,
            1000, CapsuleSettings));
    TArray<FVector> CapsuleDetour;
    TestTrue(TEXT("Capsule clearance planner finds obstacle detour"),
        UOrbitAISurfaceNavigation::FindSphericalSurfacePath(World, Start, Goal, FVector::ZeroVector,
            1000, CapsuleSettings, CapsuleDetour, Reason));
    TestTrue(TEXT("Capsule path contains detour waypoints"), CapsuleDetour.Num() >= 2);

    TArray<FVector> Detour;
    TestTrue(TEXT("Planner finds path around obstacle"),
        UOrbitAISurfaceNavigation::FindSphericalSurfacePath(World, Start, Goal, FVector::ZeroVector, 1000,
            Settings, Detour, Reason));
    TestTrue(TEXT("Detour contains waypoints"), Detour.Num() >= 2);
    for (const FVector& Point : Detour)
    {
        TestTrue(TEXT("Detour avoids raised obstacle"), (Point - BlockerActor->GetActorLocation()).Size() > 195.0);
    }
    TArray<FVector> Invalid;
    TestFalse(TEXT("Missing sphere surface cannot produce a path"),
        UOrbitAISurfaceNavigation::FindSphericalSurfacePath(World, Start, FVector(0, 0, 2000),
            FVector::ZeroVector, 1000, Settings, Invalid, Reason));
    TestTrue(TEXT("Invalid route gives diagnostic"), !Reason.IsEmpty());
    TestTrue(TEXT("Invalid route returns no waypoints"), Invalid.IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAISurfaceCapsuleNearMiss,
    "Orbit.AI.Surface.IntegrationWorld.CapsuleNearMiss",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAISurfaceCapsuleNearMiss::RunTest(const FString& Parameters)
{
    FTestWorldWrapper TestWorld;
    if (!TestTrue(TEXT("Create capsule near-miss world"), TestWorld.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = TestWorld.GetTestWorld();
    if (!TestNotNull(TEXT("Near-miss world"), World)) return false;
    AActor* Planet = World->SpawnActor<AActor>();
    USphereComponent* Surface = NewObject<USphereComponent>(Planet);
    Planet->AddInstanceComponent(Surface);
    Planet->SetRootComponent(Surface);
    Surface->InitSphereRadius(1000.0f);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent();
    AActor* NearObstacle = World->SpawnActor<AActor>(FVector(705.0, 705.0, 35.0), FRotator::ZeroRotator);
    USphereComponent* Obstacle = NewObject<USphereComponent>(NearObstacle);
    NearObstacle->AddInstanceComponent(Obstacle);
    NearObstacle->SetRootComponent(Obstacle);
    // Keep the blocker clearly outside a radial point trace but inside the
    // footprint of the 34 cm radius / 88 cm half-height radial capsule.
    Obstacle->InitSphereRadius(32.0f);
    Obstacle->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Obstacle->SetCollisionResponseToAllChannels(ECR_Block);
    Obstacle->RegisterComponent();
    NearObstacle->SetActorLocation(FVector(705.0, 705.0, 35.0));
    FOrbitSphericalNavigationSettings PointSettings;
    FOrbitSphericalNavigationSettings CapsuleSettings = PointSettings;
    CapsuleSettings.AgentRadius = 34.0f;
    CapsuleSettings.AgentHalfHeight = 88.0f;
    const FVector Start(1000.0, 0.0, 0.0);
    const FVector Goal(0.0, 1000.0, 0.0);
    // Sanity-check that the obstacle genuinely overlaps the capsule footprint,
    // independently of the planner. Keep the scene fixture meaningful even if
    // navigation sampling changes in a future iteration.
    const FVector MidSurface(707.106781, 707.106781, 0.0);
    const FVector MidUp = MidSurface.GetSafeNormal();
    FCollisionQueryParams CheckParams(SCENE_QUERY_STAT(OrbitNearMissFixture), false);
    CheckParams.AddIgnoredActor(Planet);
    const bool bFixtureOverlapsCapsule = World->OverlapBlockingTestByChannel(
        MidSurface + MidUp * (CapsuleSettings.AgentHalfHeight + CapsuleSettings.SurfaceSkin),
        FQuat::FindBetweenNormals(FVector::UpVector, MidUp), CapsuleSettings.TraceChannel,
        FCollisionShape::MakeCapsule(CapsuleSettings.AgentRadius, CapsuleSettings.AgentHalfHeight), CheckParams);
    TestTrue(TEXT("Near-miss obstacle actually overlaps the character capsule"), bFixtureOverlapsCapsule);
    TestTrue(TEXT("Point-sized route clears offset obstacle"),
        UOrbitAISurfaceNavigation::IsSphericalSurfaceLegTraversable(World, Start, Goal,
            FVector::ZeroVector, 1000.0f, PointSettings));
    TestFalse(TEXT("Character capsule detects obstacle beside point route"),
        UOrbitAISurfaceNavigation::IsSphericalSurfaceLegTraversable(World, Start, Goal,
            FVector::ZeroVector, 1000.0f, CapsuleSettings));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitAISurfaceFollowerReplan,
    "Orbit.AI.Surface.IntegrationWorld.CharacterCapsuleReplan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitAISurfaceFollowerReplan::RunTest(const FString& Parameters)
{
    FTestWorldWrapper TestWorld;
    if (!TestTrue(TEXT("Create replan world"), TestWorld.CreateTestWorld(EWorldType::Game))) return false;
    UWorld* World = TestWorld.GetTestWorld();
    if (!TestNotNull(TEXT("Replan world"), World)) return false;
    AActor* Planet = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Planet Actor"), Planet)) return false;
    USphereComponent* Surface = NewObject<USphereComponent>(Planet, TEXT("OrbitReplanSurface"));
    Planet->AddInstanceComponent(Surface);
    Planet->SetRootComponent(Surface);
    Surface->InitSphereRadius(1000.0f);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->RegisterComponent();

    ACharacter* Character = World->SpawnActor<ACharacter>(ACharacter::StaticClass(),
        FVector(1000.0, 0.0, 0.0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Navigation Character"), Character)) return false;
    // Navigation expects the Character capsule center above the surface, not
    // centered at the surface. Its feet are at radius 1000.
    Character->SetActorRotation(FQuat::FindBetweenNormals(FVector::UpVector, FVector::ForwardVector));
    const float CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Character->SetActorLocation(FVector(1000.0 + CapsuleHalfHeight + 3.0, 0.0, 0.0), false, nullptr,
        ETeleportType::TeleportPhysics);
    UOrbitAIPathFollowerComponent* Follower = NewObject<UOrbitAIPathFollowerComponent>(Character);
    Character->AddInstanceComponent(Follower);
    Follower->RegisterComponent();
    FOrbitSphericalNavigationSettings Settings;
    Settings.LongitudeSamples = 24;
    Settings.LatitudeSamples = 13;
    const FVector Goal(0.0, 1000.0, 0.0);
    const bool bAccepted = Follower->FollowSphericalSurfacePath(Goal, FVector::ZeroVector, 1000.0f, Settings);
    if (!TestTrue(FString::Printf(TEXT("Character accepts capsule-sized spherical route (%s)"),
        *Follower->LastPathStatus), bAccepted)) return false;
    TestTrue(TEXT("Follower is following"), Follower->bFollowingPath);
    TestEqual(TEXT("No replan initially"), Follower->SurfaceReplanCount, 0);

    AActor* BlockerActor = World->SpawnActor<AActor>(FVector(707.1, 707.1, 0.0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Dynamic obstacle"), BlockerActor)) return false;
    USphereComponent* Blocker = NewObject<USphereComponent>(BlockerActor, TEXT("OrbitReplanObstacle"));
    BlockerActor->AddInstanceComponent(Blocker);
    BlockerActor->SetRootComponent(Blocker);
    Blocker->InitSphereRadius(210.0f);
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Blocker->SetCollisionResponseToAllChannels(ECR_Block);
    Blocker->RegisterComponent();
    BlockerActor->SetActorLocation(FVector(707.1, 707.1, 0.0));
    TestTrue(TEXT("Character can replan after obstacle appears"), Follower->ReplanSphericalSurfacePathNow());
    TestEqual(TEXT("One recorded successful replan"), Follower->SurfaceReplanCount, 1);
    TestTrue(TEXT("Follower retains replacement route"), Follower->bFollowingPath && Follower->Waypoints.Num() >= 2);

    Follower->StopFollowing();
    TestFalse(TEXT("Stopping disables movement"), Follower->bFollowingPath);
    TestFalse(TEXT("Stopped follower rejects replan"), Follower->ReplanSphericalSurfacePathNow());
    return true;
}

#endif
