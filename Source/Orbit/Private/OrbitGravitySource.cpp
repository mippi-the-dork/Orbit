#include "OrbitGravitySource.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitBoundarySelectionComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "OrbitSplineSampling.h"
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
    BoundarySelection = CreateEditorOnlyDefaultSubobject<UOrbitBoundarySelectionComponent>(TEXT("OrbitBoundarySelection"));
    if (BoundarySelection)
    {
        BoundarySelection->SetupAttachment(SceneRoot);
    }
    OrbitBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("OrbitBillboard"));
    OrbitBillboard->SetupAttachment(SceneRoot);
    OrbitBillboard->SetHiddenInGame(true);
    OrbitBillboard->bIsScreenSizeScaled = true;
#endif
    bReplicates = false; // Network replication remains outside the current implementation.
}

void AOrbitGravitySource::EnsureBoxCornersMigrated()
{
    if (bBoxCornersInitialized) { return; }
    const FVector Extents = BoxHalfExtents.GetAbs();
    BoxCornerA.SetTranslation(-Extents);
    BoxCornerB.SetTranslation(Extents);
    bBoxCornersInitialized = true;
}

void AOrbitGravitySource::PostActorCreated()
{
    Super::PostActorCreated();
    // Existing deserialized assets do not receive this creation callback, so
    // they keep Every Frame and their original procedural-spline behavior.
    if (!IsTemplate() && GetArchetype() == GetClass()->GetDefaultObject())
    {
        SplineUpdateMode = EOrbitSplineUpdateMode::Automatic;
    }
}

void AOrbitGravitySource::RefreshSplineGravity()
{
    SplineGeometryCache.Invalidate();
    if (UWorld* World = GetWorld())
    {
        if (World->IsGameWorld())
        {
            if (UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>())
            {
                Subsystem->RefreshActiveSources();
            }
        }
    }
}

void AOrbitGravitySource::PostLoad()
{
    Super::PostLoad();
    EnsureBoxCornersMigrated();
}

#if WITH_EDITOR
void AOrbitGravitySource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    // Shape edits and the direction fallback belong to the same UE transaction.
    // The previous direction is restored when Undo restores the previous shape.
    Direction = OrbitDirectionForShape(Shape, Direction);

    if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AOrbitGravitySource, BoxCornerA) ||
        PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AOrbitGravitySource, BoxCornerB) ||
        (PropertyChangedEvent.MemberProperty &&
         (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AOrbitGravitySource, BoxCornerA) ||
          PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AOrbitGravitySource, BoxCornerB))))
    {
        bBoxCornersInitialized = true;
    }
    bStreamlinesCached = false;
    UpdateSelectableBounds();
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
        if (EditedProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AOrbitGravitySource, Shape))
        {
            RuntimeActor->Direction = Direction;
        }
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
    UpdateSelectableBounds();
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

