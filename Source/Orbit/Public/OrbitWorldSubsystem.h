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

    /** Retrieve measured, read-only runtime physics diagnostics for a single simulated primitive.
     *  Unlike transient viewport text, this is inspectable while paused in PIE.
     *  Requires that Orbit discovered and manages this primitive. */
    UFUNCTION(BlueprintPure, Category="Orbit|Physics|Diagnostics")
    bool GetPhysicsBodyDiagnostics(UPrimitiveComponent* Component, FOrbitPhysicsBodyDiagnostics& OutDiagnostics) const;

    /** PIE/development diagnostic recorder. Records one named body or a bounded snapshot of ALL
     *  currently Orbit-managed simulated primitives. Newly discovered bodies are not auto-enrolled.
     *  Console: Orbit.Motion.List, Orbit.Motion.Start <name> [seconds] [Hz],
     *  Orbit.Motion.StartAll [seconds] [Hz], Orbit.Motion.Stop, Orbit.Motion.Export [label],
     *  Orbit.Motion.Status. Game-thread observations, NOT Chaos contact manifolds/substeps. */
    TArray<FString> ListMotionCaptureBodies() const;
    bool StartMotionHistory(const FString& BodyName, double DurationSeconds = 20.0, int32 SamplesPerSecond = 60);
    bool StartAllMotionHistory(double DurationSeconds = 20.0, int32 SamplesPerSecond = 60);
    void StopMotionHistory();
    bool ExportMotionHistory(const FString& Label = FString(), FString* OutPath = nullptr) const;
    FString GetMotionHistoryStatus() const;

    /** World gravity fallback is returned when no custom source influences the point. */
    UFUNCTION(BlueprintPure, Category="Orbit|Gravity")
    FVector GetGravityAcceleration(const FVector& WorldLocation, EOrbitGravityChannel Channel = EOrbitGravityChannel::General) const;

    UFUNCTION(BlueprintPure, Category="Orbit|Gravity")
    int32 GetActiveSourceCount() const { return ActiveFields.Num(); }

