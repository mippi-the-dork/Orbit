#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OrbitGravityOrientationComponent.h"
#include "OrbitGravityTypes.h"
#include "OrbitBlueprintLibrary.generated.h"

class ACharacter;

/** Animation-friendly velocities expressed in the character's gravity-relative forward/right axes.
 *  Does not change AnimBP logic unless the animation blueprint consumes these values. */
USTRUCT(BlueprintType)
struct ORBIT_API FOrbitGravityRelativeLocomotion
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    bool bValid = false;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    FVector GravityUp = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    FVector TangentVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float GroundSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float ForwardSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float RightSpeed = 0.0f;

    /** Signed degrees relative to the actor's gravity-tangent forward: 0 forward, +90 right, -90 left, +/-180 backward. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float MovementDirectionDegrees = 0.0f;
};

/** Animation Blueprint-compatible locomotion state. Read on the game thread during
 * Event Blueprint Update Animation; cache fields for the AnimGraph. Merely adding
 * Orbit to a character does not rewrite the existing template Animation Blueprint. */
USTRUCT(BlueprintType)
struct ORBIT_API FOrbitGravityAnimationState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    bool bValid = false;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    bool bIsFalling = false;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    bool bIsMovingOnGround = false;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    bool bHasAcceleration = false;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    bool bShouldMove = false;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float GroundSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float ForwardSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float RightSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float MovementDirectionDegrees = 0.0f;

    /** Signed velocity along local gravity Up (cm/s). Positive = ascending, negative = descending.
     *  Use this in jump transitions instead of world Velocity.Z. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float GravityRelativeVerticalSpeed = 0.0f;

    /** True when vertical velocity exceeds +1 cm/s, in any gravity direction. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    bool bIsAscending = false;

    /** True when vertical velocity is below -1 cm/s, in any gravity direction. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    bool bIsDescending = false;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float TangentAccelerationMagnitude = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    FVector GravityUp = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    FVector GravityDown = FVector::DownVector;

    /** World-space floor normal from native CurrentFloor when grounded, otherwise GravityUp. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    FVector FloorNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    FVector TangentVelocity = FVector::ZeroVector;

    /** World-space normalized tangent travel direction for compatible Orientation Warping nodes. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    FVector WorldTangentMoveDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    FVector TangentAcceleration = FVector::ZeroVector;

    /** Debug comparison only; NOT suitable for planetary locomotion Blend Spaces. */
    UPROPERTY(BlueprintReadOnly, Category="Orbit|Animation")
    float WorldXYSpeed = 0.0f;
};

