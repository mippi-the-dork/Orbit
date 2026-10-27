// Copyright Mippithedork 2026, Inc. All Rights Reserved.

#include "OrbitGravityOrientationComponent.h"
#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitBlueprintLibrary.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UOrbitGravityOrientationComponent::UOrbitGravityOrientationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

bool UOrbitGravityOrientationComponent::OrientNow()
{
    return ApplyOrientation(0.0f, true);
}

void UOrbitGravityOrientationComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bEnabled) { ApplyOrientation(DeltaTime, false); }
}

bool UOrbitGravityOrientationComponent::ApplyOrientation(float DeltaTime, bool bImmediate)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World) { return false; }
    FVector Up;
    const UObject* Source = SpecificSourceComponent ? static_cast<const UObject*>(SpecificSourceComponent.Get()) :
        static_cast<const UObject*>(SpecificSourceActor.Get());
    bool bValid = UOrbitBlueprintLibrary::GetOrbitGravityUpForActor(Owner, Owner->GetActorLocation(),
        Source, Up);
    bLastSampleValid = bValid;
    if (!bValid)
    {
        if (bHoldOrientationWithoutGravity) { return false; }
        Up = FVector::UpVector;
    }
    LastGravityUp = Up;
    FRotator TargetRotator;
    if (!UOrbitBlueprintLibrary::MakeOrbitAxisAlignedRotation(Owner->GetActorRotation(), Up, LocalAxis, TargetRotator))
    {
        return false;
    }
    FQuat Target = TargetRotator.Quaternion();
    const FQuat Current = Owner->GetActorQuat();
    const double Alpha = bImmediate || ResponseRate <= 0.0f ? 1.0 :
        1.0 - FMath::Exp(-ResponseRate * FMath::Max(0.0f, DeltaTime));
    Target = FQuat::Slerp(Current, Target, Alpha).GetNormalized();
    if (Target.ContainsNaN()) { return false; }
    Owner->SetActorRotation(Target, ETeleportType::None);
    return true;
}
