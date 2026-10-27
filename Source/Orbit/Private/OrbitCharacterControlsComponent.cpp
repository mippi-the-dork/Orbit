#include "OrbitCharacterControlsComponent.h"

#include "OrbitGravityFrame.h"
#include "OrbitCharacterGravityComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/RotationMatrix.h"
#include "Engine/World.h"

UOrbitCharacterControlsComponent::UOrbitCharacterControlsComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UOrbitCharacterControlsComponent::BeginPlay()
{
    Super::BeginPlay();
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            AddTickPrerequisiteComponent(Movement);
            // UE broadcasts after the native movement scope. Make facing coherent
            // with that frame's velocity before dependent animation evaluation.
            Character->OnCharacterMovementUpdated.AddUniqueDynamic(
                this, &UOrbitCharacterControlsComponent::OnNativeMovementFinished);
        }
    }
    UpdateCameraOwnership();
    UpdateNativeRotationOwnership();
    RefreshFacingDebugStatus();
}

void UOrbitCharacterControlsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        Character->OnCharacterMovementUpdated.RemoveDynamic(
            this, &UOrbitCharacterControlsComponent::OnNativeMovementFinished);
    }
    RestoreCameraOwnership();
    RestoreNativeRotationOwnership();
    Super::EndPlay(EndPlayReason);
}

bool UOrbitCharacterControlsComponent::IsManagingCharacterOrientation() const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    return bFaceMovementDirection && IsActive() && IsComponentTickEnabled() &&
        Character && Character->IsLocallyControlled();
}

void UOrbitCharacterControlsComponent::RestoreNativeRotationOwnership()
{
    if (!bOwnsNativeRotation) { return; }
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        Character->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
        if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
        {
            Move->bOrientRotationToMovement = bSavedOrientRotationToMovement;
            Move->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
        }
    }
    bOwnsNativeRotation = false;
}

void UOrbitCharacterControlsComponent::UpdateNativeRotationOwnership()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
    if (!IsManagingCharacterOrientation() || !bDisableCompetingNativeRotation || !Move)
    {
        RestoreNativeRotationOwnership();
        return;
    }
    if (bOwnsNativeRotation) { return; }
    bSavedOrientRotationToMovement = Move->bOrientRotationToMovement;
    bSavedUseControllerDesiredRotation = Move->bUseControllerDesiredRotation;
    bSavedUseControllerRotationYaw = Character->bUseControllerRotationYaw;
    Move->bOrientRotationToMovement = false;
    Move->bUseControllerDesiredRotation = false;
    Character->bUseControllerRotationYaw = false;
    bOwnsNativeRotation = true;
}

void UOrbitCharacterControlsComponent::RestoreCameraOwnership()
{
    if (bOwnsSpringArmRotation)
    {
        if (USpringArmComponent* Boom = CameraSpringArm.Get())
        {
            Boom->bUsePawnControlRotation = bSavedBoomUsePawnControlRotation;
            Boom->SetUsingAbsoluteRotation(bSavedBoomAbsoluteRotation);
        }
        bOwnsSpringArmRotation = false;
    }
}

void UOrbitCharacterControlsComponent::UpdateCameraOwnership()
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const bool bShouldOwn = Character && Character->IsLocallyControlled() &&
        bOrientCameraToGravity && bPreferSpringArmCamera;
    if (!bShouldOwn)
    {
        RestoreCameraOwnership();
        return;
    }
    USpringArmComponent* Boom = Character->FindComponentByClass<USpringArmComponent>();
    if (!Boom)
    {
        RestoreCameraOwnership();
        return;
    }
    if (bOwnsSpringArmRotation && CameraSpringArm.Get() == Boom) { return; }
    RestoreCameraOwnership();
    CameraSpringArm = Boom;
    bSavedBoomUsePawnControlRotation = Boom->bUsePawnControlRotation;
    bSavedBoomAbsoluteRotation = Boom->IsUsingAbsoluteRotation();
    Boom->bUsePawnControlRotation = false;
    Boom->SetUsingAbsoluteRotation(true);
    bOwnsSpringArmRotation = true;
}

FVector UOrbitCharacterControlsComponent::GetCurrentUp() const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    return Movement && !Movement->GetGravityDirection().IsNearlyZero()
        ? -Movement->GetGravityDirection().GetSafeNormal() : FVector::UpVector;
}

