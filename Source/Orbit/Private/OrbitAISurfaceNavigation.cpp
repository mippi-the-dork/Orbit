#include "OrbitAISurfaceNavigation.h"
#include "Algo/Reverse.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "HAL/PlatformTime.h"
#include "Engine/OverlapResult.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

namespace OrbitSurfaceNav
{
    static int32 Index(int32 Lat, int32 Lon, int32 Longitudes)
    {
        return Lat * Longitudes + (Lon + Longitudes) % Longitudes;
    }

    static FVector Direction(int32 Lat, int32 Lon, int32 Latitudes, int32 Longitudes)
    {
        const double Latitude = -HALF_PI + PI * static_cast<double>(Lat) / (Latitudes - 1);
        const double Longitude = 2.0 * PI * static_cast<double>(Lon) / Longitudes;
        return FVector(FMath::Cos(Latitude) * FMath::Cos(Longitude),
            FMath::Cos(Latitude) * FMath::Sin(Longitude), FMath::Sin(Latitude));
    }

    static bool ValidVector(const FVector& V)
    {
        return !V.ContainsNaN();
    }

    static bool SampleSurface(UWorld* World, const FVector& Center, double Radius,
        const FVector& UnitDirection, const FOrbitSphericalNavigationSettings& Settings, FVector& OutPoint, const AActor* IgnoredActor)
    {
        const FVector Up = UnitDirection.GetSafeNormal();
        if (Up.IsNearlyZero()) return false;
        FHitResult Hit;
        const FVector TraceStart = Center + Up * (Radius + Settings.TraceOutward);
        const FVector TraceEnd = Center + Up * (Radius - Settings.TraceInward);
        FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(OrbitSphericalNavigation), false);
        if (IgnoredActor) TraceParams.AddIgnoredActor(IgnoredActor);
        const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, Settings.TraceChannel, TraceParams);
        if (!bHit || !Hit.bBlockingHit || !ValidVector(Hit.ImpactPoint) || !ValidVector(Hit.ImpactNormal)) return false;
        if (FMath::Abs((Hit.ImpactPoint - Center).Size() - Radius) > Settings.MaxSurfaceHeightError) return false;
        if (FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), Up) < Settings.MinimumWalkableNormalDot) return false;
        if (Settings.AgentRadius > 0.0f)
        {
            const float HalfHeight = FMath::Max(Settings.AgentRadius, Settings.AgentHalfHeight);
            const FVector CenterOfCapsule = Hit.ImpactPoint + Up * (HalfHeight + Settings.SurfaceSkin);
            const FQuat CapsuleRotation = FQuat::FindBetweenNormals(FVector::UpVector, Up);
            const FCollisionShape Shape = FCollisionShape::MakeCapsule(Settings.AgentRadius, HalfHeight);
            FCollisionQueryParams ClearanceParams(SCENE_QUERY_STAT(OrbitSphericalClearance), false);
            if (IgnoredActor) ClearanceParams.AddIgnoredActor(IgnoredActor);
            // A navigation capsule is intentionally adjacent to its supporting surface.
            // OverlapBlockingTestByChannel cannot distinguish that surface from a wall,
            // and a curved/swept contact may register as a blocking overlap even with skin.
            // Inspect individual overlaps and ignore ONLY the component that supplied the
            // walkable hit. Other obstacles, including other components on the same Actor,
            // must still block the capsule.
            TArray<FOverlapResult> Overlaps;
            World->OverlapMultiByChannel(Overlaps, CenterOfCapsule, CapsuleRotation,
                Settings.TraceChannel, Shape, ClearanceParams);
            for (const FOverlapResult& Overlap : Overlaps)
            {
                if (Overlap.bBlockingHit && Overlap.Component.Get() != Hit.GetComponent()) return false;
            }
        }
        OutPoint = Hit.ImpactPoint;
        return true;
    }

    static const AActor* ContextActor(const UObject* Context)
    {
        if (const UActorComponent* Component = Cast<UActorComponent>(Context)) return Component->GetOwner();
        return Cast<AActor>(Context);
    }

    static double EdgeCost(int32 A, int32 B, int32 Latitudes, int32 Longitudes)
    {
        return (Direction(A / Longitudes, A % Longitudes, Latitudes, Longitudes) -
            Direction(B / Longitudes, B % Longitudes, Latitudes, Longitudes)).Size();
    }

    static void Neighbors(int32 Node, int32 Latitudes, int32 Longitudes, TArray<int32, TInlineAllocator<8>>& Out)
    {
        Out.Reset();
        const int32 Lat = Node / Longitudes;
        const int32 Lon = Node % Longitudes;
        for (int32 DLat = -1; DLat <= 1; ++DLat)
        {
            const int32 OtherLat = Lat + DLat;
            if (OtherLat < 0 || OtherLat >= Latitudes) continue;
            for (int32 DLon = -1; DLon <= 1; ++DLon)
            {
                if (DLat == 0 && DLon == 0) continue;
                const int32 Next = Index(OtherLat, Lon + DLon, Longitudes);
                if (Next != Node && !Out.Contains(Next)) Out.Add(Next);
            }
        }
        // All longitude cells describe the same physical pole; connect them without a wrap discontinuity.
        if (Lat == 0 || Lat == Latitudes - 1)
        {
            for (int32 OtherLon = 0; OtherLon < Longitudes; ++OtherLon)
            {
                const int32 Next = Index(Lat, OtherLon, Longitudes);
                if (Next != Node && !Out.Contains(Next)) Out.Add(Next);
            }
        }
    }
}

