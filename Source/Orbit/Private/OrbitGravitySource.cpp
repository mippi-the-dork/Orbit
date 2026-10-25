#include "OrbitGravitySource.h"
#include "OrbitWorldSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

#if WITH_EDITOR
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#endif

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
#if WITH_EDITORONLY_DATA
    OrbitBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("OrbitBillboard"));
    OrbitBillboard->SetupAttachment(SceneRoot);
    OrbitBillboard->SetHiddenInGame(true);
    OrbitBillboard->bIsScreenSizeScaled = true;
#endif
    bReplicates = false; // Network replication remains outside the current implementation.
}

#if WITH_EDITOR
void AOrbitGravitySource::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
#if WITH_EDITORONLY_DATA
    if (!OrbitBillboard || IsTemplate() || !GetWorld() || GetWorld()->IsGameWorld())
    {
        return;
    }

    // Import the designer-supplied Orbit billboard image once per Editor session.
    // The transient sprite is rebuilt after reopening a level and never cooked into Shipping.
    static TStrongObjectPtr<UTexture2D> BillboardTexture;
    if (!BillboardTexture.IsValid())
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Orbit"));
        if (Plugin.IsValid())
        {
            const FString SpritePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Icons/OrbitBillboard.png"));
            BillboardTexture.Reset(FImageUtils::ImportFileAsTexture2D(SpritePath));
        }
    }
    if (BillboardTexture.IsValid() && OrbitBillboard->Sprite != BillboardTexture.Get())
    {
        OrbitBillboard->SetSprite(BillboardTexture.Get());
    }
#endif
}
#endif

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
    Field.VolumeShape = VolumeShape;
    Field.VolumeRadius = FMath::Max(VolumeRadius, 1.0);
    Field.VolumeHalfHeight = FMath::Max(VolumeHalfHeight, 1.0);
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
    return bShowGravityPreview || (bShowVolumeBounds && (Shape == EOrbitGravityShape::Volume || bLimitToBox));
}
#endif

