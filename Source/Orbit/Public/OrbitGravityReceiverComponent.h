#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitGravityTypes.h"
#include "OrbitGravityReceiverComponent.generated.h"

/**
 * Optional lighting-style gravity channel selection for an Actor.
 * With no Receiver component, ALL actors and physics components use channel 0.
 * A Receiver affects all primitive components owned by its actor and the
 * Orbit Character Gravity adapter; it does not change native Enable Gravity.
 */
UCLASS(ClassGroup=(Orbit), meta=(BlueprintSpawnableComponent), BlueprintType)
class ORBIT_API UOrbitGravityReceiverComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOrbitGravityReceiverComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Channels")
    FOrbitGravityChannels GravityChannels;

    UFUNCTION(BlueprintPure, Category="Orbit|Channels")
    int32 GetGravityChannelMask() const { return static_cast<int32>(GravityChannels.ToMask()); }
};