bool UOrbitAISurfaceNavigation::FindGridRoute(int32 LongitudeSamples, int32 LatitudeSamples,
    const TArray<uint8>& BlockedMask, int32 StartIndex, int32 GoalIndex, TArray<int32>& OutIndices)
{
    OutIndices.Reset();
    if (LongitudeSamples < 8 || LongitudeSamples > 64 || LatitudeSamples < 5 || LatitudeSamples > 33) return false;
    const int32 Count = LongitudeSamples * LatitudeSamples;
    if (BlockedMask.Num() != Count || StartIndex < 0 || GoalIndex < 0 || StartIndex >= Count || GoalIndex >= Count ||
        BlockedMask[StartIndex] || BlockedMask[GoalIndex]) return false;
    TArray<double> G; G.Init(TNumericLimits<double>::Max(), Count);
    TArray<int32> Parent; Parent.Init(INDEX_NONE, Count);
    TArray<uint8> Closed; Closed.Init(0, Count);
    G[StartIndex] = 0.0;
    // Deterministic bounded A*. Linear open-set scan is acceptable for <= 2112 cells and avoids external deps.
    TArray<int32> Open; Open.Add(StartIndex);
    while (!Open.IsEmpty())
    {
        int32 BestSlot = 0;
        double BestF = TNumericLimits<double>::Max();
        for (int32 I = 0; I < Open.Num(); ++I)
        {
            const int32 Node = Open[I];
            const double F = G[Node] + OrbitSurfaceNav::EdgeCost(Node, GoalIndex, LatitudeSamples, LongitudeSamples);
            if (F < BestF - 1e-9 || (FMath::Abs(F - BestF) < 1e-9 && Node < Open[BestSlot]))
            {
                BestF = F; BestSlot = I;
            }
        }
        const int32 Current = Open[BestSlot];
        Open.RemoveAtSwap(BestSlot, 1, EAllowShrinking::No);
        if (Current == GoalIndex)
        {
            for (int32 Node = GoalIndex; Node != INDEX_NONE; Node = Parent[Node])
            {
                OutIndices.Add(Node);
                if (OutIndices.Num() > Count) { OutIndices.Reset(); return false; }
            }
            Algo::Reverse(OutIndices);
            return true;
        }
        Closed[Current] = 1;
        TArray<int32, TInlineAllocator<8>> Adjacent;
        OrbitSurfaceNav::Neighbors(Current, LatitudeSamples, LongitudeSamples, Adjacent);
        for (const int32 Next : Adjacent)
        {
            if (BlockedMask[Next] || Closed[Next]) continue;
            const double NewG = G[Current] + OrbitSurfaceNav::EdgeCost(Current, Next, LatitudeSamples, LongitudeSamples);
            if (NewG + 1e-9 >= G[Next]) continue;
            const bool bNew = G[Next] == TNumericLimits<double>::Max();
            G[Next] = NewG;
            Parent[Next] = Current;
            if (bNew) Open.Add(Next);
        }
    }
    return false;
}

