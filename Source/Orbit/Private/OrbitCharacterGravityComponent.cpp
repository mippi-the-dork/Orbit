#include "OrbitCharacterGravityComponent.h"

#include "OrbitGravityFrame.h"
#include "OrbitWorldSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrbitCharacter, Log, All);

UOrbitCharacterGravityComponent::UOrbitCharacterGravityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UOrbitCharacterGravityComponent::BeginPlay()
{
    Super::BeginPlay();
    // Apply gravity before CharacterMovement updates walking, falling and floor checks.
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->AddTickPrerequisiteComponent(this);
        }
    }
}

void UOrbitCharacterGravityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        RestoreMovement(Character->GetCharacterMovement());
    }
    Super::EndPlay(EndPlayReason);
}

void UOrbitCharacterGravityComponent::RestoreMovement(UCharacterMovementComponent* Movement)
{
    if (!bControllingMovement) { return; }
    if (IsValid(Movement))
    {
        Movement->SetGravityDirection(SavedGravityDirection);
        Movement->GravityScale = SavedGravityScale;
    }
    bControllingMovement = false;
}

void UOrbitCharacterGravityComponent::AlignCharacterToUp(const FVector& Up, float DeltaTime)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !bAlignActorToGravity || Up.IsNearlyZero()) { return; }

    const FQuat Current = Character->GetActorQuat();
    FQuat Target;
    if (!FOrbitGravityFrame::MakeAlignedRotation(Up, Current, Target)) { return; }

    // Quaternion interpolation avoids Euler wraparound and invalid cross products at planet poles.
    const float Alpha = OrientationResponseRate <= 0.0f ? 1.0f :
        1.0f - FMath::Exp(-OrientationResponseRate * FMath::Max(0.0f, DeltaTime));
    const FQuat NewRotation = FQuat::Slerp(Current, Target, Alpha).GetNormalized();
    if (!NewRotation.ContainsNaN())
    {
        Character->SetActorRotation(NewRotation.Rotator());
        bHasAlignedActor = true;
    }
}

void UOrbitCharacterGravityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UWorld* World = GetWorld();
    if (!Character || !World) { return; }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Movement || !Gravity) { return; }

    const FOrbitGravitySample Sample = Gravity->SampleGravityForActor(
        Character, Character->GetActorLocation(), EOrbitGravityChannel::Character);
    bInsideOrbitField = Sample.bHasInfluence;
    LastFieldAcceleration = bInsideOrbitField ? Sample.Acceleration : FVector::ZeroVector;

    if (!Sample.bHasInfluence)
    {
        if (bUseDefaultGravityOutsideFields)
        {
            RestoreMovement(Movement);
            if (bReturnToDefaultUpOutsideFields && bHasAlignedActor)
            {
                const FVector DefaultUp = -SavedGravityDirection.GetSafeNormal();
                AlignCharacterToUp(DefaultUp.IsNearlyZero() ? FVector::UpVector : DefaultUp, DeltaTime);
            }
        }
        // Opting out of native fallback deliberately preserves the last field's movement state.
        return;
    }

    if (!bControllingMovement)
    {
        SavedGravityDirection = Movement->GetGravityDirection();
        if (SavedGravityDirection.IsNearlyZero()) { SavedGravityDirection = FVector::DownVector; }
        SavedGravityScale = Movement->GravityScale;
        bControllingMovement = true;
    }

    if (Sample.bHasValidDirection)
    {
        Movement->SetGravityDirection(Sample.Down);
        AlignCharacterToUp(Sample.Up, DeltaTime);
    }
    // Zero-gravity fields intentionally set magnitude to zero but preserve a stable orientation.
    if (bMatchGravityMagnitude)
    {
        float NewScale = 0.0f;
        if (FOrbitGravityFrame::MakeGravityScale(Sample.Magnitude, FMath::Abs(World->GetGravityZ()),
            MaxGravityScale, NewScale))
        {
            Movement->GravityScale = NewScale;
        }
        else if (!bWarnedZeroWorldGravity)
        {
            bWarnedZeroWorldGravity = true;
            UE_LOG(LogOrbitCharacter, Warning,
                TEXT("Orbit cannot match Character gravity magnitude while WorldSettings gravity is zero or configuration is invalid (%s). Direction still works."),
                *GetNameSafe(Character));
        }
    }
}
