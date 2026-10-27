// Copyright Mippithedork 2026, Inc. All Rights Reserved.

#include "OrbitGravitySource.h"
#include "OrbitWorldSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformTime.h"

#if WITH_EDITOR
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Engine/Level.h"
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
void AOrbitGravitySource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    bStreamlinesCached = false;
    UWorld* World = GetWorld();
    if (!World) { return; }

    if (World->IsGameWorld())
    {
        // Editing the actual PIE actor works immediately, whether edited by
        // Details or Blueprint (source snapshots also rebuild each world tick).
        if (UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Subsystem->RefreshActiveSources();
        }
        return;
    }

    LastEditorPIEMirrorCount = 0;
    if (World->WorldType != EWorldType::Editor || !bMirrorEditorChangesToPIE ||
        !PropertyChangedEvent.MemberProperty)
    {
        return;
    }

    FProperty* EditedProperty = PropertyChangedEvent.MemberProperty;
    // Never copy component pointers, Actor base properties, or transient debug
    // values between worlds. Only user-authored Orbit properties are eligible.
    if (!EditedProperty->HasAnyPropertyFlags(CPF_Edit) ||
        EditedProperty->GetOwnerClass() != AOrbitGravitySource::StaticClass() ||
        EditedProperty->HasAnyPropertyFlags(CPF_Transient))
    {
        return;
    }

    const FGuid EditorGuid = GetActorGuid();
    for (TObjectIterator<AOrbitGravitySource> It; It; ++It)
    {
        AOrbitGravitySource* RuntimeActor = *It;
        if (!IsValid(RuntimeActor) || RuntimeActor == this || RuntimeActor->IsTemplate() ||
            RuntimeActor->GetClass() != GetClass())
        {
            continue;
        }
        UWorld* RuntimeWorld = RuntimeActor->GetWorld();
        if (!RuntimeWorld || RuntimeWorld->WorldType != EWorldType::PIE) { continue; }
        const FGuid RuntimeGuid = RuntimeActor->GetActorGuid();
        const bool bGuidMatches = EditorGuid.IsValid() && RuntimeGuid.IsValid() &&
            EditorGuid == RuntimeGuid;
        // Some editor simulation paths remap GUIDs. Fall back only to the
        // original actor name PLUS exact (PIE-prefix-adjusted) level package.
        const bool bNameAndLevelMatch = RuntimeActor->GetFName() == GetFName() &&
            RuntimeActor->GetLevel() && GetLevel() &&
            RuntimeActor->GetLevel()->GetOutermost()->GetName().EndsWith(
                GetLevel()->GetOutermost()->GetName());
        const bool bIdentityMatches = bGuidMatches || bNameAndLevelMatch;
        if (!bIdentityMatches) { continue; }

        EditedProperty->CopyCompleteValue_InContainer(RuntimeActor, this);
        RuntimeActor->bStreamlinesCached = false;
        if (UOrbitWorldSubsystem* Subsystem = RuntimeWorld->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Subsystem->RefreshActiveSources();
        }
        ++LastEditorPIEMirrorCount;
    }
}

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

void AOrbitGravitySource::SetGravityStrength(double NewStrength)
{
    if (!FMath::IsFinite(NewStrength) || NewStrength < 0.0)
    {
        return;
    }
    Strength = NewStrength;
    if (UWorld* World = GetWorld(); World && World->IsGameWorld())
    {
        if (UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Subsystem->RefreshActiveSources();
        }
    }
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
    Field.VortexSpin = VortexSpin;
    Field.VortexPhysicsMode = VortexPhysicsMode;
    Field.VortexConvergenceRate = VortexConvergenceRate;
    Field.VortexAngularRateRadians = FMath::DegreesToRadians(VortexAngularRate);
    Field.VortexResponseRate = VortexResponseRate;
    Field.VortexMaxFlowSpeed = VortexMaxFlowSpeed;
    Field.VortexMaxAccelerationFactor = VortexMaxAccelerationFactor;
    Field.MaxDistance = MaxDistance;
    Field.ReferenceDistance = InverseSquareReferenceDistance;
    Field.Priority = Priority;
    Field.SourceId = GetUniqueID();
    Field.ChannelMask = GravityChannels.ToMask();
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
    return bShowGravityPreview || bShowStreamlines ||
        (bShowMaxDistanceBoundary && MaxDistance > 0.0) ||
        (bShowVolumeBounds && (Shape == EOrbitGravityShape::Volume || bLimitToBox));
}
#endif

