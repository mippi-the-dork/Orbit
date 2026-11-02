// Copyright Mippithedork 2026, Inc. All Rights Reserved.

#include "OrbitGravitySourceComponent.h"
#include "OrbitWorldSubsystem.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UOrbitGravitySourceComponent::UOrbitGravitySourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetMobility(EComponentMobility::Movable);
}

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

void UOrbitGravitySourceComponent::BuildDefinition(FOrbitFieldDefinition& Field) const
{
    Field = FOrbitFieldDefinition();
    Field.Shape = Shape;
    Field.Direction = Direction;
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
            const double Length = Spline->GetSplineLength();
            const int32 Samples = FMath::Clamp(FMath::CeilToInt(Length / FMath::Max(SplineSampleSpacing, 25.0)), 1, 2048);
            Field.Curve.Reserve(Samples + 1);
            Field.CurveUp.Reserve(Samples + 1);
            for (int32 Index = 0; Index <= Samples; ++Index)
            {
                const double Distance = Length * static_cast<double>(Index) / Samples;
                Field.Curve.Add(Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
                Field.CurveUp.Add(Spline->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
            }
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
