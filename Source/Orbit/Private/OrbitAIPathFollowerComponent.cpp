#include "OrbitAIPathFollowerComponent.h"
#include "OrbitAISurfaceNavigation.h"
#include "OrbitCharacterGravityComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

UOrbitAIPathFollowerComponent::UOrbitAIPathFollowerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bAutoActivate = true;
}

FOrbitAIMoveDiagnostics UOrbitAIPathFollowerComponent::GetMoveDiagnostics() const
{
    FOrbitAIMoveDiagnostics Snapshot;
    Snapshot.RequestId = MoveRequestId;
    Snapshot.Result = LastMoveResult;
    Snapshot.bActive = bFollowingPath;
    Snapshot.bSphericalRoute = bHasActiveSurfaceRoute;
    Snapshot.WaypointIndex = CurrentWaypointIndex;
    Snapshot.WaypointCount = Waypoints.Num();
    Snapshot.SuccessfulReplans = SurfaceReplanCount;
    Snapshot.StallReplans = StallReplanAttempts;
    Snapshot.MovementSpeed = LastMovementSpeed;
    Snapshot.SecondsStalled = SecondsWithoutMovement;
    Snapshot.TangentWaypointDistance = TangentDistanceToWaypoint;
    Snapshot.Status = LastPathStatus;
    if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
    {
        if (const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
        {
            Snapshot.bGrounded = Movement->IsMovingOnGround();
        }
        if (const UOrbitCharacterGravityComponent* Gravity = OwnerCharacter->FindComponentByClass<UOrbitCharacterGravityComponent>())
        {
            Snapshot.bInsideOrbitGravityField = Gravity->bInsideOrbitField;
            Snapshot.GravitySourceId = Gravity->LastDominantSourceId;
        }
    }
    return Snapshot;
}

void UOrbitAIPathFollowerComponent::BeginPlay()
{
    Super::BeginPlay();
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        // Orbit gravity must establish the current gravity frame before steering;
        // CharacterMovement must consume the steering input afterwards.
        if (UOrbitCharacterGravityComponent* Gravity = Character->FindComponentByClass<UOrbitCharacterGravityComponent>())
        {
            AddTickPrerequisiteComponent(Gravity);
        }
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->AddTickPrerequisiteComponent(this);
        }
    }
}

FVector UOrbitAIPathFollowerComponent::CalculateTangentSteering(const FVector& CurrentLocation, const FVector& TargetLocation, const FVector& GravityDown)
{
    const FVector Delta = TargetLocation - CurrentLocation;
    if (Delta.IsNearlyZero())
    {
        return FVector::ZeroVector;
    }
    const FVector Down = GravityDown.GetSafeNormal();
    return Down.IsNearlyZero() ? Delta.GetSafeNormal() : FVector::VectorPlaneProject(Delta, Down).GetSafeNormal();
}

bool UOrbitAIPathFollowerComponent::IsWaypointReached(const FVector& CurrentLocation, const FVector& TargetLocation,
    const FVector& GravityDown, float Radius, float MaxNormalError, bool bTangentArrival)
{
    if (!!CurrentLocation.ContainsNaN() || !!TargetLocation.ContainsNaN())
    {
        return false;
    }
    const float SafeRadius = FMath::Max(1.0f, Radius);
    const FVector Delta = TargetLocation - CurrentLocation;
    if (Delta.SizeSquared() <= FMath::Square(SafeRadius))
    {
        return true;
    }
    const FVector Down = GravityDown.GetSafeNormal();
    if (!bTangentArrival || Down.IsNearlyZero())
    {
        return false;
    }
    const float NormalDistance = FMath::Abs(FVector::DotProduct(Delta, Down));
    return NormalDistance <= FMath::Max(0.0f, MaxNormalError)
        && FVector::VectorPlaneProject(Delta, Down).SizeSquared() <= FMath::Square(SafeRadius);
}

