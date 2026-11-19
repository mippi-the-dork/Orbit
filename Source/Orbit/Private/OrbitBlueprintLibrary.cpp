#include "OrbitBlueprintLibrary.h"
#include "OrbitWorldSubsystem.h"
#include "OrbitSettings.h"
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
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"

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

TArray<FVector> UOrbitBlueprintLibrary::PredictOrbitProjectileTrajectory(const UObject* WorldContextObject,
    const AActor* ProjectileOwner, FVector StartLocation, FVector InitialVelocity,
    float TotalTime, float StepSeconds, float GravityScale, EOrbitGravityChannel GravityChannel,
    EOrbitProjectileOutsideField OutsideFieldBehavior)
{
    TArray<FVector> Points;
    if (StartLocation.ContainsNaN() || InitialVelocity.ContainsNaN() ||
        !FMath::IsFinite(TotalTime) || !FMath::IsFinite(StepSeconds) ||
        !FMath::IsFinite(GravityScale) || TotalTime <= 0.0f || StepSeconds <= 0.0f)
    {
        return Points;
    }
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject,
        EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) { return Points; }
    const UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Subsystem) { return Points; }

    const int32 Count = FMath::Clamp(FMath::CeilToInt(TotalTime / StepSeconds), 1, 512);
    const double Delta = static_cast<double>(TotalTime) / Count;
    const bool bNativeFallback = OutsideFieldBehavior == EOrbitProjectileOutsideField::NativeGravity ||
        (OutsideFieldBehavior == EOrbitProjectileOutsideField::ProjectSettings &&
         GetDefault<UOrbitSettings>()->bUseWorldGravityOutsideFields);
    FVector Location = StartLocation;
    FVector Velocity = InitialVelocity;
    Points.Reserve(Count + 1);
    Points.Add(Location);
    for (int32 i = 0; i < Count; ++i)
    {
        // Midpoint integration samples curved fields at a predicted halfway position.
        const FVector SampleLocation = Location + Velocity * (Delta * 0.5);
        const FOrbitGravitySample Sample = ProjectileOwner && ProjectileOwner->GetWorld() == World
            ? Subsystem->SampleGravityForActor(ProjectileOwner, SampleLocation, GravityChannel)
            : Subsystem->SampleGravity(SampleLocation, GravityChannel);
        const FVector NativeGravity(0.0, 0.0, World->GetGravityZ() * GravityScale);
        const FVector Acceleration = UOrbitProjectileMovementComponent::ReplaceGravityTerm(
            NativeGravity, NativeGravity.Z, Sample, GravityScale, true, bNativeFallback);
        Velocity += Acceleration * Delta;
        Location += Velocity * Delta;
        if (Location.ContainsNaN() || Velocity.ContainsNaN()) { Points.Reset(); return Points; }
        Points.Add(Location);
    }
    return Points;
}

FOrbitProjectileFlightPreview UOrbitBlueprintLibrary::PredictOrbitProjectileFlight(
    const UObject* WorldContextObject, const UOrbitProjectileMovementComponent* Projectile,
    FVector StartLocation, FVector InitialVelocity, float TotalTime, float StepSeconds,
    TEnumAsByte<ECollisionChannel> TraceChannel, float CollisionRadius, bool bPreviewBounce)
{
    FOrbitProjectileFlightPreview Result;
    if (!IsValid(Projectile) || !IsValid(Projectile->GetOwner()) ||
        StartLocation.ContainsNaN() || InitialVelocity.ContainsNaN() ||
        !FMath::IsFinite(TotalTime) || TotalTime <= 0.0f ||
        !FMath::IsFinite(StepSeconds) || StepSeconds <= 0.0f ||
        !FMath::IsFinite(CollisionRadius) || CollisionRadius < 0.0f)
    {
        return Result;
    }
    const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject,
        EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || World != Projectile->GetWorld()) { return Result; }
    const UOrbitWorldSubsystem* Subsystem = World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Subsystem) { return Result; }

    const int32 Steps = FMath::Clamp(FMath::CeilToInt(TotalTime / StepSeconds), 1, 512);
    const double Dt = static_cast<double>(TotalTime) / Steps;
    const bool bNativeFallback = Projectile->OutsideFieldBehavior == EOrbitProjectileOutsideField::NativeGravity ||
        (Projectile->OutsideFieldBehavior == EOrbitProjectileOutsideField::ProjectSettings &&
         GetDefault<UOrbitSettings>()->bUseWorldGravityOutsideFields);
    const AActor* Owner = Projectile->GetOwner();
    const UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(Projectile->UpdatedComponent);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OrbitProjectilePreview), false);
    Params.AddIgnoredActor(Owner);
    if (UpdatedPrimitive) { Params.AddIgnoredComponent(UpdatedPrimitive); }
    FVector Position = StartLocation;
    FVector Velocity = InitialVelocity;
    Result.Positions.Reserve(Steps + 1);
    Result.Positions.Add(Position);

    for (int32 Step = 0; Step < Steps; ++Step)
    {
        const FVector MidPoint = Position + Velocity * (Dt * 0.5);
        const FOrbitGravitySample Sample = Subsystem->SampleGravityForActor(Owner, MidPoint, Projectile->GravityChannel);
        // GetGravityZ includes ProjectileGravityScale and honors native physics volume gravity.
        const FVector NativeGravity(0.0, 0.0, Projectile->GetNativeProjectileGravityZForPreview());
        FVector Acceleration = UOrbitProjectileMovementComponent::ReplaceGravityTerm(
            NativeGravity, NativeGravity.Z, Sample, Projectile->ProjectileGravityScale,
            Projectile->bUseOrbitGravity, bNativeFallback);
        if (Projectile->bIsHomingProjectile && Projectile->HomingTargetComponent.IsValid() &&
            Projectile->HomingAccelerationMagnitude > 0.0f)
        {
            // Approximate native homing: accelerate toward the target from current flight location.
            Acceleration += (Projectile->HomingTargetComponent->GetComponentLocation() - MidPoint).GetSafeNormal() *
                Projectile->HomingAccelerationMagnitude;
        }
        Velocity += Acceleration * Dt;
        const FVector Desired = Position + Velocity * Dt;
        if (Desired.ContainsNaN() || Velocity.ContainsNaN()) { return FOrbitProjectileFlightPreview(); }

        FHitResult Hit;
        const bool bHit = CollisionRadius > KINDA_SMALL_NUMBER
            ? World->SweepSingleByChannel(Hit, Position, Desired, FQuat::Identity, TraceChannel,
                FCollisionShape::MakeSphere(CollisionRadius), Params)
            : World->LineTraceSingleByChannel(Hit, Position, Desired, TraceChannel, Params);
        if (bHit && Hit.bBlockingHit)
        {
            if (!Result.bHitSomething) { Result.bHitSomething = true; Result.FirstBlockingHit = Hit; }
            Position = Hit.Location;
            Result.Positions.Add(Position);
            const bool bCanBounce = bPreviewBounce && Projectile->bShouldBounce &&
                Result.BounceCount < 16 && !Hit.bStartPenetrating;
            if (!bCanBounce) { Result.FinalVelocity = Velocity; return Result; }
            Velocity = UOrbitProjectileMovementComponent::PredictBounceVelocity(
                Velocity, Hit.ImpactNormal, Projectile->Bounciness, Projectile->Friction);
            ++Result.BounceCount;
            // Advance outside the contact surface to avoid repeated zero-distance trace hits.
            Position += Hit.ImpactNormal.GetSafeNormal() * FMath::Max(0.1, static_cast<double>(CollisionRadius) * 0.01);
            if (Velocity.SizeSquared() <= FMath::Square(static_cast<double>(Projectile->BounceVelocityStopSimulatingThreshold)))
            {
                Result.FinalVelocity = Velocity;
                return Result;
            }
        }
        else
        {
            Position = Desired;
            Result.Positions.Add(Position);
        }
    }
    Result.FinalVelocity = Velocity;
    return Result;
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