void UOrbitCharacterControlsComponent::RefreshCameraFrame(const FVector& NewUp)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !Character->GetController() || NewUp.IsNearlyZero()) { return; }
    const FVector Up = NewUp.GetSafeNormal();
    if (!bInitializedCameraFrame)
    {
        const FQuat ControlQuat = Character->GetControlRotation().Quaternion();
        const FVector CurrentLook = ControlQuat.GetForwardVector();
        const FVector Reference = Character->GetActorForwardVector();
        FVector Right;
        if (!FOrbitGravityFrame::MakeTangentBasis(Up,
                FVector::VectorPlaneProject(CurrentLook, Up).IsNearlyZero() ? Reference : CurrentLook,
                CameraTangentForward, Right))
        {
            return;
        }
        CameraPitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(
            FVector::DotProduct(CurrentLook.GetSafeNormal(), Up), -1.0, 1.0)));
        CameraPitch = FMath::Clamp(CameraPitch, FMath::Min(MinPitch, MaxPitch), FMath::Max(MinPitch, MaxPitch));
        CameraFrameUp = Up;
        bInitializedCameraFrame = true;
    }
    else
    {
        // Parallel-transport the previous heading as gravity Up changes. This avoids
        // re-extracting yaw from Euler angles at the equator or over a planet's poles.
        const FQuat Transport = FQuat::FindBetweenNormals(CameraFrameUp, Up);
        const FVector Transported = Transport.RotateVector(CameraTangentForward);
        FVector Right;
        if (FOrbitGravityFrame::MakeTangentBasis(Up, Transported, CameraTangentForward, Right))
        {
            CameraFrameUp = Up;
        }
    }
    LastGravityUp = Up;
}

void UOrbitCharacterControlsComponent::ApplyCameraRotation()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!bOrientCameraToGravity || !bInitializedCameraFrame || !Character ||
        !Character->IsLocallyControlled() || !Character->GetController())
    {
        return;
    }
    FVector Forward, Right;
    if (!FOrbitGravityFrame::MakeTangentBasis(CameraFrameUp, CameraTangentForward, Forward, Right))
    {
        return;
    }
    const FQuat Base = FRotationMatrix::MakeFromXZ(Forward, CameraFrameUp).ToQuat();
    // Unreal positive pitch is look-up, i.e. negative rotation around local +Y.
    const FQuat Pitch(FVector::RightVector, FMath::DegreesToRadians(-CameraPitch));
    const FQuat CameraRotation = (Base * Pitch).GetNormalized();
    if (!CameraRotation.ContainsNaN())
    {
        if (bOwnsSpringArmRotation && CameraSpringArm.IsValid())
        {
            CameraSpringArm->SetWorldRotation(CameraRotation);
        }
        else
        {
            // Projects with no SpringArm still get a limited fallback. Traditional
            // ControlRotation is Euler-based and can clamp at world +/-90 pitch.
            Character->GetController()->SetControlRotation(CameraRotation.Rotator());
        }
    }
}

void UOrbitCharacterControlsComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ++DebugTickCount;
    UpdateCameraOwnership();
    UpdateNativeRotationOwnership();
    if (bOrientCameraToGravity)
    {
        RefreshCameraFrame(GetCurrentUp());
        ApplyCameraRotation();
    }
    // Character Movement may skip its update delegate for a frame (e.g. movement
    // disabled/paused). The old code had no fallback, so native facing could be
    // suspended indefinitely without Orbit applying a replacement rotation.
    // This component ticks AFTER Character Movement. Only use the fallback when
    // a post-movement facing update did not already happen since our last tick.
    if (!bReceivedMovementCallbackSinceLastTick && IsManagingCharacterOrientation())
    {
        ++TickFacingFallbackCount;
        UpdateCharacterFacing(DeltaTime);
    }
    bReceivedMovementCallbackSinceLastTick = false;
    RefreshFacingDebugStatus();
    // Always draw when requested, even if facing ownership is inactive. Making
    // the drawing conditional on IsManagingCharacterOrientation() concealed the
    // exact failure that debug drawing was supposed to diagnose.
    if (bDrawLocomotionDebug) { DrawLocomotionDebug(); }
}

void UOrbitCharacterControlsComponent::OnNativeMovementFinished(
    float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
    // This callback occurs after native movement, not during a physics sweep.
    // Native gravity was installed by the Orbit Gravity component before movement.
    ++NativeMovementCallbackCount;
    bReceivedMovementCallbackSinceLastTick = true;
    UpdateCharacterFacing(DeltaSeconds);
}

void UOrbitCharacterControlsComponent::ApplyGravityRelativeLook(FVector2D LookAxis)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !Character->IsLocallyControlled() || !Character->GetController() ||
        !FMath::IsFinite(LookAxis.X) || !FMath::IsFinite(LookAxis.Y))
    {
        return;
    }
    if (!bOrientCameraToGravity)
    {
        Character->AddControllerYawInput(static_cast<float>(LookAxis.X));
        Character->AddControllerPitchInput(static_cast<float>(LookAxis.Y));
        return;
    }
    UpdateCameraOwnership();
    RefreshCameraFrame(GetCurrentUp());
    if (!bInitializedCameraFrame) { return; }
    const FQuat Yaw(CameraFrameUp, FMath::DegreesToRadians(LookAxis.X * YawDegreesPerUnit));
    CameraTangentForward = Yaw.RotateVector(CameraTangentForward).GetSafeNormal();
    CameraPitch = FMath::Clamp(CameraPitch + static_cast<float>(LookAxis.Y) * PitchDegreesPerUnit,
        FMath::Min(MinPitch, MaxPitch), FMath::Max(MinPitch, MaxPitch));
    ApplyCameraRotation();
}

