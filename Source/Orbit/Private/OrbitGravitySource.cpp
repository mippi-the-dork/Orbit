#include "OrbitGravitySource.h"
#include "OrbitWorldSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AOrbitGravitySource::AOrbitGravitySource()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);
    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DownDirection"));
    DirectionArrow->SetupAttachment(SceneRoot);
    DirectionArrow->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
    GravitySpline = CreateDefaultSubobject<USplineComponent>(TEXT("GravitySpline"));
    GravitySpline->SetupAttachment(SceneRoot);
    GravitySpline->SetClosedLoop(false);
    bReplicates = false; // Source replication is deliberately not claimed for v0.1.
}

void AOrbitGravitySource::BeginPlay()
{
    Super::BeginPlay();
    if (UWorld* World = GetWorld())
    {
        if (UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Subsystem->RegisterSource(this);
        }
    }
}

void AOrbitGravitySource::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Subsystem->UnregisterSource(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void AOrbitGravitySource::BuildDefinition(FOrbitFieldDefinition& Field) const
{
    Field = FOrbitFieldDefinition();
    Field.Shape = Shape;
    Field.Direction = Direction;
    Field.Falloff = Falloff;
    Field.Combine = Combine;
    Field.Center = GetActorLocation();
    Field.Axis = GetActorUpVector();
    Field.DirectionVector = -GetActorUpVector();
    Field.Rotation = GetActorQuat();
    Field.Strength = Strength;
    Field.MaxDistance = MaxDistance;
    Field.ReferenceDistance = InverseSquareReferenceDistance;
    Field.Priority = Priority;
    Field.SourceId = GetUniqueID();
    Field.bPhysics = bAffectPhysics;
    Field.bCharacters = bAffectCharacters;
    Field.bLimitToBox = bLimitToBox;
    Field.BoxExtents = BoxHalfExtents.GetAbs();
    if (Shape == EOrbitGravityShape::Spline && GravitySpline)
    {
        const double Length = GravitySpline->GetSplineLength();
        const int32 Samples = FMath::Clamp(FMath::CeilToInt(Length / FMath::Max(SplineSampleSpacing, 25.0)), 1, 2048);
        Field.Curve.Reserve(Samples + 1);
        Field.CurveUp.Reserve(Samples + 1);
        for (int32 I = 0; I <= Samples; ++I)
        {
            const double Dist = Length * static_cast<double>(I) / Samples;
            Field.Curve.Add(GravitySpline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World));
            Field.CurveUp.Add(GravitySpline->GetUpVectorAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World));
        }
    }
}

FOrbitGravitySample AOrbitGravitySource::SampleSource(const FVector& WorldLocation) const
{
    FOrbitGravitySample Result;
    if (!bEnabled) { return Result; }
    FOrbitFieldDefinition Field;
    BuildDefinition(Field);
    TArray<FOrbitFieldDefinition> Single;
    Single.Add(MoveTemp(Field));
    return FOrbitFieldEvaluator::Combine(Single, WorldLocation);
}

#if WITH_EDITOR
bool AOrbitGravitySource::ShouldTickIfViewportsOnly() const
{
    return bShowGravityPreview;
}
#endif

void AOrbitGravitySource::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bShowGravityPreview || !bEnabled || !GetWorld()) { return; }

    // 2D slices for point/axis/directional and cross-section samples for spline.
    TArray<FVector, TInlineAllocator<32>> ProbePoints;
    const double ProbeRadius = MaxDistance > 0.0 ? FMath::Clamp(MaxDistance * 0.5, 150.0, 800.0) : 450.0;
    if (Shape == EOrbitGravityShape::Spline && GravitySpline && GravitySpline->GetSplineLength() > 1.0)
    {
        const double Length = GravitySpline->GetSplineLength();
        for (int32 I = 0; I < 12; ++I)
        {
            const double Along = Length * static_cast<double>(I) / 12.0;
            const FVector Base = GravitySpline->GetLocationAtDistanceAlongSpline(Along, ESplineCoordinateSpace::World);
            const FVector Up = GravitySpline->GetUpVectorAtDistanceAlongSpline(Along, ESplineCoordinateSpace::World);
            ProbePoints.Add(Base + Up * ProbeRadius);
        }
    }
    else
    {
        const FVector Origin = GetActorLocation();
        for (int32 X = -2; X <= 2; ++X)
        {
            for (int32 Y = -2; Y <= 2; ++Y)
            {
                ProbePoints.Add(Origin + GetActorForwardVector() * (X * ProbeRadius * 0.5) + GetActorRightVector() * (Y * ProbeRadius * 0.5) + GetActorUpVector() * (0.2 * ProbeRadius));
            }
        }
    }
    FOrbitFieldDefinition Field;
    BuildDefinition(Field);
    for (const FVector& Probe : ProbePoints)
    {
        FVector Acceleration;
        if (FOrbitFieldEvaluator::Evaluate(Field, Probe, Acceleration) && !Acceleration.IsNearlyZero())
        {
            DrawDebugDirectionalArrow(GetWorld(), Probe, Probe + Acceleration.GetSafeNormal() * 125.0, 25.0, FColor::Cyan, false, 0.0f, 0, 1.5f);
        }
    }
}