UCLASS()
class ORBIT_API UOrbitBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity", meta=(WorldContext="WorldContextObject"))
    static FOrbitGravitySample SampleOrbitGravity(const UObject* WorldContextObject, FVector WorldPosition, EOrbitGravityChannel Channel = EOrbitGravityChannel::General);

    /** Sample the channels configured on Actor's optional Gravity Receiver (default 0). */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity", meta=(WorldContext="WorldContextObject"))
    static FOrbitGravitySample SampleOrbitGravityForActor(const UObject* WorldContextObject, const AActor* Actor, FVector WorldPosition, EOrbitGravityChannel Channel = EOrbitGravityChannel::General);

    /** Removes the component of Vector along Up. Returns Vector unchanged if Up is invalid. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity")
    static FVector ProjectOntoGravityPlane(FVector Vector, FVector Up);

    /** Returns true only when an unambiguous gravity-relative up direction can be formed. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity")
    static bool GravityUpFromAcceleration(FVector Acceleration, FVector& Up);

    /** Builds a stable gravity-relative Forward/Right frame from local Up and a preferred world-space heading. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity|Orientation")
    static bool GetGravityTangentBasis(FVector Up, FVector PreferredForward, FVector& Forward, FVector& Right);

    /** Aligns an existing world rotation's local +Z to Up while preserving heading on the gravity tangent plane. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity|Orientation")
    static bool MakeGravityAlignedRotation(FRotator CurrentRotation, FVector Up, FRotator& AlignedRotation);

    /** Constructs a world-space frame: local +Z = Gravity Up; local +X is the
     *  preferred heading projected onto the tangent plane. Returns false for
     *  invalid Up, without silently inventing a gravity direction. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity|Transforms")
    static bool MakeGravityFrame(FVector WorldOrigin, FVector GravityUp,
        FVector PreferredForward, FTransform& GravityFrame);

    /** Source-aware gravity frame using the Actor's Receiver channels and a
     *  Character gravity query. Actor can be Self in ordinary Actor Blueprints. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity|Transforms", meta=(DefaultToSelf="Actor"))
    static bool MakeGravityFrameForActor(const AActor* Actor, FVector WorldPosition,
        FVector PreferredForward, FTransform& GravityFrame);

    /** Interpret RelativeTransform as local to the supplied gravity frame. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity|Transforms")
    static FTransform GravityLocalToWorldTransform(const FTransform& RelativeTransform, const FTransform& GravityFrame);

    /** Express a world-space transform in the supplied gravity frame. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity|Transforms")
    static FTransform GravityWorldToLocalTransform(const FTransform& WorldTransform, const FTransform& GravityFrame);

    /** Get a gravity Up direction for an Actor and optional explicit source.
     * If no source is specified, uses combined gravity with Receiver channels. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity|Orientation", meta=(DefaultToSelf="Actor"))
    static bool GetOrbitGravityUpForActor(const AActor* Actor, FVector WorldPosition,
        const UObject* SpecificSource, FVector& Up);

    /** Minimal rotation aligning any signed actor-local axis to gravity Up.
     * Preserves twist/heading as far as possible. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity|Orientation")
    static bool MakeOrbitAxisAlignedRotation(FRotator CurrentRotation, FVector GravityUp,
        EOrbitLocalAxis LocalAxis, FRotator& AlignedRotation);

    /** Apply gravity orientation once, not every Tick. Source can be a Gravity Source
     * Actor, Source Component, or empty to use the combined field. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Gravity|Orientation", meta=(DefaultToSelf="Actor"))
    static bool OrientActorToOrbitGravity(AActor* Actor, EOrbitLocalAxis LocalAxis,
        const UObject* SpecificSource);

    /** Convenient Character Blueprint call (no Orbit Controls component target pin).
     *  Requires Orbit Character Controls component on that Character. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Controls|Input", meta=(DefaultToSelf="Character", DisplayName="Apply Orbit Gravity Relative Move"))
    static void ApplyOrbitGravityRelativeMove(ACharacter* Character, FVector2D MoveAxis);

    /** Ground speed and locomotion direction using local Gravity Up, NOT world X/Y.
     *  In an AnimBP use Try Get Pawn Owner -> Cast to Character -> this node.
     *  Use the result instead of VectorLengthXY / world-Z CalculateDirection. */
    UFUNCTION(BlueprintPure, Category="Orbit|Animation", meta=(DefaultToSelf="Character"))
    static FOrbitGravityRelativeLocomotion GetGravityRelativeLocomotion(const ACharacter* Character);

    /** One-node Manny/Quinn-style animation state: gravity-local GroundSpeed,
     * direction, Acceleration, ShouldMove, and Falling. Existing AnimBP graphs must
     * read this node on the game thread and replace world-XY calculations explicitly. */
    UFUNCTION(BlueprintPure, Category="Orbit|Animation", meta=(DefaultToSelf="Character"))
    static FOrbitGravityAnimationState GetOrbitGravityAnimationState(const ACharacter* Character,
        float MinGroundSpeed = 3.0f, float MinAcceleration = 0.1f);

    /** Pure signed vertical-speed calculation for custom transition rules and non-Character actors.
     *  Requires a valid local gravity Up. + ascending, - descending; outputs 0 on failure. */
    UFUNCTION(BlueprintPure, Category="Orbit|Animation")
    static bool CalculateOrbitGravityVerticalSpeed(FVector Velocity, FVector GravityUp, float& SignedVerticalSpeed);

    /** Convenient Character Blueprint call (no Orbit Controls component target pin). */
    UFUNCTION(BlueprintCallable, Category="Orbit|Controls|Input", meta=(DefaultToSelf="Character", DisplayName="Apply Orbit Gravity Relative Look"))
    static void ApplyOrbitGravityRelativeLook(ACharacter* Character, FVector2D LookAxis);
};
