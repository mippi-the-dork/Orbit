#include "OrbitGravityDebugVolumeActor.h"
#include "OrbitGravityDebugDrawComponent.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitSettings.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"

AOrbitGravityDebugVolumeActor::AOrbitGravityDebugVolumeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    SetActorEnableCollision(false);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);
    BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DebugVolumeBounds"));
    BoundsBox->SetupAttachment(SceneRoot);
    BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundsBox->SetGenerateOverlapEvents(false);
    BoundsBox->SetHiddenInGame(true);
    BoundsBox->SetBoxExtent(FVector(500.0));
    GravityArrows = CreateDefaultSubobject<UOrbitGravityDebugDrawComponent>(TEXT("GravityArrows"));
    GravityArrows->SetupAttachment(SceneRoot);
}

FIntVector AOrbitGravityDebugVolumeActor::ComputeGridCounts(const FVector& LocalSize,
    float SamplesPerMeter, int32 Budget)
{
    const FVector Size = LocalSize.GetAbs();
    const double DensitySafe = FMath::Clamp(static_cast<double>(SamplesPerMeter), 0.05, 10.0);
    const int32 Limit = FMath::Clamp(Budget, 1, 8192);
    // One sample per uniform cell. Increasing box size increases sample count.
    auto CountAtSpacing = [&Size](double Spacing)
    {
        return FIntVector(
            FMath::Clamp(FMath::CeilToInt(Size.X / Spacing), 1, 2048),
            FMath::Clamp(FMath::CeilToInt(Size.Y / Spacing), 1, 2048),
            FMath::Clamp(FMath::CeilToInt(Size.Z / Spacing), 1, 2048));
    };
    double Spacing = 100.0 / DensitySafe;
    FIntVector Counts = CountAtSpacing(Spacing);
    for (int32 Attempt = 0; Attempt < 64; ++Attempt)
    {
        const int64 Total = static_cast<int64>(Counts.X) * Counts.Y * Counts.Z;
        if (Total <= Limit) { break; }
        Spacing *= FMath::Max(1.04, FMath::Pow(static_cast<double>(Total) / Limit, 1.0 / 3.0) * 1.015);
        Counts = CountAtSpacing(Spacing);
    }
    return Counts;
}

void AOrbitGravityDebugVolumeActor::UpdateBoundsBox()
{
    if (!BoundsBox) { return; }
    const FVector Min = CornerA.ComponentMin(CornerB);
    const FVector Max = CornerA.ComponentMax(CornerB);
    BoundsBox->SetRelativeLocation((Min + Max) * 0.5);
    BoundsBox->SetBoxExtent((Max - Min) * 0.5, false);
}

void AOrbitGravityDebugVolumeActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateBoundsBox();
    if (EvaluationMode == EOrbitDebugVolumeEvaluationMode::Automatic)
    {
        RefreshGravityPreview();
    }
    else
    {
        // Construction and transform edits must never resample a frozen/manual field.
        RedrawCachedPreview();
    }
}

#if WITH_EDITOR
void AOrbitGravityDebugVolumeActor::PostEditChangeProperty(FPropertyChangedEvent& ChangedEvent)
{
    Super::PostEditChangeProperty(ChangedEvent);
    UpdateBoundsBox();
    if (EvaluationMode == EOrbitDebugVolumeEvaluationMode::Automatic)
    {
        RefreshGravityPreview();
    }
    else
    {
        // Appearance changes can redraw a frozen field without changing its values.
        RedrawCachedPreview();
    }
}
#endif

void AOrbitGravityDebugVolumeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (EvaluationMode != EOrbitDebugVolumeEvaluationMode::Automatic)
    {
        return;
    }
    TimeUntilRefresh -= DeltaSeconds;
    if (TimeUntilRefresh <= 0.0f)
    {
        TimeUntilRefresh = FMath::Max(0.05f, RefreshInterval);
        // Visibility still belongs to the render components / Actor, not a private toggle.
        if (GravityArrows && GravityArrows->IsVisible() && !IsHidden())
        {
            RefreshGravityPreview();
        }
    }
}

void AOrbitGravityDebugVolumeActor::RefreshGravityPreview()
{
    if (!GravityArrows || !GetWorld() || IsTemplate()) { return; }
    UOrbitWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UOrbitWorldSubsystem>();
    // The runtime world subsystem intentionally does not exist in editor worlds.
    // Rebuild the same field snapshots once per refresh for viewport authoring,
    // and evaluate them with the same pure field-combination function.
    TArray<FOrbitFieldDefinition> EditorFields;
#if WITH_EDITOR
    if (!Subsystem && !GetWorld()->IsGameWorld())
    {
        for (TActorIterator<AOrbitGravitySource> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It) && It->bEnabled)
            {
                FOrbitFieldDefinition& Field = EditorFields.AddDefaulted_GetRef();
                It->BuildDefinition(Field);
            }
        }
        for (TObjectIterator<UOrbitGravitySourceComponent> It; It; ++It)
        {
            UOrbitGravitySourceComponent* Source = *It;
            if (IsValid(Source) && Source->IsRegistered() && Source->bEnabled &&
                Source->GetWorld() == GetWorld() && IsValid(Source->GetOwner()))
            {
                FOrbitFieldDefinition& Field = EditorFields.AddDefaulted_GetRef();
                Source->BuildDefinition(Field);
            }
        }
    }
