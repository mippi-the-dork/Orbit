#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitCharacterControlsComponent.generated.h"

class USpringArmComponent;

/** Optional input/camera bridge for ACharacter and Unreal's native Character Movement.
 *  Add to a Character alongside Orbit Character Gravity. It NEVER intercepts input:
 *  explicitly forward IA_Move and IA_Look 2D action values to the functions below.
 *  This does NOT integrate with Epic's Mover or modify arbitrary controllers by default.
 */
UCLASS(ClassGroup=(Orbit), meta=(BlueprintSpawnableComponent), BlueprintType)
class ORBIT_API UOrbitCharacterControlsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOrbitCharacterControlsComponent();

    /** When enabled, maintain the controller's view in a smoothly transported
     *  gravity-tangent reference frame. Spring arms using Pawn Control Rotation follow it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Camera")
    bool bOrientCameraToGravity = true;

    /** Drive the Character's first Spring Arm directly with a quaternion, avoiding
     *  world-pitch clamping in the usual PlayerController camera rotation. If no
     *  spring arm exists, the Controller rotation is used as a limited fallback. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Camera")
    bool bPreferSpringArmCamera = true;

    /** Degrees of local-Up yaw per unit of IA_Look X input (default third-person mouse behavior). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Camera", meta=(ClampMin="0.0"))
    float YawDegreesPerUnit = 1.0f;

    /** Degrees of local pitch per unit of IA_Look Y input. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Camera", meta=(ClampMin="0.0"))
    float PitchDegreesPerUnit = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Camera")
    float MinPitch = -80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Camera")
    float MaxPitch = 80.0f;

    /** Rotate the Character to face its tangent-plane movement input. Requires
     *  Orbit Character Gravity actor alignment for planetary movement. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Facing")
    bool bFaceMovementDirection = true;

    /** Face the actual gravity-tangent velocity instead of raw input when moving.
     *  This reduces visual strafing caused by camera-relative input and acceleration lag.
     *  This does not rewrite a template Animation Blueprint's world-XY locomotion math. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Facing",
        meta=(EditCondition="bFaceMovementDirection"))
    bool bFaceVelocityWhenMoving = true;

    /** Speed in cm/s above which actual tangent velocity controls facing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Facing",
        meta=(EditCondition="bFaceMovementDirection && bFaceVelocityWhenMoving", ClampMin="0.0", Units="cm/s"))
    float VelocityFacingThreshold = 30.0f;

    /** When this component manages character facing, temporarily disable native
     * OrientRotationToMovement / ControllerDesiredRotation / ControllerYaw ownership.
     * Previous native values are restored when Orbit stops owning facing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Facing")
    bool bDisableCompetingNativeRotation = true;

    /** Response rate for changing facing direction, in inverse seconds. Zero snaps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Facing",
        meta=(EditCondition="bFaceMovementDirection", ClampMin="0.0", UIMin="0.0", UIMax="25.0"))
    float FacingResponseRate = 12.0f;

    /** Movement input length threshold below which the Character keeps its facing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Facing",
        meta=(EditCondition="bFaceMovementDirection", ClampMin="0.0", ClampMax="1.0"))
    float FacingInputThreshold = 0.1f;

    /** Default IA_Move convention: X = right, Y = forward. Call INSTEAD of
     *  existing Third Person template AddMovementInput nodes. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Controls|Input")
    void ApplyGravityRelativeMove(FVector2D MoveAxis);

    /** Default IA_Look convention: X = yaw, Y = pitch. Call INSTEAD of
     *  existing Third Person template AddControllerYaw/PitchInput nodes. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Controls|Input")
    void ApplyGravityRelativeLook(FVector2D LookAxis);

    /** Draw gravity Up (white), requested input (blue), actual velocity (green),
     * desired facing (cyan) and actor forward (yellow). PIE only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Debug")
    bool bDrawLocomotionDebug = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Controls|Debug",
        meta=(EditCondition="bDrawLocomotionDebug", ClampMin="10.0", Units="cm"))
    float DebugVectorLength = 120.0f;

    /** Counts final-facing evaluation on the post-movement Controls tick. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 FacingPostMovementTickCount = 0;

    /** UpdateCharacterFacing entered and chose a nonzero velocity/input heading. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 FacingUpdateAttemptCount = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 FacingHeadingChosenCount = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 FacingRotationAppliedCount = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 FacingRotationRejectedCount = 0;

    /** Angular distance before applying the current desired facing rotation. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastFacingTargetErrorDegrees = 0.0f;

    /** Actual rotation accomplished by SetActorRotation in the last update. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastFacingAppliedDeltaDegrees = 0.0f;

    /** Remaining angular difference to target after SetActorRotation returns. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastFacingRemainingErrorDegrees = 0.0f;

    /** Actor rotation drift between the prior Orbit rotation and the current tick. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastFacingRotationDriftDegrees = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    FString LastFacingApplicationStatus = TEXT("Not yet attempted");

    /** Cyan debug arrow; latest gravity-tangent orientation target. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    FVector LastDesiredFacingDirection = FVector::ZeroVector;

    /** In-PIE diagnostics. If this stays at zero, the component is not ticking. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 DebugTickCount = 0;

    /** Number of BeginPlay activations that repaired an inactive Blueprint component instance. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 FacingBeginPlayActivationCount = 0;

    /** Increments when UE Character Movement broadcasts its completed-movement callback. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 NativeMovementCallbackCount = 0;

    /** Counts ticks where the native movement callback was absent. Facing now always
     * updates on post-movement tick, whether or not a callback occurred. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 TickFacingFallbackCount = 0;

    /** Increments when the Orbit movement input function receives a valid action value. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    int32 OrbitMoveInputCount = 0;

    /** Explains why the component currently can or cannot own facing. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    FString LastFacingDebugStatus = TEXT("Waiting for BeginPlay");

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastVelocityToFacingAngleDegrees = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastCapsuleUpErrorDegrees = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastGravityRelativeGroundSpeed = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastWorldXYSpeed = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    float LastGravityRelativeDirectionDegrees = 0.0f;

    /** Used by Orbit Character Gravity to avoid independently rotating the same Actor. */
    bool IsManagingCharacterOrientation() const;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    FVector LastGravityUp = FVector::UpVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    FVector LastMoveForward = FVector::ForwardVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    FVector LastMoveRight = FVector::RightVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Controls|Debug")
    FVector LastFacingDirection = FVector::ZeroVector;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** Native movement delegate is now diagnostic only. Final actor rotation is
     * applied by the component's PostUpdateWork tick after native movement. */
    UFUNCTION()
    void OnNativeMovementFinished(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

    bool bReceivedMovementCallbackSinceLastTick = false;
    bool bHasPreviousFacingResult = false;
    FQuat LastAppliedActorRotation = FQuat::Identity;
    bool bInitializedCameraFrame = false;
    bool bOwnsSpringArmRotation = false;
    bool bSavedBoomUsePawnControlRotation = false;
    bool bSavedBoomAbsoluteRotation = false;
    TWeakObjectPtr<USpringArmComponent> CameraSpringArm;
    FVector CameraTangentForward = FVector::ForwardVector;
    FVector CameraFrameUp = FVector::UpVector;
    float CameraPitch = 0.0f;
    FVector RequestedFacingDirection = FVector::ZeroVector;
    FVector OrientationFrameUp = FVector::UpVector;
    FVector OrientationFrameForward = FVector::ForwardVector;
    bool bOrientationFrameInitialized = false;
    bool bOwnsNativeRotation = false;
    bool bSavedOrientRotationToMovement = false;
    bool bSavedUseControllerDesiredRotation = false;
    bool bSavedUseControllerRotationYaw = false;
    void UpdateNativeRotationOwnership();
    void RestoreNativeRotationOwnership();
    void DrawLocomotionDebug() const;
    void RefreshFacingDebugStatus();
    double LastFacingInputSeconds = -1.0;
    void UpdateCharacterFacing(float DeltaTime);

    FVector GetCurrentUp() const;
    void UpdateCameraOwnership();
    void RestoreCameraOwnership();
    void RefreshCameraFrame(const FVector& NewUp);
    void ApplyCameraRotation();
};