bool UOrbitAIPathFollowerComponent::FollowWaypoints(const TArray<FVector>& NewWaypoints)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !Character->GetCharacterMovement() || NewWaypoints.IsEmpty())
    {
        LastPathStatus = TEXT("Invalid owner or empty path");
        return false;
    }
    for (const FVector& Point : NewWaypoints)
    {
        if (!!Point.ContainsNaN())
        {
            LastPathStatus = TEXT("Non-finite waypoint");
            return false;
        }
    }
    MovingTarget.Reset();
    bSurfaceActorChase = false;
    bHasActiveSurfaceRoute = false;
    ++MoveRequestId;
    if (MoveRequestId <= 0) MoveRequestId = 1;
    LastMoveResult = EOrbitAIMoveResult::InProgress;
    Waypoints = NewWaypoints;
    CurrentWaypointIndex = 0;
    bFollowingPath = true;
    bHasMovementSample = false;
    LastMovementSpeed = 0.0f;
    SecondsWithoutMovement = 0.0f;
    TangentDistanceToWaypoint = 0.0f;
    StallReplanAttempts = 0;
    LastPathStatus = TEXT("Following");
    return true;
}

bool UOrbitAIPathFollowerComponent::FollowLocation(FVector Goal)
{
    TArray<FVector> Goals;
    Goals.Add(Goal);
    return FollowWaypoints(Goals);
}

bool UOrbitAIPathFollowerComponent::BuildSphericalArcWaypoints(const FVector& Start, const FVector& Goal,
    const FVector& Center, float MaxSegmentLength, TArray<FVector>& OutPoints)
{
    OutPoints.Reset();
    if (!!Start.ContainsNaN() || !!Goal.ContainsNaN() || !!Center.ContainsNaN() ||
        !FMath::IsFinite(MaxSegmentLength) || MaxSegmentLength <= 0.0f)
    {
        return false;
    }
    const FVector A = Start - Center;
    const FVector B = Goal - Center;
    const double R1 = A.Size();
    const double R2 = B.Size();
    if (R1 < 1.0 || R2 < 1.0 || FMath::Abs(R1 - R2) > FMath::Max(25.0, R1 * 0.1))
    {
        return false;
    }
    const FVector UnitA = A / R1;
    const FVector UnitB = B / R2;
    const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(UnitA, UnitB), -1.0, 1.0));
    if (Angle < KINDA_SMALL_NUMBER)
    {
        OutPoints.Add(Goal);
        return true;
    }
    const int32 Segments = FMath::Clamp(FMath::CeilToInt(Angle * R1 / MaxSegmentLength), 1, 256);
    // Deterministic great-circle axis even for antipodal points.
    FVector Tangent = FVector::VectorPlaneProject(UnitB, UnitA).GetSafeNormal();
    if (Tangent.IsNearlyZero())
    {
        Tangent = FVector::VectorPlaneProject(FVector::UpVector, UnitA).GetSafeNormal();
        if (Tangent.IsNearlyZero())
        {
            Tangent = FVector::VectorPlaneProject(FVector::ForwardVector, UnitA).GetSafeNormal();
        }
    }
    for (int32 Index = 1; Index <= Segments; ++Index)
    {
        const double T = static_cast<double>(Index) / Segments;
        const double Theta = Angle * T;
        const FVector Unit = UnitA * FMath::Cos(Theta) + Tangent * FMath::Sin(Theta);
        const double Radius = FMath::Lerp(R1, R2, T);
        OutPoints.Add(Index == Segments ? Goal : Center + Unit * Radius);
    }
    return true;
}

bool UOrbitAIPathFollowerComponent::FollowSphericalArc(FVector Goal, FVector SphereCenter, float MaxSegmentLength)
{
    if (!GetOwner())
    {
        LastPathStatus = TEXT("Missing owner");
        return false;
    }
    TArray<FVector> Points;
    if (!BuildSphericalArcWaypoints(GetOwner()->GetActorLocation(), Goal, SphereCenter, MaxSegmentLength, Points))
    {
        LastPathStatus = TEXT("Invalid spherical route");
        return false;
    }
    const bool bAccepted = FollowWaypoints(Points);
    if (bAccepted)
    {
        LastPathStatus = TEXT("Following spherical arc (no obstacle avoidance)");
    }
    return bAccepted;
}

