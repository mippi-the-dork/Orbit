#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrbitMoverGravityComponent.generated.h"

/**
 * Optional integration for synchronous kinematic Epic Mover, packaged INSIDE Orbit.
 * No headers, symbols or module dependencies from Epic Mover are required to load Orbit.
 * Applies overrides during pre-physics game-thread ticking, with a tick prerequisite
 * when the Mover component ticks in the same group. This is NOT a rollback-safe hook.
 */
UCLASS(ClassGroup=(Orbit), BlueprintType, meta=(BlueprintSpawnableComponent))
class ORBITMOVER_API UOrbitMoverGravityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UOrbitMoverGravityComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Orbit|Mover")
    bool bEnableOrbitGravity = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Orbit|Mover|Diagnostics")
    bool bInsideOrbitField = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Orbit|Mover|Diagnostics")
    FVector LastOrbitAcceleration = FVector::ZeroVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Orbit|Mover|Diagnostics")
    FVector LastOrbitUp = FVector::UpVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Orbit|Mover|Diagnostics")
    FVector LastSamplePosition = FVector::ZeroVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Orbit|Mover|Diagnostics")
    int32 SamplesApplied = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Orbit|Mover|Diagnostics")
    bool bBackendSupported = false;

    UFUNCTION(BlueprintCallable, Category="Orbit|Mover")
    void SetOrbitMoverGravityEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category="Orbit|Mover")
    void RefreshOrbitMoverGravity();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UActorComponent> MoverComponent = nullptr;

    bool bOwnsOverrides = false;
    bool bWarnedUnsupported = false;
    FVector LastStableUp = FVector::UpVector;

    void ReleaseOverrides();
    bool IsSupportedMover() const;
    void ApplyOrbitGravity();
    bool ApplyOverride(const FName FunctionName, const FName FlagName, const FName VectorName, bool bEnabled, const FVector& Value);
    bool ReadMoverVector(const FName FunctionName, FVector& OutValue) const;
    bool ReadMoverAsyncFlag(bool& bOutAsync) const;
};