namespace OrbitSurfaceNav
{
    // Use the same requesting Actor exclusion for graph-node sampling, graph-edge
    // clearance, and endpoint connectors. Calling the public world-context API
    // with UWorld here loses the Actor exclusion and causes Characters to block
    // their own freshly generated route.
    static bool IsLegTraversable(UWorld* World, const AActor* IgnoredActor,
        FVector From, FVector To, FVector SphereCenter, float ReferenceRadius,
        const FOrbitSphericalNavigationSettings& Settings)
    {
        if (!World || !ValidVector(From) || !ValidVector(To) ||
            !ValidVector(SphereCenter) || !FMath::IsFinite(ReferenceRadius) || ReferenceRadius < 100.0f ||
            !FMath::IsFinite(Settings.AgentRadius) || Settings.AgentRadius < 0.0f ||
            !FMath::IsFinite(Settings.AgentHalfHeight) || Settings.AgentHalfHeight < 0.0f ||
            !FMath::IsFinite(Settings.SurfaceSkin) || Settings.SurfaceSkin < 0.0f) return false;
        const FVector A = (From - SphereCenter).GetSafeNormal();
        const FVector B = (To - SphereCenter).GetSafeNormal();
        if (A.IsNearlyZero() || B.IsNearlyZero() || FVector::DotProduct(A, B) < 0.0) return false;
        const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0, 1.0));
        const double Spacing = Settings.AgentRadius > 0 ? FMath::Max(20.0, static_cast<double>(Settings.AgentRadius)) : 75.0;
        const int32 Steps = FMath::Clamp(FMath::CeilToInt(Angle * ReferenceRadius / Spacing), 2, 12);
        for (int32 I = 0; I <= Steps; ++I)
        {
            const FVector Dir = FMath::Lerp(A, B, static_cast<double>(I) / Steps).GetSafeNormal();
            FVector Surface;
            if (Dir.IsNearlyZero() || !SampleSurface(World, SphereCenter, ReferenceRadius,
                Dir, Settings, Surface, IgnoredActor)) return false;
        }
        return true;
    }
}


bool UOrbitAISurfaceNavigation::ProjectToSphericalSurface(const UObject* WorldContextObject,
    FVector Candidate, FVector SphereCenter, float ReferenceRadius,
    const FOrbitSphericalNavigationSettings& Settings, FVector& OutSurfacePoint)
{
    OutSurfacePoint = FVector::ZeroVector;
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World || !OrbitSurfaceNav::ValidVector(Candidate) ||
        !OrbitSurfaceNav::ValidVector(SphereCenter) ||
        !FMath::IsFinite(ReferenceRadius) || ReferenceRadius < 100.0f ||
        !FMath::IsFinite(Settings.AgentRadius) || Settings.AgentRadius < 0.0f ||
        !FMath::IsFinite(Settings.AgentHalfHeight) || Settings.AgentHalfHeight < 0.0f ||
        !FMath::IsFinite(Settings.SurfaceSkin) || Settings.SurfaceSkin < 0.0f ||
        !FMath::IsFinite(Settings.TraceOutward) || Settings.TraceOutward < 1.0f ||
        !FMath::IsFinite(Settings.TraceInward) || Settings.TraceInward < 1.0f ||
        !FMath::IsFinite(Settings.MaxSurfaceHeightError) || Settings.MaxSurfaceHeightError < 1.0f ||
        !FMath::IsFinite(Settings.MinimumWalkableNormalDot) ||
        Settings.MinimumWalkableNormalDot < 0.0f || Settings.MinimumWalkableNormalDot > 1.0f)
        return false;
    const FVector UnitDirection = (Candidate - SphereCenter).GetSafeNormal();
    if (UnitDirection.IsNearlyZero()) return false;
    return OrbitSurfaceNav::SampleSurface(World, SphereCenter, ReferenceRadius,
        UnitDirection, Settings, OutSurfacePoint, OrbitSurfaceNav::ContextActor(WorldContextObject));
}

double UOrbitAISurfaceNavigation::SphericalArcDistance(FVector A, FVector B,
    FVector SphereCenter, float ReferenceRadius)
{
    if (!OrbitSurfaceNav::ValidVector(A) || !OrbitSurfaceNav::ValidVector(B) ||
        !OrbitSurfaceNav::ValidVector(SphereCenter) ||
        !FMath::IsFinite(ReferenceRadius) || ReferenceRadius <= 0.0f)
        return -1.0;
    const FVector DirA = (A - SphereCenter).GetSafeNormal();
    const FVector DirB = (B - SphereCenter).GetSafeNormal();
    if (DirA.IsNearlyZero() || DirB.IsNearlyZero()) return -1.0;
    return ReferenceRadius * FMath::Acos(FMath::Clamp(FVector::DotProduct(DirA, DirB), -1.0, 1.0));
}

