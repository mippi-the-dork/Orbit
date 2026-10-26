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

    /** Experimental near-floor transition ONLY after UE's native capsule sweep validates a
     *  walkable landing surface. Does not teleport, trace through surfaces or fake ground. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Landing")
    bool bEnableNearFloorLandingAssist = true;

    /** Maximum gap between falling capsule and a sweep-validated floor (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Landing",
        meta=(EditCondition="bEnableNearFloorLandingAssist", ClampMin="1.0", ClampMax="20.0", Units="cm"))
    float NearFloorLandingDistance = 10.0f;

    /** Only assist characters approaching a floor slowly, not high-speed impacts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character|Landing",
        meta=(EditCondition="bEnableNearFloorLandingAssist", ClampMin="1.0", ClampMax="300.0", Units="cm/s"))
    float NearFloorMaxApproachSpeed = 100.0f;

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
    bool bHasAlignedActor = false;
    bool bWarnedZeroWorldGravity = false;
    float SavedGravityScale = 1.0f;
    FVector SavedGravityDirection = FVector::DownVector;

    void TryAssistNearFloorLanding(UCharacterMovementComponent* Movement);
    void RestoreMovement(UCharacterMovementComponent* Movement);
    void AlignCharacterToUp(const FVector& Up, float DeltaTime);
};
