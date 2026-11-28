#include "EnvQueryGenerator_OrbitSurfacePoints.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

UEnvQueryGenerator_OrbitSurfacePoints::UEnvQueryGenerator_OrbitSurfacePoints(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ItemType = UEnvQueryItemType_Point::StaticClass();
    bAutoSortTests = true;
}

FVector UEnvQueryGenerator_OrbitSurfacePoints::SampleDirection(int32 Index, int32 Count)
{
    if (Count <= 0 || Index < 0 || Index >= Count) return FVector::ZeroVector;
    const double Z = 1.0 - 2.0 * (static_cast<double>(Index) + 0.5) / Count;
    const double PlanarRadius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
    const double Angle = Index * (PI * (3.0 - FMath::Sqrt(5.0)));
    return FVector(PlanarRadius * FMath::Cos(Angle), PlanarRadius * FMath::Sin(Angle), Z);
}

FVector UEnvQueryGenerator_OrbitSurfacePoints::SampleDirectionInCap(int32 Index, int32 Count,
    const FVector& CapAxis, float CapAngleDegrees)
{
    if (Count <= 0 || Index < 0 || Index >= Count || CapAxis.ContainsNaN() ||
        CapAxis.IsNearlyZero() || !FMath::IsFinite(CapAngleDegrees) || CapAngleDegrees <= 0.0f)
    {
        return FVector::ZeroVector;
    }
    const FVector Axis = CapAxis.GetSafeNormal();
    const double Angle = FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(CapAngleDegrees), 1.0, 180.0));
    // Equal-area distribution from the cap center to its boundary, including both hemispheres
    // for large caps. No allocation, random state, shared cache, or per-agent mutation.
    const double U = (static_cast<double>(Index) + 0.5) / Count;
    const double CosTheta = 1.0 - U * (1.0 - FMath::Cos(Angle));
    const double SinTheta = FMath::Sqrt(FMath::Max(0.0, 1.0 - CosTheta * CosTheta));
    const double Phi = Index * (PI * (3.0 - FMath::Sqrt(5.0)));
    FVector TangentX, TangentY;
    Axis.FindBestAxisVectors(TangentX, TangentY);
    return (Axis * CosTheta + TangentX * (SinTheta * FMath::Cos(Phi)) +
        TangentY * (SinTheta * FMath::Sin(Phi))).GetSafeNormal();
}

void UEnvQueryGenerator_OrbitSurfacePoints::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
    const UObject* Owner = QueryInstance.Owner.Get();
    if (!IsValid(Owner) || !FMath::IsFinite(SurfaceRadius) || SurfaceRadius <= 0.0f) return;
    FVector QueryCenter = SphereCenter;
    if (bUseSphereCenterContext)
    {
        TArray<FVector> Centers;
        if (!SphereCenterContext || !QueryInstance.PrepareContext(SphereCenterContext.Get(), Centers) ||
            Centers.IsEmpty()) return;
        QueryCenter = Centers[0];
    }
    if (QueryCenter.ContainsNaN()) return;
    const int32 Count = FMath::Clamp(SampleCount, 8, 256);
    FVector RegionAxis = FVector::ZeroVector;
    if (bLimitToQuerierRegion)
    {
        TArray<FVector> Locations;
        if (!QueryInstance.PrepareContext(UEnvQueryContext_Querier::StaticClass(), Locations) ||
            Locations.IsEmpty()) return;
        RegionAxis = (Locations[0] - QueryCenter).GetSafeNormal();
        // An undefined radial direction cannot safely define a local region.
        if (RegionAxis.IsNearlyZero()) return;
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector Direction = bLimitToQuerierRegion
            ? SampleDirectionInCap(Index, Count, RegionAxis, QuerierRegionAngleDegrees)
            : SampleDirection(Index, Count);
        if (Direction.IsNearlyZero()) continue;
        const FVector Candidate = QueryCenter + Direction * SurfaceRadius;
        FVector Point = Candidate;
        if (bProjectToWalkableSurface && !UOrbitAISurfaceNavigation::ProjectToSphericalSurface(
            Owner, Candidate, QueryCenter, SurfaceRadius, NavigationSettings, Point))
        {
            continue;
        }
        QueryInstance.AddItemData<UEnvQueryItemType_Point>(FNavLocation(Point));
    }
}

FText UEnvQueryGenerator_OrbitSurfacePoints::GetDescriptionTitle() const
{
    return NSLOCTEXT("OrbitEQS", "SphereGeneratorTitle", "Orbit: Spherical Surface Points");
}

FText UEnvQueryGenerator_OrbitSurfacePoints::GetDescriptionDetails() const
{
    return NSLOCTEXT("OrbitEQS", "SphereGeneratorDetail",
        "Deterministic whole-sphere or querier-centered candidates; optional native center context and collision projection.");
}