bool UOrbitBlueprintLibrary::CalculateOrbitGravityVerticalSpeed(FVector Velocity,
    FVector GravityUp, float& SignedVerticalSpeed)
{
    SignedVerticalSpeed = 0.0f;
    double Result = 0.0;
    if (!FOrbitGravityFrame::CalculateGravityRelativeVerticalSpeed(Velocity, GravityUp, Result))
    {
        return false;
    }
    SignedVerticalSpeed = static_cast<float>(Result);
    return true;
}

FOrbitGravityAnimationState UOrbitBlueprintLibrary::GetOrbitGravityAnimationState(
    const ACharacter* Character, float MinGroundSpeed, float MinAcceleration)
{
    FOrbitGravityAnimationState State;
    if (!IsValid(Character)) { return State; }
    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement) { return State; }

    const FOrbitGravityRelativeLocomotion Locomotion = GetGravityRelativeLocomotion(Character);
    if (!Locomotion.bValid) { return State; }
    const FVector Acceleration = Movement->GetCurrentAcceleration();
    if (Acceleration.ContainsNaN()) { return State; }
    State.bValid = true;
    State.GravityUp = Locomotion.GravityUp;
    State.GravityDown = -State.GravityUp;
    State.FloorNormal = Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor()
        ? Movement->CurrentFloor.HitResult.ImpactNormal.GetSafeNormal() : State.GravityUp;
    State.TangentVelocity = Locomotion.TangentVelocity;
    State.WorldTangentMoveDirection = Locomotion.TangentVelocity.GetSafeNormal();
    State.GroundSpeed = Locomotion.GroundSpeed;
    State.ForwardSpeed = Locomotion.ForwardSpeed;
    State.RightSpeed = Locomotion.RightSpeed;
    State.MovementDirectionDegrees = Locomotion.MovementDirectionDegrees;
    double SignedVerticalSpeed = 0.0;
    if (!FOrbitGravityFrame::CalculateGravityRelativeVerticalSpeed(
        Movement->Velocity, State.GravityUp, SignedVerticalSpeed))
    {
        return FOrbitGravityAnimationState();
    }
    State.GravityRelativeVerticalSpeed = static_cast<float>(SignedVerticalSpeed);
    State.bIsAscending = SignedVerticalSpeed > 1.0;
    State.bIsDescending = SignedVerticalSpeed < -1.0;
    State.TangentAcceleration = FVector::VectorPlaneProject(Acceleration, State.GravityUp);
    State.TangentAccelerationMagnitude = static_cast<float>(State.TangentAcceleration.Size());
    State.bHasAcceleration = State.TangentAccelerationMagnitude > FMath::Max(0.0f, MinAcceleration);
    State.bShouldMove = State.GroundSpeed > FMath::Max(0.0f, MinGroundSpeed) && State.bHasAcceleration;
    State.bIsFalling = Movement->IsFalling();
    State.bIsMovingOnGround = Movement->IsMovingOnGround();
    State.WorldXYSpeed = static_cast<float>(FVector(Movement->Velocity.X, Movement->Velocity.Y, 0.0).Size());
    return State;
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
