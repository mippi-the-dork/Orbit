#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitCharacterGravityComponent.generated.h"

class UCharacterMovementComponent;

/** Optional bridge to UE 5.8 Character Movement custom gravity, strength and actor orientation.
 *  Does not modify camera, controller, animation or replicated movement prediction.
 */
UCLASS(ClassGroup=(Orbit), meta=(BlueprintSpawnableComponent), BlueprintType)
class ORBIT_API UOrbitCharacterGravityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOrbitCharacterGravityComponent();

    /** Restore native movement gravity direction/scale when the actor leaves all matching Orbit fields. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character")
    bool bUseDefaultGravityOutsideFields = true;

    /** Make CharacterMovement gravity strength match sampled Orbit acceleration. Enabled by default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Magnitude")
    bool bMatchGravityMagnitude = true;

    /** Safety ceiling for mapping strength to CharacterMovement.GravityScale. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Magnitude",
        meta=(EditCondition="bMatchGravityMagnitude", ClampMin="0.0", UIMin="0.0", UIMax="20.0"))
    float MaxGravityScale = 100.0f;

    /** Experimental actor/capsule alignment to local Up. Off by default so existing camera/control systems are untouched. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Orientation")
    bool bAlignActorToGravity = false;

    /** Exponential orientation interpolation rate (1/seconds). Zero snaps immediately. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Orientation",
        meta=(EditCondition="bAlignActorToGravity", ClampMin="0.0", UIMin="0.0", UIMax="20.0"))
    float OrientationResponseRate = 8.0f;

    /** When leaving a field, smoothly return the actor to its original gravity Up direction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Orientation",
        meta=(EditCondition="bAlignActorToGravity"))
    bool bReturnToDefaultUpOutsideFields = true;

    /** Retain previous winning Override source until the new one is meaningfully stronger.
     * Uses Project Settings > Orbit hysteresis fraction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Transitions")
    bool bUseSourceHysteresis = true;

    /** Smooth changes in native CharacterMovement gravity direction. Off leaves instant handoffs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Transitions")
    bool bSmoothGravityDirection = true;

    /** Exponential interpolation rate in 1/s, independent of gravity acceleration magnitude. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Transitions",
        meta=(EditCondition="bSmoothGravityDirection", ClampMin="0.0", UIMin="0.0", UIMax="25.0"))
    float GravityDirectionResponseRate = 5.0f;

    /** Experimental near-floor transition ONLY after UE's native capsule sweep validates a
     *  walkable landing surface. Does not teleport, trace through surfaces or fake ground. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Landing")
    bool bEnableNearFloorLandingAssist = true;

    /** Maximum gap between falling capsule and a sweep-validated floor (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Landing",
        meta=(EditCondition="bEnableNearFloorLandingAssist", ClampMin="1.0", ClampMax="20.0", Units="cm"))
    float NearFloorLandingDistance = 10.0f;

    /** Safety cap for post-movement floor acquisition. Default 600 accommodates initial planet drops,
     *  unlike the former 100 cm/s cap. No assist occurs without a walkable native sweep. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Landing",
        meta=(EditCondition="bEnableNearFloorLandingAssist", ClampMin="1.0", ClampMax="1500.0", UIMax="800.0", Units="cm/s"))
    float NearFloorMaxApproachSpeed = 600.0f;

    /** One-line readout of the LAST post-movement landing-assist decision. This is an
     *  observation aid, not a guarantee that a floor transition will be possible. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    FString LastLandingAssistResult = TEXT("Not evaluated");

    /** Last gravity-relative descent and lateral speed seen by the post-movement probe (cm/s). */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    float LastLandingDescentSpeed = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    float LastLandingLateralSpeed = 0.0f;

    /** Distinguishes first-spawn falling from jumps after an actual Walking update. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    bool bHasBeenGrounded = false;

    /** Number of post-movement checks while Falling before first ground contact. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    int32 PreFirstLandingChecks = 0;

    /** True when the native FindFloor result reports a walkable floor in landing-assist range. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    bool bNearFloorWalkable = false;

    /** Native FindFloor distance, even when the assist declines to switch movement modes. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    float LastNativeFloorDistance = -1.0f;

    /** Read-only runtime diagnostics for PIE; distinguish actual game-world changes
     *  from edits to an inactive editor-world source. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    float LastAppliedGravityScale = 1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    int32 LastDominantSourceId = INDEX_NONE;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    float LastWalkableFloorDistance = -1.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    bool bLandingAssistTriggered = false;

    /** Persistent counter, easier to inspect than a one-frame boolean in Details. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    int32 LandingAssistCount = 0;

    /** Last source-specific acceleration sampled by the adapter, for debugging and gameplay. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    FVector LastFieldAcceleration = FVector::ZeroVector;

    /** True when an Orbit source matching this Character is currently active at its location. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Orbit|Character|Debug")
    bool bInsideOrbitField = false;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    bool bControllingMovement = false;
    int32 PreferredGravitySourceId = INDEX_NONE;
    bool bHasAlignedActor = false;
    bool bWarnedZeroWorldGravity = false;
    float SavedGravityScale = 1.0f;
    FVector SavedGravityDirection = FVector::DownVector;

    /** After CharacterMovement finishes its movement scope. Native movement gets first chance to land. */
    UFUNCTION()
    void OnCharacterMovementFinished(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

    void TryAssistNearFloorLanding(UCharacterMovementComponent* Movement);
    void RestoreMovement(UCharacterMovementComponent* Movement);
    void AlignCharacterToUp(const FVector& Up, float DeltaTime);
};
