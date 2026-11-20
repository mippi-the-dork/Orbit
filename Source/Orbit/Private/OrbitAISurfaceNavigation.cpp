#include "OrbitAISurfaceNavigation.h"
#include "Algo/Reverse.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
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
            if (World->OverlapBlockingTestByChannel(CenterOfCapsule, CapsuleRotation, Settings.TraceChannel,
                Shape, ClearanceParams)) return false;
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

bool UOrbitAISurfaceNavigation::IsSphericalSurfaceLegTraversable(const UObject* WorldContextObject, FVector From,
    FVector To, FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings)
{
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World || !OrbitSurfaceNav::ValidVector(From) || !OrbitSurfaceNav::ValidVector(To) ||
        !OrbitSurfaceNav::ValidVector(SphereCenter) || !FMath::IsFinite(ReferenceRadius) || ReferenceRadius < 100.0f ||
        !FMath::IsFinite(Settings.AgentRadius) || Settings.AgentRadius < 0.0f ||
        !FMath::IsFinite(Settings.AgentHalfHeight) || Settings.AgentHalfHeight < 0.0f ||
        !FMath::IsFinite(Settings.SurfaceSkin) || Settings.SurfaceSkin < 0.0f) return false;
    const AActor* IgnoredActor = OrbitSurfaceNav::ContextActor(WorldContextObject);
    const FVector A = (From - SphereCenter).GetSafeNormal();
    const FVector B = (To - SphereCenter).GetSafeNormal();
    if (A.IsNearlyZero() || B.IsNearlyZero() || FVector::DotProduct(A, B) < 0.0) return false;
    // Up to 32 interpolation samples, with spacing derived from the agent footprint.
    const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0, 1.0));
    const double Spacing = Settings.AgentRadius > 0 ? FMath::Max(20.0, static_cast<double>(Settings.AgentRadius)) : 75.0;
    const int32 Steps = FMath::Clamp(FMath::CeilToInt(Angle * ReferenceRadius / Spacing), 2, 12);
    for (int32 I = 0; I <= Steps; ++I)
    {
        const FVector Dir = FMath::Lerp(A, B, static_cast<double>(I) / Steps).GetSafeNormal();
        FVector Surface;
        if (Dir.IsNearlyZero() || !OrbitSurfaceNav::SampleSurface(World, SphereCenter, ReferenceRadius,
            Dir, Settings, Surface, IgnoredActor)) return false;
    }
    return true;
}

bool UOrbitAISurfaceNavigation::FindSphericalSurfacePath(const UObject* WorldContextObject, FVector Start, FVector Goal,
    FVector SphereCenter, float ReferenceRadius, const FOrbitSphericalNavigationSettings& Settings,
    TArray<FVector>& OutWaypoints, FString& OutFailureReason)
{
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
    FVector SurfaceStart, SurfaceGoal;
    if (!OrbitSurfaceNav::SampleSurface(World, SphereCenter, ReferenceRadius, StartDir, Settings, SurfaceStart, IgnoredActor) ||
        !OrbitSurfaceNav::SampleSurface(World, SphereCenter, ReferenceRadius, GoalDir, Settings, SurfaceGoal, IgnoredActor))
    {
        OutFailureReason = TEXT("Start or goal has no acceptable walkable surface"); return false;
    }
    const int32 Lon = Settings.LongitudeSamples;
    const int32 Lat = Settings.LatitudeSamples;
    const int32 Count = Lon * Lat;
    TArray<FVector> Samples; Samples.SetNum(Count);
    TArray<uint8> Blocked; Blocked.Init(1, Count);
    int32 StartNode = INDEX_NONE, GoalNode = INDEX_NONE;
    double BestStart = -2.0, BestGoal = -2.0;
    for (int32 I = 0; I < Count; ++I)
    {
        const FVector Dir = OrbitSurfaceNav::Direction(I / Lon, I % Lon, Lat, Lon);
        if (!OrbitSurfaceNav::SampleSurface(World, SphereCenter, ReferenceRadius, Dir, Settings, Samples[I], IgnoredActor)) continue;
        Blocked[I] = 0;
        const double DotStart = FVector::DotProduct(Dir, StartDir);
        const double DotGoal = FVector::DotProduct(Dir, GoalDir);
        if (DotStart > BestStart) { BestStart = DotStart; StartNode = I; }
        if (DotGoal > BestGoal) { BestGoal = DotGoal; GoalNode = I; }
    }
    if (StartNode == INDEX_NONE || GoalNode == INDEX_NONE)
    {
        OutFailureReason = TEXT("No traversable navigation samples"); return false;
    }
    // Validate all candidate edges in advance, so the A* route cannot use a link that fails collision.
    // We encode each directed invalid link as a sorted uint64 pair; bidirectional edges share the same key.
    TSet<uint64> InvalidEdges;
    auto Key = [](int32 A, int32 B) -> uint64
    {
        return (static_cast<uint64>(FMath::Min(A, B)) << 32) | static_cast<uint32>(FMath::Max(A, B));
    };
    if (Settings.bValidateEdges)
    {
        for (int32 A = 0; A < Count; ++A)
        {
            if (Blocked[A]) continue;
            TArray<int32, TInlineAllocator<8>> Adjacent;
            OrbitSurfaceNav::Neighbors(A, Lat, Lon, Adjacent);
            for (const int32 B : Adjacent)
            {
                if (B <= A || Blocked[B]) continue;
                const bool bEdgeValid = IsSphericalSurfaceLegTraversable(World, Samples[A], Samples[B],
                    SphereCenter, ReferenceRadius, Settings);
                if (!bEdgeValid) InvalidEdges.Add(Key(A, B));
            }
        }
    }
    // Repeat search after removing obstructed edges using a bounded local A* with the same deterministic ordering.
    TArray<double> G; G.Init(TNumericLimits<double>::Max(), Count);
    TArray<int32> Parent; Parent.Init(INDEX_NONE, Count);
    TArray<uint8> Closed; Closed.Init(0, Count);
    TArray<int32> Open; Open.Add(StartNode); G[StartNode] = 0;
    bool bFound = false;
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
        Open.RemoveAtSwap(Best, 1, EAllowShrinking::No);
        if (Current == GoalNode) { bFound = true; break; }
        Closed[Current] = 1;
        TArray<int32, TInlineAllocator<8>> Adjacent;
        OrbitSurfaceNav::Neighbors(Current, Lat, Lon, Adjacent);
        for (int32 Next : Adjacent)
        {
            if (Blocked[Next] || Closed[Next] || InvalidEdges.Contains(Key(Current, Next))) continue;
            const double NewG = G[Current] + (Samples[Current] - Samples[Next]).Size();
            if (NewG + 1e-6 >= G[Next]) continue;
            const bool bNew = G[Next] == TNumericLimits<double>::Max();
            G[Next] = NewG; Parent[Next] = Current;
            if (bNew) Open.Add(Next);
        }
    }
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
        return !Settings.bValidateEdges || IsSphericalSurfaceLegTraversable(World, A, B,
            SphereCenter, ReferenceRadius, Settings);
    };
    if (!CheckConnector(SurfaceStart, Samples[ReverseIndices[0]]) ||
        !CheckConnector(Samples[ReverseIndices.Last()], SurfaceGoal))
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