bool UOrbitAIPathFollowerComponent::FollowActor(AActor* Goal)
{
    if (!IsValid(Goal) || Goal == GetOwner() || !FollowLocation(Goal->GetActorLocation()))
    {
        LastPathStatus = TEXT("Invalid target actor");
        return false;
    }
    MovingTarget = Goal;
    return true;
}

bool UOrbitAIPathFollowerComponent::FollowNativeNavigationPath(FVector Goal)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UWorld* World = GetWorld();
    if (!Character || !World || !!Goal.ContainsNaN())
    {
        LastPathStatus = TEXT("Invalid owner, world, or goal");
        return false;
    }
    UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Character->GetActorLocation(), Goal, Character);
    if (!IsValid(Path) || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.IsEmpty())
    {
        LastPathStatus = TEXT("No complete native navigation path");
        return false;
    }
    // Skip native path's starting location; native path points can be at a slightly different
    // height on the navmesh. The full 3D acceptance rule still applies to every waypoint.
    TArray<FVector> Points = Path->PathPoints;
    if (Points.Num() > 1)
    {
        Points.RemoveAt(0);
    }
    const bool bAccepted = FollowWaypoints(Points);
    if (bAccepted)
    {
        LastPathStatus = TEXT("Following native NavMesh path (not curved-world pathfinding)");
    }
    return bAccepted;
}

bool UOrbitAIPathFollowerComponent::FollowSphericalSurfacePath(FVector Goal, FVector SphereCenter,
    float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    FOrbitSphericalNavigationSettings Effective = Settings;
    if (bUseCharacterCapsuleForSurfaceNavigation && Character && Character->GetCapsuleComponent())
    {
        float Radius = 0.0f, HalfHeight = 0.0f;
        Character->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
        Effective.AgentRadius = Radius;
        Effective.AgentHalfHeight = HalfHeight;
    }
    TArray<FVector> Planned;
    FString Reason;
    if (!Character || !UOrbitAISurfaceNavigation::FindSphericalSurfacePath(this,
        Character->GetActorLocation(), Goal, SphereCenter, ReferenceRadius, Effective, Planned, Reason))
    {
        StopFollowing();
        LastMoveResult = EOrbitAIMoveResult::Failed;
        LastPathStatus = Reason.IsEmpty() ? TEXT("Missing Character") : Reason;
        return false;
    }
    if (!FollowWaypoints(Planned)) return false;
    bHasActiveSurfaceRoute = true;
    SurfaceGoal = Goal;
    SurfaceCenter = SphereCenter;
    SurfaceRadius = ReferenceRadius;
    ActiveSurfaceSettings = Effective;
    SurfaceReplanCount = 0;
    TimeUntilRouteCheck = FMath::Max(0.2f, RouteCheckInterval);
    LastPathStatus = TEXT("Following capsule-verified spherical route");
    return true;
}

bool UOrbitAIPathFollowerComponent::FollowSphericalSurfaceActor(AActor* Goal, FVector SphereCenter,
    float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings)
{
    if (!IsValid(Goal) || Goal == GetOwner())
    {
        LastPathStatus = TEXT("Invalid spherical chase target");
        return false;
    }
    if (!FollowSphericalSurfacePath(Goal->GetActorLocation(), SphereCenter, ReferenceRadius, Settings))
    {
        return false;
    }
    MovingTarget = Goal;
    bSurfaceActorChase = true;
    return true;
}

bool UOrbitAIPathFollowerComponent::AbortMoveRequest(int32 ExpectedRequestId)
{
    if (!bFollowingPath || ExpectedRequestId <= 0 || ExpectedRequestId != MoveRequestId)
    {
        return false;
    }
    StopFollowing();
    LastMoveResult = EOrbitAIMoveResult::Aborted;
    LastPathStatus = TEXT("Aborted");
    return true;
}

