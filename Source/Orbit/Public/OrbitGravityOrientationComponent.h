#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitGravityOrientationComponent.generated.h"

class AOrbitGravitySource;
class UOrbitGravitySourceComponent;

/** Which local direction should be parallel to gravity's UP (against acceleration). */
UENUM(BlueprintType)
enum class EOrbitLocalAxis : uint8
{
    PositiveX UMETA(DisplayName="+X"), NegativeX UMETA(DisplayName="-X"),
    PositiveY UMETA(DisplayName="+Y"), NegativeY UMETA(DisplayName="-Y"),
    PositiveZ UMETA(DisplayName="+Z"), NegativeZ UMETA(DisplayName="-Z")
};

/** Maintains a plain actor's chosen local axis along gravity Up. Do not add to
 * a Character whose other movement component is already rotating the actor. */
UCLASS(ClassGroup=(Orbit), BlueprintType, meta=(BlueprintSpawnableComponent))
class ORBIT_API UOrbitGravityOrientationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOrbitGravityOrientationComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Orientation")
    bool bEnabled = true;
    /** Positive/negative local axis to aim toward the sampled gravity Up. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Orientation")
    EOrbitLocalAxis LocalAxis = EOrbitLocalAxis::PositiveZ;
    /** 0 snaps immediately. Higher values turn faster. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Orientation", meta=(ClampMin="0.0"))
    float ResponseRate = 8.0f;
    /** When unset, sample the combined gravity field and honor this actor's Receiver channels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Orientation|Source")
    TObjectPtr<AOrbitGravitySource> SpecificSourceActor = nullptr;
    /** Alternatively choose a gravity source component; takes precedence over Specific Source Actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Orientation|Source")
    TObjectPtr<UOrbitGravitySourceComponent> SpecificSourceComponent = nullptr;
    /** When gravity is zero/undefined, keep the actor's last valid orientation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Orientation")
    bool bHoldOrientationWithoutGravity = true;

    /** Align the actor immediately, regardless of response rate. Returns false if no valid direction. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Orientation")
    bool OrientNow();

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Orientation|Debug")
    FVector LastGravityUp = FVector::UpVector;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Orbit|Orientation|Debug")
    bool bLastSampleValid = false;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    bool ApplyOrientation(float DeltaTime, bool bImmediate);
};
