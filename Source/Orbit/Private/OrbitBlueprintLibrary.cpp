#include "OrbitBlueprintLibrary.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitGravityFrame.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
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
