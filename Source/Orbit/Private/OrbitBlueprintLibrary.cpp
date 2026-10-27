// Copyright Mippithedork 2026, Inc. All Rights Reserved.

#include "OrbitBlueprintLibrary.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitGravityReceiverComponent.h"
#include "OrbitGravityOrientationComponent.h"
#include "OrbitGravitySourceComponent.h"
#include "OrbitGravityFrame.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OrbitCharacterControlsComponent.h"
#include "Math/RotationMatrix.h"

FOrbitGravitySample UOrbitBlueprintLibrary::SampleOrbitGravity(const UObject* WorldContextObject, FVector WorldPosition, EOrbitGravityChannel Channel)
{
    FOrbitGravitySample Empty;
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) { return Empty; }
    const UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Gravity ? Gravity->SampleGravity(WorldPosition, Channel) : Empty;
}

FOrbitGravitySample UOrbitBlueprintLibrary::SampleOrbitGravityForActor(const UObject* WorldContextObject, const AActor* Actor, FVector WorldPosition, EOrbitGravityChannel Channel)
{
    FOrbitGravitySample Empty;
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || !Actor) { return Empty; }
    const UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>();
    return Gravity ? Gravity->SampleGravityForActor(Actor, WorldPosition, Channel) : Empty;
}

FVector UOrbitBlueprintLibrary::ProjectOntoGravityPlane(FVector Vector, FVector Up)
{
    if (Up.IsNearlyZero()) { return Vector; }
    return FVector::VectorPlaneProject(Vector, Up.GetSafeNormal());
}

bool UOrbitBlueprintLibrary::GravityUpFromAcceleration(FVector Acceleration, FVector& Up)
{
    Up = FVector::UpVector;
    if (Acceleration.ContainsNaN() || Acceleration.IsNearlyZero()) { return false; }
    Up = -Acceleration.GetSafeNormal();
    return true;
}


bool UOrbitBlueprintLibrary::GetGravityTangentBasis(FVector Up, FVector PreferredForward, FVector& Forward, FVector& Right)
{
    return FOrbitGravityFrame::MakeTangentBasis(Up, PreferredForward, Forward, Right);
}

bool UOrbitBlueprintLibrary::MakeGravityAlignedRotation(FRotator CurrentRotation, FVector Up, FRotator& AlignedRotation)
{
    FQuat Output;
    const bool bValid = FOrbitGravityFrame::MakeAlignedRotation(Up, CurrentRotation.Quaternion(), Output);
    AlignedRotation = bValid ? Output.Rotator() : CurrentRotation;
    return bValid;
}

bool UOrbitBlueprintLibrary::MakeGravityFrame(FVector WorldOrigin, FVector GravityUp,
    FVector PreferredForward, FTransform& GravityFrame)
{
    GravityFrame = FTransform::Identity;
    if (WorldOrigin.ContainsNaN()) { return false; }
    FVector Forward, Right;
    if (!FOrbitGravityFrame::MakeTangentBasis(GravityUp, PreferredForward, Forward, Right))
    {
        return false;
    }
    const FQuat Rotation = FRotationMatrix::MakeFromXZ(Forward, GravityUp.GetSafeNormal()).ToQuat();
    GravityFrame = FTransform(Rotation, WorldOrigin, FVector::OneVector);
    return !GravityFrame.ContainsNaN();
}

bool UOrbitBlueprintLibrary::MakeGravityFrameForActor(const AActor* Actor,
    FVector WorldPosition, FVector PreferredForward, FTransform& GravityFrame)
{
    GravityFrame = FTransform::Identity;
    const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
    const UOrbitWorldSubsystem* Gravity = World ? World->GetSubsystem<UOrbitWorldSubsystem>() : nullptr;
    if (!Gravity) { return false; }
    const FOrbitGravitySample Sample = Gravity->SampleGravityForActor(
        Actor, WorldPosition, EOrbitGravityChannel::Character);
    return Sample.bHasValidDirection && MakeGravityFrame(WorldPosition, Sample.Up,
        PreferredForward, GravityFrame);
}

FTransform UOrbitBlueprintLibrary::GravityLocalToWorldTransform(
    const FTransform& RelativeTransform, const FTransform& GravityFrame)
{
    return RelativeTransform * GravityFrame;
}

FTransform UOrbitBlueprintLibrary::GravityWorldToLocalTransform(
    const FTransform& WorldTransform, const FTransform& GravityFrame)
{
    return WorldTransform.GetRelativeTransform(GravityFrame);
}

void UOrbitBlueprintLibrary::ApplyOrbitGravityRelativeMove(ACharacter* Character, FVector2D MoveAxis)
{
    if (IsValid(Character))
    {
        if (UOrbitCharacterControlsComponent* Controls = Character->FindComponentByClass<UOrbitCharacterControlsComponent>())
        {
            Controls->ApplyGravityRelativeMove(MoveAxis);
        }
    }
}

