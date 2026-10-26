#include "OrbitCharacterControlsComponent.h"

#include "OrbitGravityFrame.h"
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
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            AddTickPrerequisiteComponent(Movement);
        }
    }
    UpdateCameraOwnership();
}

void UOrbitCharacterControlsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RestoreCameraOwnership();
    Super::EndPlay(EndPlayReason);
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
    UpdateCameraOwnership();
    if (bOrientCameraToGravity)
    {
        RefreshCameraFrame(GetCurrentUp());
        ApplyCameraRotation();
    }
    UpdateCharacterFacing(DeltaTime);
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
    // Use two separate calls so diagonals receive the same scaling as the template.
    Character->AddMovementInput(Forward, static_cast<float>(MoveAxis.Y));
    Character->AddMovementInput(Right, static_cast<float>(MoveAxis.X));
}

void UOrbitCharacterControlsComponent::UpdateCharacterFacing(float DeltaTime)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UWorld* World = GetWorld();
    if (!bFaceMovementDirection || !Character || !Character->IsLocallyControlled() ||
        !World || RequestedFacingDirection.IsNearlyZero() ||
        LastFacingInputSeconds < 0.0 || World->GetTimeSeconds() - LastFacingInputSeconds > 0.15)
    {
        return;
    }

    const FVector Up = GetCurrentUp();
    const FVector Forward = FVector::VectorPlaneProject(RequestedFacingDirection, Up).GetSafeNormal();
    if (Forward.IsNearlyZero()) { return; }

    // Use a tangent-space target rather than a world-yaw rotation. The absolute
    // Spring Arm orientation remains owned by the camera component, so character
    // heading can change without rolling or dragging the player's view.
    const FQuat Target = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
    const FQuat Current = Character->GetActorQuat();
    const float Alpha = FacingResponseRate <= 0.0f ? 1.0f :
        1.0f - FMath::Exp(-FacingResponseRate * FMath::Max(0.0f, DeltaTime));
    const FQuat NewRotation = FQuat::Slerp(Current, Target, Alpha).GetNormalized();
    if (!NewRotation.ContainsNaN())
    {
        Character->SetActorRotation(NewRotation, ETeleportType::None);
        LastFacingDirection = Forward;
    }
}
