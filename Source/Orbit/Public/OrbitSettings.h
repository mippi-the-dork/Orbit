#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OrbitSettings.generated.h"

/** Project Settings > Plugins > Orbit. Settings apply to new and existing play worlds. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Orbit"))
class ORBIT_API UOrbitSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

    /** Enables experimental automatic game-thread gravity on ordinary simulating components. */
    UPROPERTY(EditAnywhere, Config, Category="Physics", meta=(DisplayName="Automatically Apply to Physics Bodies"))
    bool bAutomaticRigidBodyGravity = true;

    /** Do not silently turn objects intentionally configured for zero gravity into normal gravity objects. */
    UPROPERTY(EditAnywhere, Config, Category="Physics")
    bool bIncludeInitiallyGravityDisabledBodies = false;

    /** Zero uses a new component discovery pass every frame; larger intervals reduce scans but delay new object pickup. */
    UPROPERTY(EditAnywhere, Config, Category="Physics", meta=(ClampMin="0.0", ClampMax="5.0", Units="s"))
    float PhysicsDiscoveryInterval = 0.20f;

    /** Use normal WorldSettings gravity outside all Orbit sources; otherwise unused regions are zero-g. */
    UPROPERTY(EditAnywhere, Config, Category="Field")
    bool bUseWorldGravityOutsideFields = true;

    /** Global safety toggle; disable if another system is responsible for physical gravity. */
    UPROPERTY(EditAnywhere, Config, Category="Physics")
    bool bEnablePhysicsIntegration = true;
};
