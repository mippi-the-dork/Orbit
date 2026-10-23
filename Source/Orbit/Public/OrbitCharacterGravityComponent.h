#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitCharacterGravityComponent.generated.h"

/** Optional experimental bridge to native CharacterMovement gravity direction. Does not align cameras or meshes. */
UCLASS(ClassGroup=(Orbit), meta=(BlueprintSpawnableComponent), BlueprintType)
class ORBIT_API UOrbitCharacterGravityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOrbitCharacterGravityComponent();

    /** Keep the character's native gravity direction when Orbit has no influencing source. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Character")
    bool bUseDefaultGravityOutsideFields = true;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
