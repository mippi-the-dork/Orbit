#include "EnvQueryTest_OrbitSphericalDistance.h"
#include "OrbitAISurfaceNavigation.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

UEnvQueryTest_OrbitSphericalDistance::UEnvQueryTest_OrbitSphericalDistance(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Cost = EEnvTestCost::Low;
    // A distance test should score candidates by default, not accidentally
    // filter every point against unconfigured float thresholds. EQS authors
    // can explicitly switch to a filtering purpose when desired.
    TestPurpose = EEnvTestPurpose::Score;
    ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
    DistanceTo = UEnvQueryContext_Querier::StaticClass();
    SetWorkOnFloatValues(true);
}

float UEnvQueryTest_OrbitSphericalDistance::CalculateDistance(const FVector& A, const FVector& B,
    const FVector& Center, float Radius)
{
    const double Arc = UOrbitAISurfaceNavigation::SphericalArcDistance(A, B, Center, Radius);
    return Arc >= 0.0 && FMath::IsFinite(Arc) ? static_cast<float>(Arc) : -1.0f;
}

void UEnvQueryTest_OrbitSphericalDistance::RunTest(FEnvQueryInstance& QueryInstance) const
{
    if (!QueryInstance.Owner.IsValid() || !FMath::IsFinite(SurfaceRadius) || SurfaceRadius <= 0.0f) return;
    FVector QueryCenter = SphereCenter;
    if (bUseSphereCenterContext)
    {
        TArray<FVector> Centers;
        if (!SphereCenterContext || !QueryInstance.PrepareContext(SphereCenterContext.Get(), Centers) ||
            Centers.IsEmpty()) return;
        QueryCenter = Centers[0];
    }
    if (QueryCenter.ContainsNaN()) return;
    TArray<FVector> ContextLocations;
    if (!QueryInstance.PrepareContext(DistanceTo.Get(), ContextLocations) || ContextLocations.IsEmpty()) return;
    FloatValueMin.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
    FloatValueMax.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
    const float MinThreshold = FloatValueMin.GetValue();
    const float MaxThreshold = FloatValueMax.GetValue();
    for (FEnvQueryInstance::FItemIterator It(this, QueryInstance); It; ++It)
    {
        const FVector Item = GetItemLocation(QueryInstance, It);
        for (const FVector& ContextLocation : ContextLocations)
        {
            const float Arc = CalculateDistance(Item, ContextLocation, QueryCenter, SurfaceRadius);
            if (Arc >= 0.0f)
            {
                It.SetScore(TestPurpose, FilterType, Arc, MinThreshold, MaxThreshold);
            }
        }
    }
}

FText UEnvQueryTest_OrbitSphericalDistance::GetDescriptionTitle() const
{
    return NSLOCTEXT("OrbitEQS", "SphericalDistanceTitle", "Orbit: Spherical Distance");
}

FText UEnvQueryTest_OrbitSphericalDistance::GetDescriptionDetails() const
{
    return NSLOCTEXT("OrbitEQS", "SphericalDistanceDetail",
        "Great-circle distance on the configured sphere or center context; excludes obstacle travel cost.");
}