void UOrbitCharacterControlsComponent::ApplyGravityRelativeMove(FVector2D MoveAxis)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !Character->IsLocallyControlled() || !Character->GetController() ||
        !FMath::IsFinite(MoveAxis.X) || !FMath::IsFinite(MoveAxis.Y))
    {
        return;
    }
    const FVector Up = GetCurrentUp();
    FVector Forward, Right;
    const FVector PreferredForward = bOrientCameraToGravity && bInitializedCameraFrame
        ? CameraTangentForward : Character->GetControlRotation().Quaternion().GetForwardVector();
    if (!FOrbitGravityFrame::MakeTangentBasis(Up, PreferredForward, Forward, Right))
    {
        return;
    }
    ++OrbitMoveInputCount;
    LastGravityUp = Up;
    LastMoveForward = Forward;
    LastMoveRight = Right;
    if (MoveAxis.SizeSquared() >= FMath::Square(FacingInputThreshold))
    {
        RequestedFacingDirection = (Forward * MoveAxis.Y + Right * MoveAxis.X).GetSafeNormal();
        if (UWorld* World = GetWorld())
        {
            LastFacingInputSeconds = World->GetTimeSeconds();
        }
    }
    // Capture and suspend native facing BEFORE native movement consumes input,
    // including on the first possession frame.
    UpdateNativeRotationOwnership();
    // Use two separate calls so diagonals receive the same scaling as the template.
    Character->AddMovementInput(Forward, static_cast<float>(MoveAxis.Y));
    Character->AddMovementInput(Right, static_cast<float>(MoveAxis.X));
}

void UOrbitCharacterControlsComponent::UpdateCharacterFacing(float DeltaTime)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UWorld* World = GetWorld();
    if (!IsManagingCharacterOrientation() || !Character || !World) { return; }

    const FVector Up = GetCurrentUp().GetSafeNormal();
    if (Up.IsNearlyZero()) { return; }
    FVector Forward, Right;
    if (!bOrientationFrameInitialized)
    {
        if (!FOrbitGravityFrame::MakeTangentBasis(Up, Character->GetActorForwardVector(),
            OrientationFrameForward, Right)) { return; }
        OrientationFrameUp = Up;
        bOrientationFrameInitialized = true;
    }
    else
    {
        if (!FOrbitGravityFrame::TransportTangentHeading(OrientationFrameUp,
            OrientationFrameForward, Up, Forward)) { return; }
        OrientationFrameForward = Forward;
        OrientationFrameUp = Up;
    }

    const bool bHasRecentInput = !RequestedFacingDirection.IsNearlyZero() &&
        LastFacingInputSeconds >= 0.0 &&
        World->GetTimeSeconds() - LastFacingInputSeconds <= 0.15;
    // Native AddMovementInput paths can move the Character without ever calling
    // ApplyGravityRelativeMove. Actual gravity-tangent velocity is still enough
    // to determine facing; it must not be gated on recently cached Orbit input.
    const UCharacterMovementComponent* FacingMovement = Character->GetCharacterMovement();
    const FVector FacingVelocity = FacingMovement ? FacingMovement->Velocity : FVector::ZeroVector;
    FVector DesiredHeading;
    const bool bHasFacingHeading = FOrbitGravityFrame::ChooseGravityRelativeFacing(
        Up, FacingVelocity, RequestedFacingDirection, bHasRecentInput,
        bFaceVelocityWhenMoving, VelocityFacingThreshold, DesiredHeading);
    if (bHasFacingHeading) { OrientationFrameForward = DesiredHeading; }

    if (!FOrbitGravityFrame::MakeTangentBasis(Up, OrientationFrameForward, Forward, Right)) { return; }
    const FQuat Target = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
    const FQuat Current = Character->GetActorQuat();

    // ONE actor-rotation owner when Orbit Controls is active. Character Gravity
    // still sets the native movement gravity direction before movement but skips
    // its independent actor rotation. During idle, follow its alignment rate.
    float ResponseRate = FacingResponseRate;
    if (!bHasFacingHeading)
    {
        if (const UOrbitCharacterGravityComponent* Gravity =
            Character->FindComponentByClass<UOrbitCharacterGravityComponent>())
        {
            if (Gravity->bAlignActorToGravity) { ResponseRate = Gravity->OrientationResponseRate; }
        }
    }
    const float Alpha = ResponseRate <= 0.0f ? 1.0f :
        1.0f - FMath::Exp(-ResponseRate * FMath::Max(0.0f, DeltaTime));
    const FQuat NewRotation = FQuat::Slerp(Current, Target, Alpha).GetNormalized();
    if (!NewRotation.ContainsNaN())
    {
        Character->SetActorRotation(NewRotation, ETeleportType::None);
        LastFacingDirection = Forward;
    }

    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const FVector Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;
    const FVector TangentVelocity = FVector::VectorPlaneProject(Velocity, Up);
    double GroundSpeed = 0.0, ForwardSpeed = 0.0, SideSpeed = 0.0, Direction = 0.0;
    FOrbitGravityFrame::CalculateGravityRelativeLocomotion(Velocity, Up,
        Character->GetActorForwardVector(), GroundSpeed, ForwardSpeed, SideSpeed, Direction);
    LastGravityRelativeGroundSpeed = static_cast<float>(GroundSpeed);
    LastGravityRelativeDirectionDegrees = static_cast<float>(Direction);
    LastWorldXYSpeed = static_cast<float>(FVector(Velocity.X, Velocity.Y, 0.0).Size());
    LastCapsuleUpErrorDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(
        FMath::Clamp(FVector::DotProduct(Character->GetActorUpVector(), Up), -1.0, 1.0))));
    const FVector ActualHeading = FVector::VectorPlaneProject(Character->GetActorForwardVector(), Up).GetSafeNormal();
    LastVelocityToFacingAngleDegrees = TangentVelocity.IsNearlyZero() || ActualHeading.IsNearlyZero()
        ? 0.0f : static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
            FVector::DotProduct(ActualHeading, TangentVelocity.GetSafeNormal()), -1.0, 1.0))));
}

