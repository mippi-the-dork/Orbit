#include "OrbitGravitySourceComponent.h"
#include "OrbitWorldSubsystem.h"
#include "Components/SplineComponent.h"
#include "OrbitSplineSampling.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UOrbitGravitySourceComponent::UOrbitGravitySourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetMobility(EComponentMobility::Movable);
}

void UOrbitGravitySourceComponent::EnsureBoxCornersMigrated()
{
    if (bBoxCornersInitialized) { return; }
    const FVector Extents = BoxHalfExtents.GetAbs();
    BoxCornerA.SetTranslation(-Extents);
    BoxCornerB.SetTranslation(Extents);
    bBoxCornersInitialized = true;
}

void UOrbitGravitySourceComponent::OnComponentCreated()
{
    Super::OnComponentCreated();
    // Existing deserialized assets do not receive this creation callback, so
    // they keep Every Frame and their original procedural-spline behavior.
    if (!IsTemplate() && GetArchetype() == GetClass()->GetDefaultObject())
    {
        SplineUpdateMode = EOrbitSplineUpdateMode::Automatic;
    }
}

void UOrbitGravitySourceComponent::RefreshSplineGravity()
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

void UOrbitGravitySourceComponent::PostLoad()
{
    Super::PostLoad();
    EnsureBoxCornersMigrated();
}

#if WITH_EDITOR
void UOrbitGravitySourceComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    // Shape edits and the direction fallback belong to the same UE transaction.
    // The previous direction is restored when Undo restores the previous shape.
    Direction = OrbitDirectionForShape(Shape, Direction);

    if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UOrbitGravitySourceComponent, BoxCornerA) ||
        Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UOrbitGravitySourceComponent, BoxCornerB))
    {
        bBoxCornersInitialized = true;
    }
}
#endif

void UOrbitGravitySourceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UWorld* World = GetWorld())
    {
        if (UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Gravity->RegisterSourceComponent(this);
        }
    }
}

void UOrbitGravitySourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Gravity->UnregisterSourceComponent(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void UOrbitGravitySourceComponent::SetGravityStrength(double NewStrength)
{
    if (!FMath::IsFinite(NewStrength)) { return; }
    Strength = FMath::Max(0.0, NewStrength);
    if (UWorld* World = GetWorld())
    {
        if (UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>())
        {
            Gravity->RefreshActiveSources();
        }
    }
}

void UOrbitGravitySourceComponent::BuildDefinition(FOrbitFieldDefinition& Field, bool bReuseSplineArrays) const
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
    Field.Center = GetComponentLocation();
    Field.Axis = GetUpVector();
    Field.DirectionVector = -GetUpVector();
    Field.Rotation = GetComponentQuat();
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
    if (Shape == EOrbitGravityShape::Spline)
    {
        const USplineComponent* Spline = GravitySpline;
        if (!Spline && GetOwner())
        {
            Spline = GetOwner()->FindComponentByClass<USplineComponent>();
        }
        if (Spline)
        {
            SplineGeometryCache.Get(Spline, SplineSampleSpacing, SplineMaxSegments, Field.Curve, Field.CurveUp, Field.CurveBlocks, bReuseSplineArrays,
            SplineUpdateMode == EOrbitSplineUpdateMode::EveryFrame ? 0.0 :
                (SplineUpdateMode == EOrbitSplineUpdateMode::Automatic ? 0.1 : FMath::Max(0.05, SplineCheckInterval)),
            SplineUpdateMode == EOrbitSplineUpdateMode::Manual);
        }
    }
}

FOrbitGravitySample UOrbitGravitySourceComponent::SampleSourceComponent(FVector WorldLocation) const
{
    if (!bEnabled) { return FOrbitGravitySample(); }
    FOrbitFieldDefinition Definition;
    BuildDefinition(Definition);
    TArray<FOrbitFieldDefinition> Fields;
    Fields.Add(MoveTemp(Definition));
    return FOrbitFieldEvaluator::Combine(Fields, WorldLocation, EOrbitGravityChannel::General, GravityChannels.ToMask());
}
