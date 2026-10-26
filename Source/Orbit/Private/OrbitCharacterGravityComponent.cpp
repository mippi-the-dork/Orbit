#include "OrbitCharacterGravityComponent.h"

#include "OrbitGravityFrame.h"
#include "OrbitWorldSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
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

void UOrbitCharacterGravityComponent::TryAssistNearFloorLanding(UCharacterMovementComponent* Movement)
{
    bLandingAssistTriggered = false;
    bNearFloorWalkable = false;
    LastWalkableFloorDistance = -1.0f;
    LastNativeFloorDistance = -1.0f;

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!bEnableNearFloorLandingAssist || !bInsideOrbitField || !Character || !Movement ||
        !Movement->IsFalling() || LastFieldAcceleration.IsNearlyZero() ||
        !Character->GetCapsuleComponent() || NearFloorLandingDistance <= 0.0f ||
        NearFloorMaxApproachSpeed <= 0.0f)
    {
        return;
    }

    const FVector Down = Movement->GetGravityDirection();
    const FVector Velocity = Movement->Velocity;
    const double VerticalVelocityUp = FVector::DotProduct(Velocity, -Down);
    const double TangentSpeed = FVector::VectorPlaneProject(Velocity, Down).Size();
    if (VerticalVelocityUp > 0.0 || FMath::Abs(VerticalVelocityUp) > NearFloorMaxApproachSpeed ||
        TangentSpeed > NearFloorMaxApproachSpeed)
    {
        return; // No landing assist during upward jumps or fast motion.
    }

    const FVector Location = Character->GetCapsuleComponent()->GetComponentLocation();
    FFindFloorResult Floor;
    // UE 5.8's FindFloor applies the same gravity-relative floor policy that
    // Character Movement uses on entry to Walking. The previous Orbit code fed
    // the shortened-capsule ComputeFloorDist hit back into IsValidLandingSpot,
    // which checks a *full* capsule landing hit and can reject a valid floor.
    // Do not apply both different validation paths to the same synthetic hit.
    Movement->K2_FindFloor(Location, Floor);
    bNearFloorWalkable = Floor.IsWalkableFloor() && !Floor.bLineTrace &&
        Floor.HitResult.IsValidBlockingHit() && !Floor.HitResult.bStartPenetrating;
    if (Floor.bBlockingHit)
    {
        LastNativeFloorDistance = Floor.FloorDist;
    }
    if (!bNearFloorWalkable)
    {
        return;
    }

    const float FloorGap = Floor.FloorDist;
    LastWalkableFloorDistance = FloorGap;
    if (!FOrbitGravityFrame::ShouldAttemptNearFloorLanding(true, VerticalVelocityUp,
        TangentSpeed, NearFloorMaxApproachSpeed, FloorGap, NearFloorLandingDistance))
    {
        return;
    }

    // This never invents a collision or teleports a pawn: native Walking entry
    // performs its own FindFloor, AdjustFloorHeight and movement-base selection.
    // If a walkable base is not established, put the pawn back into Falling.
    Movement->SetMovementMode(MOVE_Walking);
    bLandingAssistTriggered = Movement->IsWalking() && Movement->CurrentFloor.IsWalkableFloor();
    if (bLandingAssistTriggered)
    {
        ++LandingAssistCount;
    }
    else if (Movement->IsWalking())
    {
        Movement->SetMovementMode(MOVE_Falling);
    }
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
        Character->SetActorRotation(NewRotation, ETeleportType::None);
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
    LastDominantSourceId = bInsideOrbitField ? Sample.DominantSourceId : INDEX_NONE;
    bLandingAssistTriggered = false;
    LastWalkableFloorDistance = -1.0f;
    LastNativeFloorDistance = -1.0f;
    bNearFloorWalkable = false;
    LastAppliedGravityScale = Movement->GravityScale;
    LastFieldAcceleration = bInsideOrbitField ? Sample.Acceleration : FVector::ZeroVector;

    if (!Sample.bHasInfluence)
    {
        if (bUseDefaultGravityOutsideFields)
        {
            RestoreMovement(Movement);
            LastAppliedGravityScale = Movement->GravityScale;
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
            LastAppliedGravityScale = NewScale;
        }
        else if (!bWarnedZeroWorldGravity)
        {
            bWarnedZeroWorldGravity = true;
            UE_LOG(LogOrbitCharacter, Warning,
                TEXT("Orbit cannot match Character gravity magnitude while WorldSettings gravity is zero or configuration is invalid (%s). Direction still works."),
                *GetNameSafe(Character));
        }
    }
    TryAssistNearFloorLanding(Movement);
}
