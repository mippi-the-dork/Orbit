#pragma once

#include "CoreMinimal.h"
#include "OrbitGravityTypes.h"

class UWorld;
class UPrimitiveComponent;

namespace Chaos { class FPhysicsSolverBase; }
struct FOrbitUniformGravitySimCallback;

/** Opt-in, per-world Chaos gravity group 7 bridge. Never constructed in Standard mode.
 *  Supports Uniform Directional, single Point, and a separately opted-in limited
 *  multiple-Point source collection. Point variants compute per-body acceleration
 *  each physics substep, without AddForce / dirty-property wake side effects.
 *  Group 7 is solver-global and must not be assigned by another integration. */
class FOrbitChaosGravityBridge final
{
public:
    static constexpr int32 GravityGroup = 7;
    static bool TryGetUniformField(const TArray<FOrbitFieldDefinition>& Fields, FVector& OutAcceleration);
    /** The existing uniform mode OR one Point source evaluated individually per rigid body. */
    static bool TryGetSolverField(const TArray<FOrbitFieldDefinition>& Fields,
        FOrbitFieldDefinition& OutField, FVector& OutUniformAcceleration, bool& bOutPerParticlePoint);
    // Deliberately separate from TryGetSolverField: existing experimental
    // projects keep the single-source-only policy until this option is enabled.
    static constexpr int32 MaxExperimentalPointSources = 32;
    static bool TryGetMultiplePointFields(const TArray<FOrbitFieldDefinition>& Fields,
        TArray<FOrbitFieldDefinition>& OutFields);
    // At least one ordinary Point and one ordinary constant, unlimited
    // Directional source. Separate opt-in: never changes the multi-Point mode.
    static bool TryGetMixedPointDirectionalFields(
        const TArray<FOrbitFieldDefinition>& Fields, TArray<FOrbitFieldDefinition>& OutFields);
    /** Expanded, opt-in position-based field selection. Existing selectors run first. */
    static bool TryGetExpandedFields(
        const TArray<FOrbitFieldDefinition>& Fields, TArray<FOrbitFieldDefinition>& OutFields,
        bool bAllowChannels = false, bool bAllowVortex = false);
    static bool TryGetVortexFields(const TArray<FOrbitFieldDefinition>& Fields,
        TArray<FOrbitFieldDefinition>& OutFields, bool bAllowChannels = false);
    // Legacy multi-source paths are Channel-0 only. Opted-in channel mode
    // carries a per-particle mask; every other mode preserves its old policy.
    static bool CanAssignMultiPointReceiver(bool bMultiPointMode, uint8 ReceiverMask,
        bool bChannelAware = false)
    {
        return bChannelAware ? (ReceiverMask & 0x07u) != 0u :
            (!bMultiPointMode || ReceiverMask == 1u);
    }
    bool Start(UWorld* World);
    void Update(const FOrbitFieldDefinition& Field, const FVector& UniformAcceleration, bool bPerParticlePoint);
    void UpdateMultiplePoint(const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution);
    void UpdateMixedPointDirectional(const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution);
    void UpdateExpandedFields(const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution);
    void UpdateChannelFields(const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution);
    void UpdateVortexFields(const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution);
    // Unique Chaos particle identity is stable across GT/PT copies. Never
    // identify a receiver by position, actor order or shared gravity group.
    void SetBodyReceiverMask(int32 ParticleId, uint8 ReceiverMask);
    void RemoveBodyReceiverMask(int32 ParticleId);
    void CommitReceiverMasks();
    void Stop();
    bool IsActive() const { return Callback != nullptr; }
    bool IsPointMode() const { return bPointMode; }
    bool IsMultiPointMode() const { return bMultiPointMode; }
    bool IsMixedPointDirectionalMode() const { return bMixedPointDirectionalMode; }
    bool IsExpandedFieldsMode() const { return bExpandedFieldsMode; }
    bool IsChannelAwareMode() const { return bChannelAwareMode; }
    bool IsVortexMode() const { return bVortexMode; }
    bool IsMultiSourceMode() const { return bMultiPointMode || bMixedPointDirectionalMode || bExpandedFieldsMode || bChannelAwareMode || bVortexMode; }
    ~FOrbitChaosGravityBridge() { Stop(); }

    // Return true only if the group was assigned successfully to a valid body.
    bool AssignBody(UPrimitiveComponent* Component, int32& OutPreviousGroup,
        int32& OutParticleId, uint8 ReceiverMask);
    static void RestoreBody(UPrimitiveComponent* Component, int32 PreviousGroup);

private:
    Chaos::FPhysicsSolverBase* Solver = nullptr;
    FOrbitUniformGravitySimCallback* Callback = nullptr;
    bool bPointMode = false;
    bool bMultiPointMode = false;
    bool bMixedPointDirectionalMode = false;
    bool bExpandedFieldsMode = false;
    bool bChannelAwareMode = false;
    bool bVortexMode = false;
    TMap<int32, uint8> ReceiverMasksByParticleId;
};