void AOrbitGravitySource::BuildDefinition(FOrbitFieldDefinition& Field, bool bReuseSplineArrays) const
{
    // The world subsystem moves forward the previous definition of this exact source.
    // Keep only its spline buffers; overwrite all scalar field settings below.
    bReuseSplineArrays = bReuseSplineArrays && Field.Shape == EOrbitGravityShape::Spline;
    if (!bReuseSplineArrays)
    {
        Field = FOrbitFieldDefinition();
    }
    if (Shape != EOrbitGravityShape::Spline)
    {
        Field.Curve.Reset(); Field.CurveUp.Reset(); Field.CurveBlocks.Reset();
    }
    Field.Shape = Shape;
    Field.Direction = OrbitDirectionForShape(Shape, Direction);
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
    Field.bUseBoxCorners = bBoxCornersInitialized;
    Field.BoxLocalMin = BoxCornerA.GetTranslation().ComponentMin(BoxCornerB.GetTranslation());
    Field.BoxLocalMax = BoxCornerA.GetTranslation().ComponentMax(BoxCornerB.GetTranslation());
    if (!Field.bUseBoxCorners)
    {
        Field.BoxLocalMin = -Field.BoxExtents;
        Field.BoxLocalMax = Field.BoxExtents;
    }
    Field.VolumeDirectionMode = VolumeDirectionMode;
    Field.VolumeShape = VolumeShape;
    Field.VolumeRadius = FMath::Max(VolumeRadius, 1.0);
    Field.VolumeHalfHeight = FMath::Max(VolumeHalfHeight, 1.0);
    if (Shape == EOrbitGravityShape::Spline && GravitySpline)
    {
        SplineGeometryCache.Get(GravitySpline, SplineSampleSpacing, SplineMaxSegments, Field.Curve, Field.CurveUp, Field.CurveBlocks, bReuseSplineArrays,
            SplineUpdateMode == EOrbitSplineUpdateMode::EveryFrame ? 0.0 :
                (SplineUpdateMode == EOrbitSplineUpdateMode::Automatic ? 0.1 : FMath::Max(0.05, SplineCheckInterval)),
            SplineUpdateMode == EOrbitSplineUpdateMode::Manual);
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

#if WITH_EDITOR
void AOrbitGravitySource::UpdateSelectableBounds()
{
#if WITH_EDITORONLY_DATA
    if (!BoundarySelection || !GetWorld() || GetWorld()->WorldType != EWorldType::Editor || IsTemplate())
    {
        return;
    }

    using FLine = UOrbitBoundarySelectionComponent::FLine;
    TArray<FLine> Lines;
    const FVector Origin = GetActorLocation();
    const FQuat Rotation = GetActorQuat();
    const FVector Up = Rotation.RotateVector(FVector::UpVector);
    const FColor Muted(110, 110, 110);
    const FColor BoxColor = bEnabled ? LimitBoxColor.ToFColor(true) : Muted;
    const FColor DistanceColor = bEnabled ? MaxDistanceColor.ToFColor(true) : Muted;
    const FColor VolumeColor = bEnabled ? FColor(65, 205, 145) : Muted;

    auto AddLine = [&Lines](const FVector& A, const FVector& B, FColor Color, float Width)
    {
        if (A.ContainsNaN() || B.ContainsNaN()) { return; }
        Lines.Add({A, B, Color, FMath::Max(0.1f, Width)});
    };
    auto AddBox = [&](const FVector& Center, const FVector& Extents, FColor Color, float Width)
    {
        const FVector E = Extents.GetAbs();
        FVector V[8];
        for (int32 I = 0; I < 8; ++I)
        {
            const FVector Local((I & 1) ? E.X : -E.X, (I & 2) ? E.Y : -E.Y, (I & 4) ? E.Z : -E.Z);
            V[I] = Center + Rotation.RotateVector(Local);
        }
        for (int32 I = 0; I < 8; ++I)
        {
            for (int32 Bit = 1; Bit <= 4; Bit <<= 1)
            {
                if (!(I & Bit)) { AddLine(V[I], V[I | Bit], Color, Width); }
            }
        }
    };
    auto AddRing = [&](const FVector& Center, const FVector& AxisA, const FVector& AxisB,
                       double Radius, FColor Color, float Width, int32 Sides = 32)
    {
        const double SafeRadius = FMath::Max(1.0, Radius);
        for (int32 I = 0; I < Sides; ++I)
        {
            const double A0 = 2.0 * PI * I / Sides;
            const double A1 = 2.0 * PI * (I + 1) / Sides;
            const FVector P0 = Center + SafeRadius * (AxisA * FMath::Cos(A0) + AxisB * FMath::Sin(A0));
            const FVector P1 = Center + SafeRadius * (AxisA * FMath::Cos(A1) + AxisB * FMath::Sin(A1));
            AddLine(P0, P1, Color, Width);
        }
    };
    auto AddSphere = [&](const FVector& Center, double Radius, FColor Color, float Width)
    {
        const FVector X = Rotation.RotateVector(FVector::ForwardVector);
        const FVector Y = Rotation.RotateVector(FVector::RightVector);
        AddRing(Center, X, Y, Radius, Color, Width);
        AddRing(Center, X, Up, Radius, Color, Width);
        AddRing(Center, Y, Up, Radius, Color, Width);
    };
    auto AddCylinder = [&](const FVector& Center, double Radius, double HalfHeight, FColor Color, float Width)
    {
        const FVector X = Rotation.RotateVector(FVector::ForwardVector);
        const FVector Y = Rotation.RotateVector(FVector::RightVector);
        const FVector Bottom = Center - Up * HalfHeight;
        const FVector Top = Center + Up * HalfHeight;
        AddRing(Bottom, X, Y, Radius, Color, Width);
        AddRing(Top, X, Y, Radius, Color, Width);
        for (int32 I = 0; I < 8; ++I)
        {
            const double Angle = 2.0 * PI * I / 8.0;
            const FVector Radial = (X * FMath::Cos(Angle) + Y * FMath::Sin(Angle)) * Radius;
            AddLine(Bottom + Radial, Top + Radial, Color, Width);
        }
    };
    auto AddCapsule = [&](const FVector& Center, double Radius, double HalfHeight, FColor Color, float Width)
    {
        const double CylinderHalf = FMath::Max(0.0, HalfHeight - Radius);
        AddCylinder(Center, Radius, CylinderHalf, Color, Width);
        const FVector X = Rotation.RotateVector(FVector::ForwardVector);
        const FVector Y = Rotation.RotateVector(FVector::RightVector);
        for (int32 Side = 0; Side < 8; ++Side)
        {
            const double Angle = 2.0 * PI * Side / 8.0;
            const FVector Radial = X * FMath::Cos(Angle) + Y * FMath::Sin(Angle);
            for (int32 Sign = -1; Sign <= 1; Sign += 2)
            {
                const FVector CapCenter = Center + Up * CylinderHalf * Sign;
                for (int32 Segment = 0; Segment < 8; ++Segment)
                {
                    const double T0 = PI * 0.5 * Segment / 8.0;
                    const double T1 = PI * 0.5 * (Segment + 1) / 8.0;
                    AddLine(CapCenter + Radius * (Radial * FMath::Cos(T0) + Up * Sign * FMath::Sin(T0)),
                            CapCenter + Radius * (Radial * FMath::Cos(T1) + Up * Sign * FMath::Sin(T1)), Color, Width);
                }
            }
        }
    };
    const FVector MinCorner = bBoxCornersInitialized
        ? BoxCornerA.GetTranslation().ComponentMin(BoxCornerB.GetTranslation()) : -BoxHalfExtents.GetAbs();
    const FVector MaxCorner = bBoxCornersInitialized
        ? BoxCornerA.GetTranslation().ComponentMax(BoxCornerB.GetTranslation()) : BoxHalfExtents.GetAbs();
    const FVector BoxCenter = Origin + Rotation.RotateVector((MinCorner + MaxCorner) * 0.5);
    const FVector BoxExtent = (MaxCorner - MinCorner) * 0.5;

    if (bShowVolumeBounds)
    {
        if (Shape == EOrbitGravityShape::Volume)
        {
            const double Radius = FMath::Max(1.0, VolumeRadius);
            const double HalfHeight = FMath::Max(1.0, VolumeHalfHeight);
            switch (VolumeShape)
            {
            case EOrbitVolumeShape::Box:
                AddBox(BoxCenter, BoxExtent, bLimitToBox ? BoxColor : VolumeColor,
                       bLimitToBox ? LimitBoxThickness : 1.6f);
                break;
            case EOrbitVolumeShape::Sphere: AddSphere(Origin, Radius, VolumeColor, 1.6f); break;
            case EOrbitVolumeShape::Cylinder: AddCylinder(Origin, Radius, HalfHeight, VolumeColor, 1.6f); break;
            case EOrbitVolumeShape::Capsule: AddCapsule(Origin, Radius, FMath::Max(Radius, HalfHeight), VolumeColor, 1.6f); break;
            }
        }
        if (bLimitToBox && (Shape != EOrbitGravityShape::Volume || VolumeShape != EOrbitVolumeShape::Box))
        {
            AddBox(BoxCenter, BoxExtent, BoxColor, LimitBoxThickness);
        }
    }
    if (bShowMaxDistanceBoundary && MaxDistance > 0.0)
    {
        const double Radius = MaxDistance;
        switch (Shape)
        {
        case EOrbitGravityShape::Point:
        case EOrbitGravityShape::Directional:
            AddSphere(Origin, Radius, DistanceColor, MaxDistanceThickness);
            break;
        case EOrbitGravityShape::Volume:
        {
            FOrbitFieldDefinition Field;
            BuildDefinition(Field);
            const FOrbitEffectiveVolumeBounds Bounds = FOrbitFieldEvaluator::GetEffectiveVolumeBounds(Field);
            switch (VolumeShape)
            {
            case EOrbitVolumeShape::Box:
                // Effective half-extents may be capped by MaxDistance, but the authored center is retained.
                AddBox(BoxCenter, Bounds.BoxHalfExtents, DistanceColor, MaxDistanceThickness);
                break;
            case EOrbitVolumeShape::Sphere: AddSphere(Origin, Bounds.Radius, DistanceColor, MaxDistanceThickness); break;
            case EOrbitVolumeShape::Cylinder: AddCylinder(Origin, Bounds.Radius, Bounds.HalfHeight, DistanceColor, MaxDistanceThickness); break;
            case EOrbitVolumeShape::Capsule: AddCapsule(Origin, Bounds.Radius, Bounds.HalfHeight, DistanceColor, MaxDistanceThickness); break;
            }
            break;
        }
        case EOrbitGravityShape::Axis:
            AddCylinder(Origin, Radius, FMath::Max(PreviewExtent, 50.0), DistanceColor, MaxDistanceThickness);
            break;
        case EOrbitGravityShape::Spline:
            if (GravitySpline && GravitySpline->GetNumberOfSplinePoints() > 1)
            {
                // Render the same world-space polyline used by gravity, not 48 widely
                // spaced rings that cut across tight bends or uniform-scale hierarchies.
                TArray<FVector> Centers, CurveUps;
                TArray<FOrbitSplineSegmentBlock> UnusedBlocks;
                SplineGeometryCache.Get(GravitySpline, SplineSampleSpacing, SplineMaxSegments, Centers, CurveUps, UnusedBlocks);
                const int32 Sides = Centers.Num() > 4096 ? 4 : 8;
                const int32 RingStride = Centers.Num() > 4096 ? 32 : 8;
                TArray<FVector> Previous;
                for (int32 I = 0; I < Centers.Num(); ++I)
                {
                    const FVector Tangent = (Centers[FMath::Min(I + 1, Centers.Num() - 1)] -
                        Centers[FMath::Max(I - 1, 0)]).GetSafeNormal();
                    FVector Right = FVector::CrossProduct(Tangent, CurveUps[I]).GetSafeNormal();
                    if (Right.IsNearlyZero()) { Right = FVector::CrossProduct(Tangent, FVector::RightVector).GetSafeNormal(); }
                    if (Right.IsNearlyZero()) { continue; }
                    const FVector RingUp = FVector::CrossProduct(Right, Tangent).GetSafeNormal();
                    TArray<FVector> Current;
                    Current.Reserve(Sides);
                    for (int32 Side = 0; Side < Sides; ++Side)
                    {
                        const double Angle = 2.0 * PI * Side / Sides;
                        Current.Add(Centers[I] + (RingUp * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * Radius);
                    }
                    for (int32 Side = 0; Side < Sides; ++Side)
                    {
                        // Keep every longitudinal wire for bend fidelity; draw transverse
                        // rings sparsely to avoid multiplying large splines by every side.
                        if (I == 0 || I + 1 == Centers.Num() || I % RingStride == 0)
                        {
                            AddLine(Current[Side], Current[(Side + 1) % Sides], DistanceColor, MaxDistanceThickness);
                        }
                        if (Previous.Num() == Sides)
                        {
                            AddLine(Previous[Side], Current[Side], DistanceColor, MaxDistanceThickness);
                        }
                    }
                    Previous = MoveTemp(Current);
                }
            }
            break;
        }
    }
    BoundarySelection->SetPreviewLines(MoveTemp(Lines));
#endif // WITH_EDITORONLY_DATA
}
#endif // WITH_EDITOR

void AOrbitGravitySource::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if WITH_EDITOR
    UpdateSelectableBounds();
#endif
    if (!GetWorld()) { return; }
#if WITH_EDITOR
    // In PIE/SIE, skip *all* debug drawing (including expensive spline tubes)
    // unless the designer explicitly opts this source in. Do not affect the
    // editor-world selectable boundaries or the gravity field snapshots.
    if (GetWorld()->IsGameWorld() && !bShowDebugInGame) { return; }
#endif
    if (!bShowGravityPreview && !bShowStreamlines && !bShowVolumeBounds && !bShowMaxDistanceBoundary) { return; }
#if WITH_EDITOR

    const FVector Origin = GetActorLocation();
    const FQuat Rotation = GetActorQuat();
    const FVector Up = GetActorUpVector();
    const UWorld* World = GetWorld();
    constexpr float DebugLifetime = 0.0f;
    const FVector LocalBoxMin = bBoxCornersInitialized
        ? BoxCornerA.GetTranslation().ComponentMin(BoxCornerB.GetTranslation()) : -BoxHalfExtents.GetAbs();
    const FVector LocalBoxMax = bBoxCornersInitialized
        ? BoxCornerA.GetTranslation().ComponentMax(BoxCornerB.GetTranslation()) : BoxHalfExtents.GetAbs();
    const FVector LocalBoxCenter = (LocalBoxMin + LocalBoxMax) * 0.5;
    const FVector LocalBoxExtent = (LocalBoxMax - LocalBoxMin) * 0.5;
    const FVector BoxWorldCenter = Origin + Rotation.RotateVector(LocalBoxCenter);
    const FColor BoxLimitDisplayColor = bEnabled ? LimitBoxColor.ToFColor(true) : FColor(110, 110, 110);

    // Show influence bounds independently from the vector preview. Use the same
    // dimensions and source rotation as the actual field evaluator.
    const bool bSelectableEditorBounds = BoundarySelection && World->WorldType == EWorldType::Editor;
    if (bShowVolumeBounds && !bSelectableEditorBounds)
    {
        if (Shape == EOrbitGravityShape::Volume)
        {
            const FColor Color = bEnabled ? FColor(65, 205, 145) : FColor(110, 110, 110);
            const double Radius = FMath::Max(VolumeRadius, 1.0);
            const double HalfHeight = FMath::Max(VolumeHalfHeight, 1.0);
            switch (VolumeShape)
            {
            case EOrbitVolumeShape::Box:
                DrawDebugBox(World, BoxWorldCenter, LocalBoxExtent, Rotation,
                    bLimitToBox ? BoxLimitDisplayColor : Color, false, DebugLifetime, 0,
                    bLimitToBox ? LimitBoxThickness : 1.6f);
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
            DrawDebugBox(World, BoxWorldCenter, LocalBoxExtent, Rotation, BoxLimitDisplayColor, false, DebugLifetime, 0, LimitBoxThickness);
        }
    }

    // Show the finite influence boundary even with preview arrows disabled.
    // Volumes use their selected local shape capped by MaxDistance, not an
    // unrelated spherical intersection. Other fields retain radial bounds.
    if (bShowMaxDistanceBoundary && MaxDistance > 0.0 && !bSelectableEditorBounds)
    {
        const FColor BoundColor = bEnabled ? MaxDistanceColor.ToFColor(true) : FColor(110, 110, 110);
        const double Radius = MaxDistance;
        switch (Shape)
        {
        case EOrbitGravityShape::Point:
        case EOrbitGravityShape::Directional:
            DrawDebugSphere(World, Origin, Radius, 32, BoundColor, false, DebugLifetime, 0, MaxDistanceThickness);
            break;
        case EOrbitGravityShape::Volume:
        {
            FOrbitFieldDefinition Definition;
            BuildDefinition(Definition);
            const FOrbitEffectiveVolumeBounds Bounds = FOrbitFieldEvaluator::GetEffectiveVolumeBounds(Definition);
            switch (VolumeShape)
            {
            case EOrbitVolumeShape::Box:
                DrawDebugBox(World, BoxWorldCenter, Bounds.BoxHalfExtents, Rotation, BoundColor, false, DebugLifetime, 0, MaxDistanceThickness);
                break;
            case EOrbitVolumeShape::Sphere:
                DrawDebugSphere(World, Origin, Bounds.Radius, 32, BoundColor, false, DebugLifetime, 0, MaxDistanceThickness);
                break;
            case EOrbitVolumeShape::Cylinder:
                DrawDebugCylinder(World, Origin - Up * Bounds.HalfHeight, Origin + Up * Bounds.HalfHeight,
                    Bounds.Radius, 32, BoundColor, false, DebugLifetime, 0, MaxDistanceThickness);
                break;
            case EOrbitVolumeShape::Capsule:
                DrawDebugCapsule(World, Origin, Bounds.HalfHeight, Bounds.Radius, Rotation,
                    BoundColor, false, DebugLifetime, 0, MaxDistanceThickness);
                break;
            }
            break;
        }
        case EOrbitGravityShape::Axis:
        {
            // The actual field is infinite along the axis. Draw only a finite
            // portion near the actor, with visible radial boundaries.
            const double HalfLength = FMath::Max(PreviewExtent, 50.0);
            DrawDebugCylinder(World, Origin - Up * HalfLength, Origin + Up * HalfLength,
                Radius, 32, BoundColor, false, DebugLifetime, 0, MaxDistanceThickness);
            break;
        }
        case EOrbitGravityShape::Spline:
            if (GravitySpline && GravitySpline->GetNumberOfSplinePoints() > 1)
            {
                TArray<FVector> Centers, CurveUps;
                TArray<FOrbitSplineSegmentBlock> UnusedBlocks;
                SplineGeometryCache.Get(GravitySpline, SplineSampleSpacing, SplineMaxSegments, Centers, CurveUps, UnusedBlocks);
                const int32 Sides = Centers.Num() > 4096 ? 4 : 8;
                const int32 RingStride = Centers.Num() > 4096 ? 32 : 8;
                TArray<FVector> Previous;
                for (int32 I = 0; I < Centers.Num(); ++I)
                {
                    const FVector Tangent = (Centers[FMath::Min(I + 1, Centers.Num() - 1)] -
                        Centers[FMath::Max(I - 1, 0)]).GetSafeNormal();
                    FVector Right = FVector::CrossProduct(Tangent, CurveUps[I]).GetSafeNormal();
                    if (Right.IsNearlyZero()) { Right = FVector::CrossProduct(Tangent, FVector::RightVector).GetSafeNormal(); }
                    if (Right.IsNearlyZero()) { continue; }
                    const FVector RingUp = FVector::CrossProduct(Right, Tangent).GetSafeNormal();
                    TArray<FVector> Current;
                    Current.Reserve(Sides);
                    for (int32 Side = 0; Side < Sides; ++Side)
                    {
                        const double Angle = 2.0 * PI * Side / Sides;
                        Current.Add(Centers[I] + (RingUp * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * Radius);
                    }
                    for (int32 Side = 0; Side < Sides; ++Side)
                    {
                        if (I == 0 || I + 1 == Centers.Num() || I % RingStride == 0)
                        {
                            DrawDebugLine(World, Current[Side], Current[(Side + 1) % Sides], BoundColor, false, DebugLifetime, 0, MaxDistanceThickness);
                        }
                        if (Previous.Num() == Sides)
                        {
                            DrawDebugLine(World, Previous[Side], Current[Side], BoundColor, false, DebugLifetime, 0, MaxDistanceThickness);
                        }
                    }
                    Previous = MoveTemp(Current);
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
                    const FOrbitEffectiveVolumeBounds Bounds =
                        FOrbitFieldEvaluator::GetEffectiveVolumeBounds(StreamField);
                    switch (VolumeShape)
                    {
                    case EOrbitVolumeShape::Box: HalfExtents = Bounds.BoxHalfExtents; break;
                    case EOrbitVolumeShape::Sphere: HalfExtents = FVector(Bounds.Radius); break;
                    case EOrbitVolumeShape::Cylinder:
                        HalfExtents = FVector(Bounds.Radius, Bounds.Radius, Bounds.HalfHeight); break;
                    case EOrbitVolumeShape::Capsule:
                        HalfExtents = FVector(Bounds.Radius, Bounds.Radius, Bounds.HalfHeight); break;
                    }
                    // Auto Fit off limits editor-only sampling, not gameplay.
                    if (!bFitPreviewToMaxDistance || MaxDistance <= 0.0)
                    {
                        HalfExtents.X = FMath::Min(HalfExtents.X, Extent);
                        HalfExtents.Y = FMath::Min(HalfExtents.Y, Extent);
                        HalfExtents.Z = FMath::Min(HalfExtents.Z, Extent);
                    }
                }
                if (bLimitToBox)
                {
                    HalfExtents.X = FMath::Min(HalfExtents.X, FMath::Max(FMath::Abs(LocalBoxMin.X), FMath::Abs(LocalBoxMax.X)));
                    HalfExtents.Y = FMath::Min(HalfExtents.Y, FMath::Max(FMath::Abs(LocalBoxMin.Y), FMath::Abs(LocalBoxMax.Y)));
                    HalfExtents.Z = FMath::Min(HalfExtents.Z, FMath::Max(FMath::Abs(LocalBoxMin.Z), FMath::Abs(LocalBoxMax.Z)));
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
                            Candidates.Add((Shape == EOrbitGravityShape::Volume && VolumeShape == EOrbitVolumeShape::Box)
                                ? BoxWorldCenter + Rotation.RotateVector(LocalPoint)
                                : Origin + Rotation.RotateVector(LocalPoint));
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
        const FColor CurveColor = StreamlineColor.ToFColor(true);
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
                    FMath::Clamp(MarkerLength * 0.3f, 6.0f, 20.0f), StreamlineColor.ToFColor(true),
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
            const FOrbitEffectiveVolumeBounds Bounds = FOrbitFieldEvaluator::GetEffectiveVolumeBounds(Field);
            switch (VolumeShape)
            {
            case EOrbitVolumeShape::Box: LocalExtents = Bounds.BoxHalfExtents; break;
            case EOrbitVolumeShape::Sphere: LocalExtents = FVector(Bounds.Radius); break;
            case EOrbitVolumeShape::Cylinder:
                LocalExtents = FVector(Bounds.Radius, Bounds.Radius, Bounds.HalfHeight); break;
            case EOrbitVolumeShape::Capsule:
                LocalExtents = FVector(Bounds.Radius, Bounds.Radius, Bounds.HalfHeight); break;
            }
            // Seed only the actual shape-bounded region. Preview Extent is an
            // editor-only cap when Auto Fit is off or MaxDistance is unlimited.
            if (!bFitPreviewToMaxDistance || MaxDistance <= 0.0)
            {
                LocalExtents.X = FMath::Min(LocalExtents.X, Extent);
                LocalExtents.Y = FMath::Min(LocalExtents.Y, Extent);
                LocalExtents.Z = FMath::Min(LocalExtents.Z, Extent);
            }
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
            const FVector MaskExtents = LocalBoxMin.GetAbs().ComponentMax(LocalBoxMax.GetAbs());
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
                FMath::Clamp(ActualLength * 0.2f, 8.0f, 36.0f), PreviewArrowColor.ToFColor(true), false, DebugLifetime, 0, FMath::Max(0.1f, PreviewArrowThickness));
        }
    }
#endif // WITH_EDITOR: visualization is a development tool, not shipped gameplay.
}
