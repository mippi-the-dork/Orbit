#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OrbitGravityTypes.h"
#include "OrbitWorldSubsystem.generated.h"

class AOrbitGravitySource;
class AActor;
class UPrimitiveComponent;

/** A single gravity authority per runtime world. Existing sources register automatically in BeginPlay. */
UCLASS()
class ORBIT_API UOrbitWorldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void RegisterSource(AOrbitGravitySource* Source);
    void UnregisterSource(AOrbitGravitySource* Source);

    /** The field at an arbitrary world position. No gravity vector is ever normalized unless nonzero. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity")
    FOrbitGravitySample SampleGravity(const FVector& WorldLocation, EOrbitGravityChannel Channel = EOrbitGravityChannel::General) const;

    /** Explicit source/receiver mask query. Default mask is channel 0 only. */
    UFUNCTION(BlueprintPure, Category="Orbit|Channels")
    FOrbitGravitySample SampleGravityWithChannels(const FVector& WorldLocation, FOrbitGravityChannels ReceiverChannels, EOrbitGravityChannel Channel = EOrbitGravityChannel::General) const;

    /** Automatically uses an actor's optional Orbit Gravity Receiver, otherwise channel 0. */
    UFUNCTION(BlueprintPure, Category="Orbit|Channels")
    FOrbitGravitySample SampleGravityForActor(const AActor* Actor, const FVector& WorldLocation, EOrbitGravityChannel Channel = EOrbitGravityChannel::General) const;

    /** Useful for simulating bodies; respects the owning actor's Receiver. */
    FOrbitGravitySample SampleGravityForComponent(const UPrimitiveComponent* Component, EOrbitGravityChannel Channel) const;

    /** World gravity fallback is returned when no custom source influences the point. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity")
    FVector GetGravityAcceleration(const FVector& WorldLocation, EOrbitGravityChannel Channel = EOrbitGravityChannel::General) const;

    UFUNCTION(BlueprintPure, Category="Orbit|Gravity")
    int32 GetActiveSourceCount() const { return ActiveFields.Num(); }

private:
    struct FManagedBody
    {
        TWeakObjectPtr<UPrimitiveComponent> Component;
        bool bOriginalGravity = false;
        FVector LastAcceleration = FVector::ZeroVector;
        float StableSeconds = 0.0f;
    };

    TArray<TWeakObjectPtr<AOrbitGravitySource>> Sources;
    TArray<FOrbitFieldDefinition> ActiveFields;
    TArray<FManagedBody> ManagedBodies;
    float DiscoveryAccumulator = 0.0f;
    bool bNeedsInitialDiscovery = true;

    void UpdateSourceSnapshot();
    void DiscoverBodies();
    void ApplyPhysicsForces(float DeltaTime);
    bool IsSupportedAlongGravity(const UPrimitiveComponent* Component, const FVector& Down) const;
    void RestoreBodyGravity();
    bool HasManagedBody(const UPrimitiveComponent* Component) const;
    uint8 GetReceiverChannelMask(const AActor* Owner) const;
    bool HasCompatiblePhysicsSource(uint8 ReceiverMask) const;
};