void AOrbitGravitySource::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if ((!bShowGravityPreview && !bShowVolumeBounds) || !GetWorld()) { return; }
#if WITH_EDITOR

    const FVector Origin = GetActorLocation();
    const FQuat Rotation = GetActorQuat();
    const FVector Up = GetActorUpVector();
    const UWorld* World = GetWorld();
    constexpr float DebugLifetime = 0.0f;

    // Show influence bounds independently from the vector preview. Use the same
    // dimensions and source rotation as the actual field evaluator.
    if (bShowVolumeBounds)
    {
        if (Shape == EOrbitGravityShape::Volume)
        {
            const FColor Color = bEnabled ? FColor(65, 205, 145) : FColor(110, 110, 110);
            const double Radius = FMath::Max(VolumeRadius, 1.0);
            const double HalfHeight = FMath::Max(VolumeHalfHeight, 1.0);
            switch (VolumeShape)
            {
            case EOrbitVolumeShape::Box:
                DrawDebugBox(World, Origin, BoxHalfExtents.GetAbs(), Rotation, Color, false, DebugLifetime, 0, 1.6f);
                break;
            case EOrbitVolumeShape::Sphere:
                DrawDebugSphere(World, Origin, Radius, 32, Color, false, DebugLifetime, 0, 1.6f);
                break;
            case EOrbitVolumeShape::Cylinder:
                DrawDebugCylinder(World, Origin - Up * HalfHeight, Origin + Up * HalfHeight, Radius, 32, Color, false, DebugLifetime, 0, 1.6f);
                break;
            case EOrbitVolumeShape::Capsule:
                DrawDebugCapsule(World, Origin, FMath::Max(HalfHeight, Radius), Radius, Rotation, Color, false, DebugLifetime, 0, 1.6f);
                break;
            }
        }
        if (bLimitToBox && (Shape != EOrbitGravityShape::Volume || VolumeShape != EOrbitVolumeShape::Box))
        {
            DrawDebugBox(World, Origin, BoxHalfExtents.GetAbs(), Rotation, FColor::Yellow, false, DebugLifetime, 0, 1.0f);
        }
    }

    if (!bShowGravityPreview || !bEnabled) { return; }
    FOrbitFieldDefinition Field;
    BuildDefinition(Field);
    const double Extent = FMath::Max(PreviewExtent, 50.0);
    const int32 Density = FMath::Clamp(PreviewDensity, 2, 7);
    const double ArrowLength = FMath::Max(PreviewArrowLength, 10.0);
    TArray<FVector> ProbePoints;
    ProbePoints.Reserve(FMath::Max(Density * Density * Density, 200));

    if (Shape == EOrbitGravityShape::Spline && GravitySpline && GravitySpline->GetSplineLength() > 1.0)
    {
        // Probe a true 3D tube, not only the spline's local Up line.
        const double Length = GravitySpline->GetSplineLength();
        const double Radius = MaxDistance > 0.0 ? FMath::Min(Extent, MaxDistance) : Extent;
        const int32 AlongSamples = FMath::Clamp(Density * 3, 6, 21);
        for (int32 I = 0; I < AlongSamples; ++I)
        {
            const double Distance = Length * (static_cast<double>(I) + 0.5) / AlongSamples;
            const FVector Base = GravitySpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
            const FVector Forward = GravitySpline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).GetSafeNormal();
            const FVector SplineUp = GravitySpline->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
            const FVector Right = FVector::CrossProduct(Forward, SplineUp).GetSafeNormal();
            if (Right.IsNearlyZero()) { continue; }
            for (int32 RadialLayer = 1; RadialLayer <= 2; ++RadialLayer)
            {
                const double LayerRadius = Radius * (RadialLayer == 1 ? 0.35 : 0.8);
                for (int32 Slice = 0; Slice < 8; ++Slice)
                {
                    const double Angle = 2.0 * PI * static_cast<double>(Slice) / 8.0;
                    ProbePoints.Add(Base + (SplineUp * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * LayerRadius);
                }
            }
        }
    }
    else
    {
        FVector LocalExtents(Extent);
        if (Shape == EOrbitGravityShape::Volume)
        {
            switch (VolumeShape)
            {
            case EOrbitVolumeShape::Box: LocalExtents = BoxHalfExtents.GetAbs(); break;
            case EOrbitVolumeShape::Sphere: LocalExtents = FVector(FMath::Max(VolumeRadius, 1.0)); break;
            case EOrbitVolumeShape::Cylinder:
                LocalExtents = FVector(VolumeRadius, VolumeRadius, VolumeHalfHeight); break;
            case EOrbitVolumeShape::Capsule:
                LocalExtents = FVector(VolumeRadius, VolumeRadius, FMath::Max(VolumeRadius, VolumeHalfHeight)); break;
            }
        }
        else if (MaxDistance > 0.0)
        {
            LocalExtents = FVector(FMath::Min(Extent, MaxDistance));
        }
        if (bLimitToBox)
        {
            const FVector MaskExtents = BoxHalfExtents.GetAbs();
            LocalExtents.X = FMath::Min(LocalExtents.X, MaskExtents.X);
            LocalExtents.Y = FMath::Min(LocalExtents.Y, MaskExtents.Y);
            LocalExtents.Z = FMath::Min(LocalExtents.Z, MaskExtents.Z);
        }
        for (int32 X = 0; X < Density; ++X)
        {
            for (int32 Y = 0; Y < Density; ++Y)
            {
                for (int32 Z = 0; Z < Density; ++Z)
                {
                    const FVector Grid(
                        ((X + 0.5) * 2.0 / Density - 1.0) * LocalExtents.X,
                        ((Y + 0.5) * 2.0 / Density - 1.0) * LocalExtents.Y,
                        ((Z + 0.5) * 2.0 / Density - 1.0) * LocalExtents.Z);
                    ProbePoints.Add(Origin + Rotation.RotateVector(Grid));
                }
            }
        }
    }

    for (const FVector& Probe : ProbePoints)
    {
        FVector Acceleration;
        if (FOrbitFieldEvaluator::Evaluate(Field, Probe, Acceleration) && !Acceleration.IsNearlyZero())
        {
            double SizeMultiplier = 1.0;
            if (bScalePreviewByStrength && Strength > 1.0)
            {
                SizeMultiplier = FMath::Clamp(Acceleration.Size() / Strength, 0.18, 1.8);
            }
            const float ActualLength = static_cast<float>(ArrowLength * SizeMultiplier);
            DrawDebugDirectionalArrow(World, Probe, Probe + Acceleration.GetSafeNormal() * ActualLength,
                FMath::Clamp(ActualLength * 0.2f, 8.0f, 36.0f), FColor::Cyan, false, DebugLifetime, 0, 1.2f);
        }
    }
#endif // WITH_EDITOR: visualization is a development tool, not shipped gameplay.
}
