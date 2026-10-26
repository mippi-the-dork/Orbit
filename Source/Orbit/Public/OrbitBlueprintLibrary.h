#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OrbitGravityTypes.h"
#include "OrbitBlueprintLibrary.generated.h"

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
};