void AOrbitGravitySource::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if ((!bShowGravityPreview && !bShowStreamlines && !bShowVolumeBounds && !bShowMaxDistanceBoundary) || !GetWorld()) { return; }
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

    // Show an actual finite-distance influence boundary even when the arrow preview
    // is disabled. This must mirror the evaluator's distance metric:
    // Point / Directional / Volume: from actor center; Axis: perpendicular to
    // infinite local Z axis; Spline: distance to closest spline polyline segment.
    if (bShowMaxDistanceBoundary && MaxDistance > 0.0)
    {
        const FColor BoundColor = bEnabled ? FColor(70, 180, 245) : FColor(110, 110, 110);
        const double Radius = MaxDistance;
        switch (Shape)
        {
        case EOrbitGravityShape::Point:
        case EOrbitGravityShape::Directional:
        case EOrbitGravityShape::Volume:
            DrawDebugSphere(World, Origin, Radius, 32, BoundColor, false, DebugLifetime, 0, 1.3f);
            break;
        case EOrbitGravityShape::Axis:
        {
            // The actual field is infinite along the axis. Draw only a finite
            // portion near the actor, with visible radial boundaries.
            const double HalfLength = FMath::Max(PreviewExtent, 50.0);
            DrawDebugCylinder(World, Origin - Up * HalfLength, Origin + Up * HalfLength,
                Radius, 32, BoundColor, false, DebugLifetime, 0, 1.3f);
            break;
        }
        case EOrbitGravityShape::Spline:
            if (GravitySpline && GravitySpline->GetSplineLength() > 1.0)
            {
                // A sampled wire tube around the authored centerline. The real
                // field is evaluated against an interpolated polyline, so this
                // wireframe is an approximation on strongly curved segments.
                const double Length = GravitySpline->GetSplineLength();
                const int32 RingCount = FMath::Clamp(FMath::CeilToInt(Length / FMath::Max(Radius * 0.5, 100.0)), 6, 48);
                constexpr int32 RingSides = 16;
                TArray<FVector> PreviousRing;
                PreviousRing.Reserve(RingSides);
                for (int32 RingIndex = 0; RingIndex <= RingCount; ++RingIndex)
                {
                    const double Distance = Length * static_cast<double>(RingIndex) / RingCount;
                    const FVector Center = GravitySpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                    const FVector Forward = GravitySpline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).GetSafeNormal();
                    const FVector SplineUp = GravitySpline->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).GetSafeNormal();
                    const FVector Right = FVector::CrossProduct(Forward, SplineUp).GetSafeNormal();
                    if (Right.IsNearlyZero()) { continue; }
                    TArray<FVector> CurrentRing;
                    CurrentRing.Reserve(RingSides);
                    for (int32 Side = 0; Side < RingSides; ++Side)
                    {
                        const double Angle = 2.0 * PI * static_cast<double>(Side) / RingSides;
                        CurrentRing.Add(Center + (SplineUp * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * Radius);
                    }
                    for (int32 Side = 0; Side < RingSides; ++Side)
                    {
                        DrawDebugLine(World, CurrentRing[Side], CurrentRing[(Side + 1) % RingSides], BoundColor, false, DebugLifetime, 0, 1.2f);
                        if (PreviousRing.Num() == RingSides && Side % 2 == 0)
                        {
                            DrawDebugLine(World, PreviousRing[Side], CurrentRing[Side], BoundColor, false, DebugLifetime, 0, 1.0f);
                        }
                    }
                    PreviousRing = MoveTemp(CurrentRing);
                }
            }
            break;
        }
    }

    // Streamlines follow the normalized gravity VECTOR FIELD, not velocity or
    // momentum. They are not predicted rigid-body trajectories. This feature
    // is editor-only and does not change physics, gravity channels or gameplay.
    if (bShowStreamlines && bEnabled)
    {
        const double Now = FPlatformTime::Seconds();
        const bool bSourceMoved = !CachedStreamlineTransform.Equals(GetActorTransform());
        // Rebuilding a spline field for every traced point every editor frame is
        // expensive. Cache traced segments, redraw them every frame, and refresh
        // the underlying field at most eight times a second while being edited.
        if (!bStreamlinesCached || bSourceMoved || LastStreamlineBuildSeconds < 0.0 ||
            Now - LastStreamlineBuildSeconds >= 0.125)
        {
            CachedStreamlineSegments.Reset();
            CachedStreamlineMarkers.Reset();
            LastStreamlineBuildSeconds = Now;
            CachedStreamlineTransform = GetActorTransform();
            bStreamlinesCached = true;

            FOrbitFieldDefinition StreamField;
            BuildDefinition(StreamField);
            const int32 Level = FMath::Clamp(StreamlineDensity, 1, 10);
            const int32 MaxLines = FMath::Clamp(StreamlineMaxLines, 1, 96);
            const int32 Steps = FMath::Clamp(StreamlineSteps, 8, 128);
            const int32 Budget = FMath::Clamp(StreamlineMaxSegments, 128, 8192);
            const double Extent = FMath::Max(PreviewExtent, 50.0);
            const double Range = bFitPreviewToMaxDistance && MaxDistance > 0.0 ? MaxDistance : Extent;
            const double StepLength = FMath::Clamp(
                (2.0 * Range * FMath::Clamp(StreamlineLength, 0.25, 4.0)) / Steps,
                2.0, 10000000.0);

            TArray<FVector> Candidates;
            if (Shape == EOrbitGravityShape::Spline && GravitySpline && GravitySpline->GetSplineLength() > 1.0)
            {
                // Distribute seeds along the whole curve, throughout the local
                // tubular cross-section, so density remains relative to size.
                const int32 Along = FMath::Clamp(Level * 2 + 4, 6, 24);
                const int32 Sides = FMath::Clamp(Level + 5, 6, 16);
                const int32 RadialLayerCount = FMath::Clamp(1 + (Level - 1) / 4, 1, 3);
                const double Length = GravitySpline->GetSplineLength();
                for (int32 I = 0; I < Along; ++I)
                {
                    const double Dist = Length * (I + 0.5) / Along;
                    const FVector Center = GravitySpline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
                    const FVector Forward = GravitySpline->GetDirectionAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World).GetSafeNormal();
                    const FVector UpVector = GravitySpline->GetUpVectorAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World).GetSafeNormal();
                    const FVector Right = FVector::CrossProduct(Forward, UpVector).GetSafeNormal();
                    if (Right.IsNearlyZero()) { continue; }
                    for (int32 Layer = 1; Layer <= RadialLayerCount; ++Layer)
                    {
                        const double Radius = Range * Layer / (RadialLayerCount + 0.35);
                        for (int32 Side = 0; Side < Sides; ++Side)
                        {
                            const double Angle = 2.0 * PI * Side / Sides;
                            Candidates.Add(Center + (UpVector * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * Radius);
                        }
                    }
                }
            }
            else
            {
                FVector HalfExtents(Range);
                if (Shape == EOrbitGravityShape::Axis)
                {
                    // The physical Axis influence is infinite along local Z.
                    HalfExtents.Z = Extent;
                }
                else if (Shape == EOrbitGravityShape::Volume)
                {
                    switch (VolumeShape)
                    {
                    case EOrbitVolumeShape::Box: HalfExtents = BoxHalfExtents.GetAbs(); break;
                    case EOrbitVolumeShape::Sphere: HalfExtents = FVector(FMath::Max(VolumeRadius, 1.0)); break;
                    case EOrbitVolumeShape::Cylinder: HalfExtents = FVector(VolumeRadius, VolumeRadius, VolumeHalfHeight); break;
                    case EOrbitVolumeShape::Capsule:
                        HalfExtents = FVector(VolumeRadius, VolumeRadius, FMath::Max(VolumeRadius, VolumeHalfHeight)); break;
                    }
                    // Clip the seed region to the effective displayed area.
                    const double Limit = MaxDistance > 0.0
                        ? (bFitPreviewToMaxDistance ? MaxDistance : FMath::Min(Extent, MaxDistance)) : Extent;
                    HalfExtents.X = FMath::Min(HalfExtents.X, Limit);
                    HalfExtents.Y = FMath::Min(HalfExtents.Y, Limit);
                    HalfExtents.Z = FMath::Min(HalfExtents.Z, Limit);
                }
                if (bLimitToBox)
                {
                    HalfExtents.X = FMath::Min(HalfExtents.X, FMath::Abs(BoxHalfExtents.X));
                    HalfExtents.Y = FMath::Min(HalfExtents.Y, FMath::Abs(BoxHalfExtents.Y));
                    HalfExtents.Z = FMath::Min(HalfExtents.Z, FMath::Abs(BoxHalfExtents.Z));
                }
                const int32 SamplesPerAxis = Level + 2;
                for (int32 I = 0; I < SamplesPerAxis; ++I)
                {
                    for (int32 J = 0; J < SamplesPerAxis; ++J)
                    {
                        for (int32 K = 0; K < SamplesPerAxis; ++K)
                        {
                            const FVector LocalPoint(
                                ((I + 0.5) * 2.0 / SamplesPerAxis - 1.0) * HalfExtents.X,
                                ((J + 0.5) * 2.0 / SamplesPerAxis - 1.0) * HalfExtents.Y,
                                ((K + 0.5) * 2.0 / SamplesPerAxis - 1.0) * HalfExtents.Z);
                            Candidates.Add(Origin + Rotation.RotateVector(LocalPoint));
                        }
                    }
                }
            }

            // Filter all candidates before thinning so invalid/out-of-volume
            // seeds cannot consume the line budget. Selection is evenly spread
            // through the FULL sampling region, not concentrated at one end.
            TArray<FVector> ValidSeeds;
            ValidSeeds.Reserve(Candidates.Num());
            for (const FVector& Candidate : Candidates)
            {
                FVector Acceleration;
                if (FOrbitFieldEvaluator::Evaluate(StreamField, Candidate, Acceleration) &&
                    Acceleration.SizeSquared() > 1.0e-8)
                {
                    ValidSeeds.Add(Candidate);
                }
            }

            // Density controls both spatial seed resolution AND the number of
            // lines. Max Lines is a ceiling, not a count that ignores Density.
            const int32 DesiredLines = FMath::Clamp(
                FMath::RoundToInt(MaxLines * (0.1 + 0.09 * Level)), 1, MaxLines);
            const int32 LineCount = FMath::Min(DesiredLines, ValidSeeds.Num());
            for (int32 LineIndex = 0; LineIndex < LineCount && CachedStreamlineSegments.Num() < Budget; ++LineIndex)
            {
                const int32 SeedIndex = LineCount > 1
                    ? static_cast<int32>((static_cast<int64>(LineIndex) * (ValidSeeds.Num() - 1)) / (LineCount - 1)) : 0;
                const int32 SegmentStart = CachedStreamlineSegments.Num();
                TArray<FVector> TracedPoints;
                const int32 AllowedSteps = FMath::Min(Steps, Budget - SegmentStart);
                const int32 SegmentCount = FOrbitFieldEvaluator::TraceStreamline(
                    StreamField, ValidSeeds[SeedIndex], StepLength, AllowedSteps, TracedPoints);
                for (int32 PointIndex = 0; PointIndex < SegmentCount; ++PointIndex)
                {
                    CachedStreamlineSegments.Emplace(TracedPoints[PointIndex], TracedPoints[PointIndex + 1]);
                }
                if (bShowStreamlineDirectionMarkers && SegmentCount >= 2)
                {
                    const TPair<FVector, FVector>& Segment = CachedStreamlineSegments[SegmentStart + SegmentCount / 2];
                    const FVector Unit = (Segment.Value - Segment.Key).GetSafeNormal();
                    const FVector Mid = (Segment.Key + Segment.Value) * 0.5;
                    const double MarkerLength = FMath::Clamp(StepLength * 0.9, 10.0, 75.0);
                    CachedStreamlineMarkers.Emplace(Mid - Unit * MarkerLength * 0.5, Mid + Unit * MarkerLength * 0.5);
                }
            }
        }

        const float Width = FMath::Clamp(StreamlineThickness, 0.5f, 4.0f);
        const FColor CurveColor(194, 116, 250);
        for (const TPair<FVector, FVector>& Segment : CachedStreamlineSegments)
        {
            DrawDebugLine(World, Segment.Key, Segment.Value, CurveColor, false, DebugLifetime, 0, Width);
        }
        if (bShowStreamlineDirectionMarkers)
        {
            for (const TPair<FVector, FVector>& Arrow : CachedStreamlineMarkers)
            {
                const float MarkerLength = static_cast<float>((Arrow.Value - Arrow.Key).Size());
                DrawDebugDirectionalArrow(World, Arrow.Key, Arrow.Value,
                    FMath::Clamp(MarkerLength * 0.3f, 6.0f, 20.0f), FColor(244, 194, 255),
                    false, DebugLifetime, 0, Width + 0.4f);
            }
        }
    }
    else
    {
        // Turning off the preview must not retain or regenerate debug geometry.
        CachedStreamlineSegments.Reset();
        CachedStreamlineMarkers.Reset();
        bStreamlinesCached = false;
    }

    if (!bShowGravityPreview || !bEnabled) { return; }
    FOrbitFieldDefinition Field;
    BuildDefinition(Field);
    const double Extent = FMath::Max(PreviewExtent, 50.0);
    // A dimensionless resolution across the full source. Samples remain proportional
    // when the influence radius changes; they do not grow as radius cubed.
    const int32 Level = FMath::Clamp(PreviewDensity, 1, 10);
    const int32 Density = Level + 2; // 3..12 samples per local-space axis.
    const int32 ArrowBudget = FMath::Clamp(PreviewMaxArrows, 64, 2048);
    const double ArrowLength = FMath::Max(PreviewArrowLength, 10.0);
    // Keep the sample count (and arrow size) stable as the influence expands.
    // Unbounded sources and disabled Auto Fit retain the Preview Extent cap.
    const double SamplingRadius = bFitPreviewToMaxDistance && MaxDistance > 0.0 ? MaxDistance : Extent;
    TArray<FVector> ProbePoints;
    ProbePoints.Reserve(FMath::Max(Density * Density * Density, 200));

    if (Shape == EOrbitGravityShape::Spline && GravitySpline && GravitySpline->GetSplineLength() > 1.0)
    {
        // Probe a true 3D tube, not only the spline's local Up line.
        const double Length = GravitySpline->GetSplineLength();
        const double Radius = MaxDistance > 0.0 ? FMath::Min(SamplingRadius, MaxDistance) : SamplingRadius;
        // Resolution applies both along the authored curve and across its tube.
        // Samples are normalized to the entire spline length and effective radius.
        const int32 AlongSamples = FMath::Clamp(Level * 2 + 4, 6, 24);
        const int32 RadialLayers = FMath::Clamp(1 + (Level - 1) / 3, 1, 4);
        const int32 AngularSamples = FMath::Clamp(Level + 5, 6, 16);
        for (int32 I = 0; I < AlongSamples; ++I)
        {
            const double Distance = Length * (static_cast<double>(I) + 0.5) / AlongSamples;
            const FVector Base = GravitySpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
            const FVector Forward = GravitySpline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).GetSafeNormal();
            const FVector SplineUp = GravitySpline->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
            const FVector Right = FVector::CrossProduct(Forward, SplineUp).GetSafeNormal();
            if (Right.IsNearlyZero()) { continue; }
            for (int32 RadialLayer = 1; RadialLayer <= RadialLayers; ++RadialLayer)
            {
                const double LayerRadius = Radius * (static_cast<double>(RadialLayer) / (RadialLayers + 0.2));
                for (int32 Slice = 0; Slice < AngularSamples; ++Slice)
                {
                    const double Angle = 2.0 * PI * static_cast<double>(Slice) / AngularSamples;
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
            // The visible Volume boundary and finite Max Distance intersect.
            // Sample the INTERSECTION's bounding box rather than wasting dense
            // previews in space where Evaluate() will reject every candidate.
            // With Auto Fit off, Preview Extent is an additional visualization cap.
            const double VolumePreviewLimit = MaxDistance > 0.0
                ? (bFitPreviewToMaxDistance ? MaxDistance : FMath::Min(Extent, MaxDistance))
                : Extent;
            LocalExtents.X = FMath::Min(LocalExtents.X, VolumePreviewLimit);
            LocalExtents.Y = FMath::Min(LocalExtents.Y, VolumePreviewLimit);
            LocalExtents.Z = FMath::Min(LocalExtents.Z, VolumePreviewLimit);
        }
        else if (MaxDistance > 0.0)
        {
            if (Shape == EOrbitGravityShape::Axis)
            {
                // Axis Max Distance is radial only; along its axis the field is infinite.
                const double RadialRange = FMath::Min(SamplingRadius, MaxDistance);
                LocalExtents = FVector(RadialRange, RadialRange, Extent);
            }
            else
            {
                LocalExtents = FVector(FMath::Min(SamplingRadius, MaxDistance));
            }
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

    // Spread the arrow budget across the complete sampling region, including the
    // beginning and end. High density costs are bounded independently of radius.
    const int32 SampleCount = ProbePoints.Num();
    const int32 DrawCount = FMath::Min(SampleCount, ArrowBudget);
    for (int32 DrawIndex = 0; DrawIndex < DrawCount; ++DrawIndex)
    {
        const int32 ProbeIndex = (DrawCount > 1)
            ? static_cast<int32>((static_cast<int64>(DrawIndex) * (SampleCount - 1)) / (DrawCount - 1))
            : 0;
        const FVector& Probe = ProbePoints[ProbeIndex];
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