private:
    /** Evidence from the same geometry sweep already used by the rest system.
     *  Capturing rejected candidates is observation-only, not another sweep. */
    struct FRestSupportProbe
    {
        bool bHit = false;
        bool bStartedPenetrating = false;
        FVector Normal = FVector::ZeroVector;
        FVector ImpactPoint = FVector::ZeroVector;
        TWeakObjectPtr<UPrimitiveComponent> SupportComponent;
        int32 BlockingCandidates = 0;
        int32 RejectedNormals = 0;
        double BestRejectedAlignment = -2.0; // -2 means no rejected candidate.
        FVector BestRejectedNormal = FVector::ZeroVector;
        FVector BestRejectedImpact = FVector::ZeroVector;
        bool bBestRejectedStartedPenetrating = false;
        TWeakObjectPtr<UPrimitiveComponent> BestRejectedComponent;
    };

    struct FManagedBody
    {
        TWeakObjectPtr<UPrimitiveComponent> Component;
        bool bOriginalGravity = false;
        float OriginalLinearDamping = 0.0f;
        float OriginalAngularDamping = 0.0f;
        bool bNativeContactDampingApplied = false;
        FOrbitPhysicsBodyDiagnostics LastDiagnostics;
        FVector LastAcceleration = FVector::ZeroVector;
        float StableSeconds = 0.0f;
        // Short tolerance for collision-query misses while maintaining gentle contact.
        // Never grants support to a body that has not actually had a valid hit.
        float SupportGraceRemaining = 0.0f;
        FVector LastSupportedLocation = FVector::ZeroVector;
        FVector LastSupportedUp = FVector::UpVector;
        FVector LastSupportNormal = FVector::UpVector;
        FVector LastSupportImpactPoint = FVector::ZeroVector;
        bool bLastSupportStartedPenetrating = false;
        TWeakObjectPtr<UPrimitiveComponent> LastSupportComponent;
        // Last tick's *actual* support query evidence. Reset for sleep/fast impacts.
        FRestSupportProbe LastProbe;
        bool bLastProbeAttempted = false;
        bool bLastFreshSupport = false;
        bool bLastGraceSupport = false;
        bool bLastRestCandidate = false;
        bool bLastGentleContact = false;
        double LastNormalSpeedCmS = 0.0;
        float LastProbeGraceSeconds = 0.0f;
    };

    struct FMotionHistoryRow
    {
        int32 BodyIndex = INDEX_NONE; // Frozen roster index, stable for this capture.
        double TimeSeconds = 0.0;
        FVector Position = FVector::ZeroVector;
        FVector CenterOfMass = FVector::ZeroVector;
        FVector DominantSourceCenter = FVector::ZeroVector;
        bool bHasDominantSourceCenter = false;
        FQuat Rotation = FQuat::Identity;
        FVector LinearVelocity = FVector::ZeroVector;
        FVector AngularVelocity = FVector::ZeroVector;
        FOrbitPhysicsBodyDiagnostics Diagnostics;
        FVector SupportNormal = FVector::ZeroVector;
        FVector SupportImpactPoint = FVector::ZeroVector;
        bool bSupportStartedPenetrating = false;
        FString SupportComponentName;
        FString SupportCollisionGeometry;
        bool bReversedTangentialMotion = false;
        // Explicit PIE time and sampled transitions for reproducible comparisons.
        double PieTimeSeconds = 0.0;
        bool bFirstVerifiedSupport = false;
        bool bFirstSleepTransition = false;
        double FirstVerifiedSupportPIETime = -1.0;
        double FirstSleepTransitionPIETime = -1.0;
        bool bSupportStateChanged = false;
        bool bAwakeStateChanged = false;
        bool bFreshSupport = false;
        bool bGraceSupport = false;
        bool bRestCandidate = false;
        bool bGentleContact = false;
        bool bProbeAttempted = false;
        int32 ProbeBlockingCandidates = 0;
        int32 ProbeRejectedNormals = 0;
        double BestRejectedAlignment = -2.0;
        FVector BestRejectedNormal = FVector::ZeroVector;
        FVector BestRejectedImpact = FVector::ZeroVector;
        bool bBestRejectedStartedPenetrating = false;
        FString BestRejectedComponentName;
        FString BestRejectedGeometry;
        double NormalSpeedCmS = 0.0;
        float GraceRemainingSeconds = 0.0f;
        float EffectiveLinearDamping = 0.0f;
        float EffectiveAngularDamping = 0.0f;
        FString ProbeOutcome;
        bool bConfiguredSupportedDamping = false;
        bool bConfiguredRestSleep = false;
        float ConfiguredLinearDamping = 0.0f;
        float ConfiguredAngularDamping = 0.0f;
        float ConfiguredProbeDistance = 0.0f;
        float ConfiguredMaxContactSpeed = 0.0f;
    };

    struct FMotionHistoryBody
    {
        TWeakObjectPtr<UPrimitiveComponent> Component;
        FString Path;
        FString OwnerName;
        FString OwnerLabel;
        FString MeshAssetPath;
        FString CollisionGeometry;
        double LastSampleTime = -1.0;
        FVector PreviousVelocity = FVector::ZeroVector;
        bool bHasPreviousVelocity = false;
        int32 SampleCount = 0;
        int32 ReversalCount = 0;
        FVector InitialPosition = FVector::ZeroVector;
        bool bInitiallyAwake = false;
        double InitialLinearSpeed = 0.0;
        double InitialAngularSpeed = 0.0;
        bool bHasPreviousAwake = false;
        bool bPreviousAwake = false;
        bool bHasPreviousSupport = false;
        FString PreviousSupportState;
        bool bSeenFreshSupport = false;
        double FirstVerifiedSupportPIETime = -1.0;
        double FirstSleepTransitionPIETime = -1.0;
    };

    // Snapshotted roster + constant-time component lookup. Stale pointers are never dereferenced.
    TArray<FMotionHistoryBody> MotionHistoryBodies;
    TMap<const UPrimitiveComponent*, int32> MotionHistoryBodyLookup;
    TArray<FMotionHistoryRow> MotionHistoryRows;
    bool bMotionHistoryAllBodies = false;
    int32 MotionHistoryRowLimit = 15002;
    double MotionHistoryStartTime = 0.0;
    double MotionHistoryDuration = 20.0;
    int32 MotionHistoryRateHz = 60;
    int32 MotionHistoryReversalCount = 0;
    bool bMotionHistoryRecording = false;
    mutable bool bMotionHistoryExported = false;
    FString MotionHistoryLastStopReason;

    TArray<TWeakObjectPtr<AOrbitGravitySource>> Sources;
    TArray<TWeakObjectPtr<UOrbitGravitySourceComponent>> SourceComponents;
    TArray<FOrbitFieldDefinition> ActiveFields;
    TArray<FManagedBody> ManagedBodies;
    float DiscoveryAccumulator = 0.0f;
    bool bNeedsInitialDiscovery = true;

    bool BeginMotionHistory(const TArray<UPrimitiveComponent*>& Targets, bool bAllBodies, double DurationSeconds, int32 SamplesPerSecond);
    void CaptureMotionHistory(UPrimitiveComponent* Component, const FManagedBody& Body);
    void FinishMotionHistory(const FString& Reason);
    static FString DescribeCollisionGeometry(const UPrimitiveComponent* Component);
    void UpdateSourceSnapshot();
    void DiscoverBodies();
    void ApplyPhysicsForces(float DeltaTime);
    FRestSupportProbe ProbeSupportAlongGravity(UPrimitiveComponent* Component, const FVector& Down, float ProbeDistance) const;
    void RestoreBodyGravity();
    void RestoreBodyDamping(FManagedBody& Body);
    bool HasManagedBody(const UPrimitiveComponent* Component) const;
    uint8 GetReceiverChannelMask(const AActor* Owner) const;
    bool HasCompatiblePhysicsSource(uint8 ReceiverMask) const;
};
