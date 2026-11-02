#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "OrbitBlueprintLibrary.h"
#include "OrbitGravityAnimInstance.generated.h"

/** Optional AnimBP base class that caches gravity-relative animation state each animation update.
 *  Existing AnimBPs can keep using Get Orbit Gravity Animation State directly without reparenting.
 *  Reparenting does NOT automatically replace old Blend Space variable wiring. */
UCLASS(Blueprintable, BlueprintType)
class ORBIT_API UOrbitGravityAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    /** Speed below which ShouldMove becomes false, in cm/s. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Orbit|Animation", meta=(ClampMin="0.0"))
    float OrbitMinGroundSpeed = 3.0f;

    /** Acceleration magnitude required by the ShouldMove gate. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Orbit|Animation", meta=(ClampMin="0.0"))
    float OrbitMinAcceleration = 0.1f;

    /** Latest full gravity-local animation state. Updated on the game thread by NativeUpdateAnimation. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Orbit|Animation")
    FOrbitGravityAnimationState OrbitAnimationState;

    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