bool UOrbitAISurfaceNavigation::IsSphericalSurfaceLegTraversable(const UObject* WorldContextObject, FVector From,
    FVector To, FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings)
{
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    return OrbitSurfaceNav::IsLegTraversable(World, OrbitSurfaceNav::ContextActor(WorldContextObject),
        From, To, SphereCenter, ReferenceRadius, Settings);
}

bool UOrbitAISurfaceNavigation::FindSphericalSurfacePath(const UObject* WorldContextObject, FVector Start, FVector Goal,
    FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings,
    TArray<FVector>& OutWaypoints, FString& OutFailureReason)
{
    return FindSphericalSurfacePathProfiled(WorldContextObject, Start, Goal, SphereCenter, ReferenceRadius,
        Settings, OutWaypoints, OutFailureReason, nullptr);
}

bool UOrbitAISurfaceNavigation::FindSphericalSurfacePathProfiled(const UObject* WorldContextObject, FVector Start, FVector Goal,
    FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings,
    TArray<FVector>& OutWaypoints, FString& OutFailureReason, FOrbitSurfaceNavigationProfile* OutProfile)
{
    if (OutProfile) *OutProfile = FOrbitSurfaceNavigationProfile();
    OutWaypoints.Reset();
    OutFailureReason.Reset();
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World || !OrbitSurfaceNav::ValidVector(Start) || !OrbitSurfaceNav::ValidVector(Goal) ||
        !OrbitSurfaceNav::ValidVector(SphereCenter) || !FMath::IsFinite(ReferenceRadius) || ReferenceRadius < 100.0f ||
        Settings.LongitudeSamples < 8 || Settings.LongitudeSamples > 64 ||
        Settings.LatitudeSamples < 5 || Settings.LatitudeSamples > 33 ||
        !FMath::IsFinite(Settings.TraceOutward) || Settings.TraceOutward < 1 ||
        !FMath::IsFinite(Settings.TraceInward) || Settings.TraceInward < 1 ||
        !FMath::IsFinite(Settings.MaxSurfaceHeightError) || Settings.MaxSurfaceHeightError < 1 ||
        !FMath::IsFinite(Settings.MinimumWalkableNormalDot) || Settings.MinimumWalkableNormalDot < 0 || Settings.MinimumWalkableNormalDot > 1 ||
        !FMath::IsFinite(Settings.AgentRadius) || Settings.AgentRadius < 0 ||
        !FMath::IsFinite(Settings.AgentHalfHeight) || Settings.AgentHalfHeight < 0 ||
        !FMath::IsFinite(Settings.SurfaceSkin) || Settings.SurfaceSkin < 0)
    {
        OutFailureReason = TEXT("Invalid world, sphere, or sample settings");
        return false;
    }
    const AActor* IgnoredActor = OrbitSurfaceNav::ContextActor(WorldContextObject);
    const FVector StartDir = (Start - SphereCenter).GetSafeNormal();
    const FVector GoalDir = (Goal - SphereCenter).GetSafeNormal();
    if (StartDir.IsNearlyZero() || GoalDir.IsNearlyZero())
    {
        OutFailureReason = TEXT("Start or goal is at sphere center"); return false;
    }
    const double MaxEndpointOffset = Settings.MaxSurfaceHeightError + FMath::Max(Settings.TraceOutward, Settings.TraceInward);
    if (FMath::Abs((Start - SphereCenter).Size() - ReferenceRadius) > MaxEndpointOffset ||
        FMath::Abs((Goal - SphereCenter).Size() - ReferenceRadius) > MaxEndpointOffset)
    {
        OutFailureReason = TEXT("Start or goal is too far from the reference surface"); return false;
    }
    const double EndpointStart = FPlatformTime::Seconds();
    FVector SurfaceStart, SurfaceGoal;
    if (!OrbitSurfaceNav::SampleSurface(World, SphereCenter, ReferenceRadius, StartDir, Settings, SurfaceStart, IgnoredActor) ||
        !OrbitSurfaceNav::SampleSurface(World, SphereCenter, ReferenceRadius, GoalDir, Settings, SurfaceGoal, IgnoredActor))
    {
        OutFailureReason = TEXT("Start or goal has no acceptable walkable surface"); return false;
    }
    if (OutProfile) OutProfile->EndpointMilliseconds = (FPlatformTime::Seconds() - EndpointStart) * 1000.0;
    const int32 Lon = Settings.LongitudeSamples;
    const int32 Lat = Settings.LatitudeSamples;
    const int32 Count = Lon * Lat;
    // Sample nodes only as needed by endpoint selection and A*. A grid direction is
    // immutable geometry; its world collision, clearance, and ignored actor are not.
    // The three-state array is strictly local to this request, so an obstacle or
    // capsule change on the next request is always observed.
    TArray<FVector> Samples; Samples.SetNum(Count);
    TArray<uint8> SampleState; SampleState.Init(0, Count); // 0 unknown, 1 traversable, 2 blocked
    TArray<FVector> Directions; Directions.SetNum(Count);
    for (int32 I = 0; I < Count; ++I)
    {
        Directions[I] = OrbitSurfaceNav::Direction(I / Lon, I % Lon, Lat, Lon);
    }
    auto SampleNode = [&](int32 I) -> bool
    {
        if (SampleState[I] != 0)
        {
            if (OutProfile) ++OutProfile->ReusedNodeSamples;
            return SampleState[I] == 1;
        }
        const double SampleStart = OutProfile ? FPlatformTime::Seconds() : 0.0;
        const bool bValid = OrbitSurfaceNav::SampleSurface(World, SphereCenter, ReferenceRadius,
            Directions[I], Settings, Samples[I], IgnoredActor);
        SampleState[I] = bValid ? 1 : 2;
        if (OutProfile)
        {
            ++OutProfile->AttemptedNodeSamples;
            if (bValid) ++OutProfile->SampledNodes;
            OutProfile->GraphSamplingMilliseconds += (FPlatformTime::Seconds() - SampleStart) * 1000.0;
        }
        return bValid;
    };
    // Preserve the old semantics: use the closest *traversable* graph node to
    // each endpoint, not merely the geometrically nearest unverified cell.
    // Sort by angular proximity, with index as a deterministic tie-breaker.
    // In the common case only one or two nodes are tested; obstructed nearby
    // nodes fall through to the next closest candidate.
    auto ClosestTraversable = [&](const FVector& EndpointDirection) -> int32
    {
        TArray<int32> Candidates;
        Candidates.Reserve(Count);
        for (int32 I = 0; I < Count; ++I) Candidates.Add(I);
        Candidates.Sort([&](int32 A, int32 B)
        {
            const double DotA = FVector::DotProduct(Directions[A], EndpointDirection);
            const double DotB = FVector::DotProduct(Directions[B], EndpointDirection);
            return DotA == DotB ? A < B : DotA > DotB;
        });
        for (int32 Candidate : Candidates)
        {
            if (SampleNode(Candidate)) return Candidate;
        }
        return INDEX_NONE;
    };
    const int32 StartNode = ClosestTraversable(StartDir);
    const int32 GoalNode = ClosestTraversable(GoalDir);
    if (StartNode == INDEX_NONE || GoalNode == INDEX_NONE)
    {
        OutFailureReason = TEXT("No traversable navigation samples"); return false;
    }
    // Edge collision depends on the requesting actor, current obstacles, and capsule size.
    // Validate lazily when A* explores an edge. Store only this request's results so
    // later requests cannot reuse stale traversability after geometry or gravity changes.
    TSet<uint64> InvalidEdges;
    TSet<uint64> VerifiedEdges;
    auto Key = [](int32 A, int32 B) -> uint64
    {
        return (static_cast<uint64>(FMath::Min(A, B)) << 32) | static_cast<uint32>(FMath::Max(A, B));
    };
    auto EdgeIsValid = [&](int32 A, int32 B) -> bool
    {
        if (!Settings.bValidateEdges) return true;
        const uint64 EdgeKey = Key(A, B);
        if (InvalidEdges.Contains(EdgeKey)) { if (OutProfile) ++OutProfile->ReusedEdgeChecks; return false; }
        if (VerifiedEdges.Contains(EdgeKey)) { if (OutProfile) ++OutProfile->ReusedEdgeChecks; return true; }
        if (OutProfile) ++OutProfile->EdgeChecks;
        const double ValidationStart = OutProfile ? FPlatformTime::Seconds() : 0.0;
        const bool bValid = OrbitSurfaceNav::IsLegTraversable(World, IgnoredActor,
            Samples[A], Samples[B], SphereCenter, ReferenceRadius, Settings);
        if (OutProfile) OutProfile->EdgeValidationMilliseconds += (FPlatformTime::Seconds() - ValidationStart) * 1000.0;
        if (bValid) VerifiedEdges.Add(EdgeKey);
        else { InvalidEdges.Add(EdgeKey); if (OutProfile) ++OutProfile->BlockedEdges; }
        return bValid;
    };
    // Deterministic A*: only explored links incur collision checks. A rejected edge
    // is never relaxed, giving the same collision safety as eagerly checking all links.
    TArray<double> G; G.Init(TNumericLimits<double>::Max(), Count);
    TArray<int32> Parent; Parent.Init(INDEX_NONE, Count);
    TArray<uint8> Closed; Closed.Init(0, Count);
    TArray<int32> Open; Open.Add(StartNode); G[StartNode] = 0;
    bool bFound = false;
    const double SampleMillisecondsBeforeSearch = OutProfile ? OutProfile->GraphSamplingMilliseconds : 0.0;
    const double SearchStart = FPlatformTime::Seconds();
    while (!Open.IsEmpty())
    {
        int32 Best = 0;
        double BestF = TNumericLimits<double>::Max();
        for (int32 I = 0; I < Open.Num(); ++I)
        {
            const int32 N = Open[I];
            const double F = G[N] + (Samples[N] - Samples[GoalNode]).Size();
            if (F < BestF - 1e-6 || (FMath::Abs(F - BestF) < 1e-6 && N < Open[Best]))
            {
                Best = I; BestF = F;
            }
        }
        const int32 Current = Open[Best];
        if (OutProfile) ++OutProfile->ExpandedNodes;
        Open.RemoveAtSwap(Best, 1, EAllowShrinking::No);
        if (Current == GoalNode) { bFound = true; break; }
        Closed[Current] = 1;
        TArray<int32, TInlineAllocator<8>> Adjacent;
        OrbitSurfaceNav::Neighbors(Current, Lat, Lon, Adjacent);
        for (int32 Next : Adjacent)
        {
            if (Closed[Next] || !SampleNode(Next) || !EdgeIsValid(Current, Next)) continue;
            const double NewG = G[Current] + (Samples[Current] - Samples[Next]).Size();
            if (NewG + 1e-6 >= G[Next]) continue;
            const bool bNew = G[Next] == TNumericLimits<double>::Max();
            G[Next] = NewG; Parent[Next] = Current;
            if (bNew) Open.Add(Next);
        }
    }
    if (OutProfile) OutProfile->SearchMilliseconds = FMath::Max(0.0,
        (FPlatformTime::Seconds() - SearchStart) * 1000.0 - OutProfile->EdgeValidationMilliseconds
        - (OutProfile->GraphSamplingMilliseconds - SampleMillisecondsBeforeSearch));
    if (!bFound)
    {
        OutFailureReason = TEXT("No verified route around blocked samples"); return false;
    }
    TArray<int32> ReverseIndices;
    for (int32 N = GoalNode; N != INDEX_NONE; N = Parent[N])
    {
        ReverseIndices.Add(N);
        if (ReverseIndices.Num() > Count) { OutFailureReason = TEXT("Invalid path chain"); return false; }
    }
    Algo::Reverse(ReverseIndices);
    // Verify connectors from the actual requested endpoints to the discrete graph.
    // A nearest cell may be reachable while the direct link from an endpoint is obstructed.
    auto CheckConnector = [&](const FVector& A, const FVector& B) -> bool
    {
        return !Settings.bValidateEdges || OrbitSurfaceNav::IsLegTraversable(World, IgnoredActor,
            A, B, SphereCenter, ReferenceRadius, Settings);
    };
    const double ConnectorStart = FPlatformTime::Seconds();
    const bool bStartConnectorValid = CheckConnector(SurfaceStart, Samples[ReverseIndices[0]]);
    const bool bEndConnectorValid = bStartConnectorValid && CheckConnector(Samples[ReverseIndices.Last()], SurfaceGoal);
    if (OutProfile) OutProfile->ConnectorMilliseconds = (FPlatformTime::Seconds() - ConnectorStart) * 1000.0;
    if (!bStartConnectorValid ||
        !bEndConnectorValid)
    {
        OutFailureReason = TEXT("Endpoint connection to sampled graph is blocked");
        return false;
    }
    // The first graph node can be offset from the start. The caller moves from the real current location.
    // Including it ensures even a same-cell query has a collision-verified destination.
    for (int32 N : ReverseIndices) OutWaypoints.Add(Samples[N]);
    OutWaypoints.Add(SurfaceGoal);
    return true;
}
