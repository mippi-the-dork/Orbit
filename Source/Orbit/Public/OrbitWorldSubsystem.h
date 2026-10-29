#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OrbitGravityTypes.h"
#include "OrbitWorldSubsystem.generated.h"

class AOrbitGravitySource;
class UOrbitGravitySourceComponent;
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

    /** Force a synchronous rebuild of all registered sources. Runtime Blueprint changes to
     *  source properties are normally visible by the next world tick; this forces them visible
     *  to queries later in the CURRENT tick. Call after editing many properties at once. */
    UFUNCTION(BlueprintCallable, Category="Orbit|Gravity|Runtime")
    void RefreshActiveSources();

    void RegisterSource(AOrbitGravitySource* Source);
    void UnregisterSource(AOrbitGravitySource* Source);
    void RegisterSourceComponent(UOrbitGravitySourceComponent* Component);
    void UnregisterSourceComponent(UOrbitGravitySourceComponent* Component);

    /** Stateful consumers can supply the previous winning source to avoid midpoint flip-flop. */
    FOrbitGravitySample SampleGravityForActorStable(const AActor* Actor, const FVector& WorldLocation,
        EOrbitGravityChannel Channel, int32 PreferredSourceId, double Hysteresis) const;

    /** Source-specific sampling for alignment or authored gravity receivers. */
    FOrbitGravitySample SampleIndividualSource(const UObject* Source, const FVector& WorldLocation,
        EOrbitGravityChannel Channel, uint8 ReceiverMask) const;

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
        // Short tolerance for collision-query misses while maintaining gentle contact.
        // Never grants support to a body that has not actually had a valid hit.
        float SupportGraceRemaining = 0.0f;
        FVector LastSupportedLocation = FVector::ZeroVector;
        FVector LastSupportedUp = FVector::UpVector;
        FVector LastSupportNormal = FVector::UpVector;
        TWeakObjectPtr<UPrimitiveComponent> LastSupportComponent;
    };

    TArray<TWeakObjectPtr<AOrbitGravitySource>> Sources;
    TArray<TWeakObjectPtr<UOrbitGravitySourceComponent>> SourceComponents;
    TArray<FOrbitFieldDefinition> ActiveFields;
    TArray<FManagedBody> ManagedBodies;
    float DiscoveryAccumulator = 0.0f;
    bool bNeedsInitialDiscovery = true;

    struct FRestSupportProbe
    {
        bool bHit = false;
        bool bStartedPenetrating = false;
        FVector Normal = FVector::ZeroVector;
        FVector ImpactPoint = FVector::ZeroVector;
        TWeakObjectPtr<UPrimitiveComponent> SupportComponent;
    };

    void UpdateSourceSnapshot();
    void DiscoverBodies();
    void ApplyPhysicsForces(float DeltaTime);
    FRestSupportProbe ProbeSupportAlongGravity(UPrimitiveComponent* Component, const FVector& Down, float ProbeDistance) const;
    void RestoreBodyGravity();
    bool HasManagedBody(const UPrimitiveComponent* Component) const;
    uint8 GetReceiverChannelMask(const AActor* Owner) const;
    bool HasCompatiblePhysicsSource(uint8 ReceiverMask) const;
};