bool UOrbitAIPathFollowerComponent::ReplanSphericalSurfacePathNow()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!bHasActiveSurfaceRoute || !Character)
    {
        LastPathStatus = TEXT("No active spherical route to replan");
        return false;
    }
    TArray<FVector> NewPath;
    FString Failure;
    if (!UOrbitAISurfaceNavigation::FindSphericalSurfacePath(this, Character->GetActorLocation(),
        SurfaceGoal, SurfaceCenter, SurfaceRadius, ActiveSurfaceSettings, NewPath, Failure))
    {
        StopFollowing();
        LastMoveResult = EOrbitAIMoveResult::Failed;
        LastPathStatus = FString::Printf(TEXT("Surface replan failed: %s"), *Failure);
        return false;
    }
    Waypoints = MoveTemp(NewPath);
    CurrentWaypointIndex = 0;
    bFollowingPath = true;
    ++SurfaceReplanCount;
    bHasMovementSample = false;
    SecondsWithoutMovement = 0.0f;
    TimeUntilRouteCheck = FMath::Max(0.2f, RouteCheckInterval);
    LastPathStatus = TEXT("Spherical route replanned");
    return true;
}

void UOrbitAIPathFollowerComponent::StopFollowing()
{
    if (bFollowingPath) LastMoveResult = EOrbitAIMoveResult::Aborted;
    bFollowingPath = false;
    CurrentWaypointIndex = INDEX_NONE;
    MovingTarget.Reset();
    bSurfaceActorChase = false;
    bHasActiveSurfaceRoute = false;
    TimeUntilRouteCheck = 0.0f;
    Waypoints.Reset();
    bHasMovementSample = false;
    SecondsWithoutMovement = 0.0f;
    LastMovementSpeed = 0.0f;
    TangentDistanceToWaypoint = 0.0f;
    StallReplanAttempts = 0;
    bLastMovementGrounded = false;
    LastPathStatus = TEXT("Stopped");
}

FVector UOrbitAIPathFollowerComponent::GetCurrentWaypoint() const
{
    return bFollowingPath && Waypoints.IsValidIndex(CurrentWaypointIndex)
        ? Waypoints[CurrentWaypointIndex] : FVector::ZeroVector;
}

void UOrbitAIPathFollowerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bFollowingPath)
    {
        return;
    }
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Character || !Movement || !Waypoints.IsValidIndex(CurrentWaypointIndex))
    {
        StopFollowing();
        LastMoveResult = EOrbitAIMoveResult::Failed;
        LastPathStatus = TEXT("Invalid movement or waypoint state");
        return;
    }
    if (bSurfaceActorChase)
    {
        if (!MovingTarget.IsValid())
        {
            StopFollowing();
            LastMoveResult = EOrbitAIMoveResult::Failed;
            LastPathStatus = TEXT("Spherical chase target lost");
            return;
        }
        const FVector LatestGoal = MovingTarget->GetActorLocation();
        if (FVector::DistSquared(LatestGoal, SurfaceGoal) >=
            FMath::Square(FMath::Max(1.0f, MovingTargetReplanDistance)))
        {
            SurfaceGoal = LatestGoal;
            if (!ReplanSphericalSurfacePathNow()) return;
        }
    }
    else if (MovingTarget.IsValid())
    {
        if (bTrackMovingTarget) Waypoints.Last() = MovingTarget->GetActorLocation();
    }
    else if (MovingTarget.IsStale())
    {
        StopFollowing();
        LastMoveResult = EOrbitAIMoveResult::Failed;
        LastPathStatus = TEXT("Target actor lost");
        return;
    }
    bLastMovementGrounded = Movement->IsMovingOnGround();
    if (bRequireGrounded && !bLastMovementGrounded)
    {
        // A Character falling toward a planet is not a navigation stall.
        bHasMovementSample = false;
        SecondsWithoutMovement = 0.0f;
        LastMovementSpeed = 0.0f;
        LastPathStatus = TEXT("Paused: not grounded");
        return;
    }
    const FVector Down = Movement->GetGravityDirection();
    const FVector Current = Character->GetActorLocation();
    if (DeltaTime > SMALL_NUMBER && FMath::IsFinite(DeltaTime))
    {
        if (bHasMovementSample)
        {
            LastMovementSpeed = static_cast<float>(FVector::Dist(Current, LastMovementSampleLocation) / DeltaTime);
            // Do not count the first sample, teleportation or a missing gravity frame as movement progress.
            if (bDetectMovementStalls && LastMovementSpeed < FMath::Max(0.0f, MinimumMovementSpeed))
            {
                SecondsWithoutMovement += DeltaTime;
            }
            else
            {
                SecondsWithoutMovement = 0.0f;
            }
        }
        LastMovementSampleLocation = Current;
        bHasMovementSample = true;
    }
    // Advance past all contiguous reached waypoints to avoid one-frame stalls.
    while (Waypoints.IsValidIndex(CurrentWaypointIndex) && IsWaypointReached(Current,
        Waypoints[CurrentWaypointIndex], Down, AcceptanceRadius, MaxNormalArrivalError, bUseTangentArrival))
    {
        ++CurrentWaypointIndex;
    }
    if (!Waypoints.IsValidIndex(CurrentWaypointIndex))
    {
        bFollowingPath = false;
        bHasActiveSurfaceRoute = false;
        MovingTarget.Reset();
        CurrentWaypointIndex = INDEX_NONE;
        bSurfaceActorChase = false;
        LastMoveResult = EOrbitAIMoveResult::Succeeded;
        LastPathStatus = TEXT("Reached destination");
        return;
    }
    TangentDistanceToWaypoint = static_cast<float>(FVector::VectorPlaneProject(
        Waypoints[CurrentWaypointIndex] - Current, Down.GetSafeNormal()).Size());
    if (bDetectMovementStalls && bHasMovementSample &&
        SecondsWithoutMovement >= FMath::Max(0.2f, MovementStallTimeout))
    {
        if (bHasActiveSurfaceRoute && bReplanStalledSurfaceRoutes &&
            StallReplanAttempts < FMath::Clamp(MaxStallReplans, 0, 10))
        {
            ++StallReplanAttempts;
            if (!ReplanSphericalSurfacePathNow()) return;
            LastPathStatus = TEXT("Replanned after movement stall");
            return;
        }
        StopFollowing();
        LastMoveResult = EOrbitAIMoveResult::Failed;
        LastPathStatus = TEXT("Stalled: Character did not move");
        return;
    }
    if (bHasActiveSurfaceRoute && bReplanBlockedSurfaceRoutes)
    {
        TimeUntilRouteCheck -= DeltaTime;
        if (TimeUntilRouteCheck <= 0.0f)
        {
            TimeUntilRouteCheck = FMath::Max(0.2f, RouteCheckInterval);
            // Inspect the immediate movement leg, not the entire route every tick.
            if (!UOrbitAISurfaceNavigation::IsSphericalSurfaceLegTraversable(this, Current,
                Waypoints[CurrentWaypointIndex], SurfaceCenter, SurfaceRadius, ActiveSurfaceSettings))
            {
                if (!ReplanSphericalSurfacePathNow()) return;
            }
        }
    }
    if (bDrawSurfacePathDebug && bHasActiveSurfaceRoute && GetWorld())
    {
        FVector Previous = Current;
        const int32 Last = FMath::Min(Waypoints.Num(), CurrentWaypointIndex + 64);
        for (int32 I = CurrentWaypointIndex; I < Last; ++I)
        {
            DrawDebugLine(GetWorld(), Previous, Waypoints[I], FColor::Cyan, false, 0.0f, 0, 2.0f);
            DrawDebugSphere(GetWorld(), Waypoints[I], 12.0f, 6, FColor::Green, false, 0.0f);
            Previous = Waypoints[I];
        }
    }
    const FVector Direction = CalculateTangentSteering(Current, Waypoints[CurrentWaypointIndex], Down);
    if (Direction.IsNearlyZero())
    {
        LastPathStatus = TEXT("Blocked: waypoint is aligned with gravity axis");
        return;
    }
    Character->AddMovementInput(Direction, 1.0f);
    LastPathStatus = TEXT("Following");
}

void UOrbitAIPathFollowerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->RemoveTickPrerequisiteComponent(this);
        }
    }
    StopFollowing();
    Super::EndPlay(EndPlayReason);
}