#endif
    const FVector Min = CornerA.ComponentMin(CornerB);
    const FVector Max = CornerA.ComponentMax(CornerB);
    const FVector Size = Max - Min;
    const FIntVector Counts = ComputeGridCounts(Size, Density, MaxArrows);
    const int32 SampleCount = Counts.X * Counts.Y * Counts.Z;
    TArray<FOrbitDebugVolumeCachedSample> NewSamples;
    NewSamples.Reserve(SampleCount);
    const FTransform WorldTransform = GetActorTransform();
    const double EvaluationStartSeconds = FPlatformTime::Seconds();
    for (int32 Z = 0; Z < Counts.Z; ++Z)
    for (int32 Y = 0; Y < Counts.Y; ++Y)
    for (int32 X = 0; X < Counts.X; ++X)
    {
        const FVector PositionLocal = Min + FVector(
            (X + 0.5) * Size.X / Counts.X,
            (Y + 0.5) * Size.Y / Counts.Y,
            (Z + 0.5) * Size.Z / Counts.Z);
        const FVector Position = WorldTransform.TransformPosition(PositionLocal);
        FOrbitGravitySample Sample;
        if (Subsystem)
        {
            Sample = Subsystem->SampleGravityWithChannels(Position, GravityChannels, SampleMode);
        }
        else
        {
            const UOrbitSettings* Settings = GetDefault<UOrbitSettings>();
            Sample = FOrbitFieldEvaluator::Combine(EditorFields, Position, SampleMode,
                GravityChannels.ToMask(), Settings->OverrideResolution);
            if (!Sample.bHasInfluence && Settings->bUseWorldGravityOutsideFields)
            {
                Sample.Acceleration = FVector(0.0, 0.0, GetWorld()->GetGravityZ());
                Sample.Magnitude = Sample.Acceleration.Size();
                Sample.bHasValidDirection = Sample.Magnitude > UE_DOUBLE_SMALL_NUMBER;
            }
        }
        FOrbitDebugVolumeCachedSample& Snapshot = NewSamples.AddDefaulted_GetRef();
        Snapshot.Position = Position;
        Snapshot.Acceleration = Sample.Acceleration;
        Snapshot.bHasValidDirection = Sample.bHasValidDirection;
    }
    CachedSamples = MoveTemp(NewSamples);
    LastSampleCount = CachedSamples.Num();
    ++EvaluationCount;
    LastEvaluationMilliseconds = static_cast<float>((FPlatformTime::Seconds() - EvaluationStartSeconds) * 1000.0);
    TimeUntilRefresh = FMath::Max(0.05f, RefreshInterval);
    RedrawCachedPreview();
}

void AOrbitGravityDebugVolumeActor::RedrawCachedPreview()
{
    if (!GravityArrows || IsTemplate()) { return; }
    TArray<FOrbitGravityDebugLine> NewLines;
    NewLines.Reserve(CachedSamples.Num() * 3);
    const FColor RenderColor = ArrowColor.ToFColor(true);
    const float Thickness = FMath::Clamp(ArrowThickness, 0.1f, 20.0f);
    int32 ArrowCount = 0;
    for (const FOrbitDebugVolumeCachedSample& Snapshot : CachedSamples)
    {
        const FVector Position = Snapshot.Position;
        const double Magnitude = Snapshot.Acceleration.Size();
        const bool bValid = Snapshot.bHasValidDirection && Magnitude > UE_DOUBLE_SMALL_NUMBER;
        if (!bValid)
        {
            if (bShowZeroGravitySamples)
            {
                const float HalfSize = FMath::Max(2.0f, ArrowScale * 5.0f);
                NewLines.Add({Position - FVector(HalfSize, 0, 0), Position + FVector(HalfSize, 0, 0), RenderColor, Thickness});
                NewLines.Add({Position - FVector(0, HalfSize, 0), Position + FVector(0, HalfSize, 0), RenderColor, Thickness});
            }
            continue;
        }
        ++ArrowCount;
        const FVector Direction = Snapshot.Acceleration / Magnitude;
        const double StrengthRatio = bLengthRepresentsMagnitude
            ? FMath::Clamp(Magnitude / 980.0, 0.05, 10.0) : 1.0;
        const double Length = FMath::Clamp(100.0 * FMath::Max(0.01f, ArrowScale) * StrengthRatio, 2.0, 2000.0);
        const FVector Tip = Position + Direction * Length;
        const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
        const FVector HeadRight = Side.IsNearlyZero()
            ? FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal() : Side;
        const FVector HeadUp = FVector::CrossProduct(Direction, HeadRight).GetSafeNormal();
        const double HeadLength = FMath::Clamp(Length * 0.25, 3.0, 40.0);
        const FVector HeadBase = Tip - Direction * HeadLength;
        const double HeadWidth = HeadLength * 0.35;
        NewLines.Add({Position, Tip, RenderColor, Thickness});
        NewLines.Add({Tip, HeadBase + HeadRight * HeadWidth, RenderColor, Thickness});
        NewLines.Add({Tip, HeadBase + HeadUp * HeadWidth, RenderColor, Thickness});
    }
    LastArrowCount = ArrowCount;
    LastLineCount = NewLines.Num();
    GravityArrows->SetLines(MoveTemp(NewLines));
}