void UOrbitBlueprintLibrary::ApplyOrbitGravityRelativeLook(ACharacter* Character, FVector2D LookAxis)
{
    if (IsValid(Character))
    {
        if (UOrbitCharacterControlsComponent* Controls = Character->FindComponentByClass<UOrbitCharacterControlsComponent>())
        {
            Controls->ApplyGravityRelativeLook(LookAxis);
        }
    }
}


FOrbitGravityRelativeLocomotion UOrbitBlueprintLibrary::GetGravityRelativeLocomotion(
    const ACharacter* Character)
{
    FOrbitGravityRelativeLocomotion Out;
    if (!IsValid(Character)) { return Out; }
    const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
    if (!Move) { return Out; }

    const FVector GravityDown = Move->GetGravityDirection();
    const FVector Up = GravityDown.IsNearlyZero() ? Character->GetActorUpVector() : -GravityDown.GetSafeNormal();
    const FVector Velocity = Move->Velocity;
    double Speed, ForwardSpeed, RightSpeed, Direction;
    if (!FOrbitGravityFrame::CalculateGravityRelativeLocomotion(Velocity, Up,
        Character->GetActorForwardVector(), Speed, ForwardSpeed, RightSpeed, Direction))
    {
        return Out;
    }
    Out.bValid = true;
    Out.GravityUp = Up;
    Out.TangentVelocity = FVector::VectorPlaneProject(Velocity, Up);
    Out.GroundSpeed = static_cast<float>(Speed);
    Out.ForwardSpeed = static_cast<float>(ForwardSpeed);
    Out.RightSpeed = static_cast<float>(RightSpeed);
    Out.MovementDirectionDegrees = static_cast<float>(Direction);
    return Out;
}


bool UOrbitBlueprintLibrary::GetOrbitGravityUpForActor(const AActor* Actor, FVector WorldPosition,
    const UObject* SpecificSource, FVector& Up)
{
    Up = FVector::UpVector;
    if (!IsValid(Actor) || !Actor->GetWorld()) { return false; }
    const UOrbitWorldSubsystem* Subsystem = Actor->GetWorld()->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Subsystem) { return false; }
    FOrbitGravitySample Sample;
    if (IsValid(SpecificSource))
    {
        uint8 Mask = 1u;
        if (const UOrbitGravityReceiverComponent* Receiver =
            Actor->FindComponentByClass<UOrbitGravityReceiverComponent>())
        {
            Mask = Receiver->GravityChannels.ToMask();
        }
        Sample = Subsystem->SampleIndividualSource(SpecificSource, WorldPosition,
            EOrbitGravityChannel::General, Mask);
    }
    else
    {
        Sample = Subsystem->SampleGravityForActor(Actor, WorldPosition, EOrbitGravityChannel::General);
    }
    if (!Sample.bHasInfluence || !Sample.bHasValidDirection) { return false; }
    Up = Sample.Up;
    return true;
}

static FVector OrbitSignedAxis(EOrbitLocalAxis Axis)
{
    switch (Axis)
    {
    case EOrbitLocalAxis::PositiveX: return FVector::ForwardVector;
    case EOrbitLocalAxis::NegativeX: return -FVector::ForwardVector;
    case EOrbitLocalAxis::PositiveY: return FVector::RightVector;
    case EOrbitLocalAxis::NegativeY: return -FVector::RightVector;
    case EOrbitLocalAxis::NegativeZ: return -FVector::UpVector;
    default: return FVector::UpVector;
    }
}

bool UOrbitBlueprintLibrary::MakeOrbitAxisAlignedRotation(FRotator CurrentRotation, FVector GravityUp,
    EOrbitLocalAxis LocalAxis, FRotator& AlignedRotation)
{
    AlignedRotation = CurrentRotation;
    if (GravityUp.IsNearlyZero() || GravityUp.ContainsNaN() || CurrentRotation.ContainsNaN())
    {
        return false;
    }
    const FQuat Current = CurrentRotation.Quaternion();
    const FVector CurrentAxis = Current.RotateVector(OrbitSignedAxis(LocalAxis));
    const FVector Target = GravityUp.GetSafeNormal();
    const FQuat Delta = FQuat::FindBetweenNormals(CurrentAxis.GetSafeNormal(), Target);
    const FQuat Rotation = (Delta * Current).GetNormalized();
    if (Rotation.ContainsNaN()) { return false; }
    AlignedRotation = Rotation.Rotator();
    return true;
}

bool UOrbitBlueprintLibrary::OrientActorToOrbitGravity(AActor* Actor, EOrbitLocalAxis LocalAxis,
    const UObject* SpecificSource)
{
    if (!IsValid(Actor)) { return false; }
    FVector Up;
    FRotator Rotation;
    if (!GetOrbitGravityUpForActor(Actor, Actor->GetActorLocation(), SpecificSource, Up) ||
        !MakeOrbitAxisAlignedRotation(Actor->GetActorRotation(), Up, LocalAxis, Rotation))
    {
        return false;
    }
    Actor->SetActorRotation(Rotation, ETeleportType::None);
    return true;
}
