#include "OrbitGravityAnimInstance.h"

#include "GameFramework/Character.h"

void UOrbitGravityAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
    OrbitAnimationState = UOrbitBlueprintLibrary::GetOrbitGravityAnimationState(
        Character, OrbitMinGroundSpeed, OrbitMinAcceleration);
}