void UOrbitCharacterControlsComponent::RefreshFacingDebugStatus()
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        LastFacingDebugStatus = TEXT("Owner is not an ACharacter");
    }
    else if (!IsActive())
    {
        LastFacingDebugStatus = TEXT("Controls component is not active");
    }
    else if (!IsComponentTickEnabled())
    {
        LastFacingDebugStatus = TEXT("Controls component tick is disabled");
    }
    else if (!bFaceMovementDirection)
    {
        LastFacingDebugStatus = TEXT("Face Movement Direction is disabled");
    }
    else if (!Character->IsLocallyControlled())
    {
        LastFacingDebugStatus = TEXT("Character is not locally controlled; Orbit does not own its facing");
    }
    else if (NativeMovementCallbackCount > 0)
    {
        LastFacingDebugStatus = OrbitMoveInputCount > 0
            ? TEXT("Facing active: native movement callback + Orbit input")
            : TEXT("Facing active: native movement callback; no Orbit move input observed");
    }
    else if (TickFacingFallbackCount > 0)
    {
        LastFacingDebugStatus = OrbitMoveInputCount > 0
            ? TEXT("Facing active: tick fallback + Orbit input; native callback absent")
            : TEXT("Facing active: tick fallback; no Orbit move input observed");
    }
    else
    {
        LastFacingDebugStatus = TEXT("Facing eligible; waiting for the first movement update");
    }
}

void UOrbitCharacterControlsComponent::DrawLocomotionDebug() const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const UWorld* World = GetWorld();
    if (!Character || !World) { return; }
    const FVector Up = GetCurrentUp();
    const FVector Start = Character->GetActorLocation() + Up * 85.0;
    const float Length = FMath::Max(10.0f, DebugVectorLength);
    DrawDebugDirectionalArrow(World, Start, Start + Up * Length, 18.0f, FColor::White, false, 0.0f, 0, 2.5f);
    const FVector Requested = FVector::VectorPlaneProject(RequestedFacingDirection, Up).GetSafeNormal();
    if (!Requested.IsNearlyZero())
    {
        DrawDebugDirectionalArrow(World, Start, Start + Requested * Length, 18.0f, FColor::Blue, false, 0.0f, 0, 2.5f);
    }
    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const FVector Velocity = Movement ? FVector::VectorPlaneProject(Movement->Velocity, Up).GetSafeNormal() : FVector::ZeroVector;
    if (!Velocity.IsNearlyZero())
    {
        DrawDebugDirectionalArrow(World, Start, Start + Velocity * Length, 18.0f, FColor::Green, false, 0.0f, 0, 2.5f);
    }
    const FVector Facing = FVector::VectorPlaneProject(Character->GetActorForwardVector(), Up).GetSafeNormal();
    if (!Facing.IsNearlyZero())
    {
        DrawDebugDirectionalArrow(World, Start, Start + Facing * Length, 18.0f, FColor::Yellow, false, 0.0f, 0, 2.5f);
    }
}
