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
        const TArray<FOrbitFieldDefinition>& Fields, TArray<FOrbitFieldDefinition>& OutFields);
    // Multi-source callback currently supports only default channel-0 receivers.
    static bool CanAssignMultiPointReceiver(bool bMultiPointMode, uint8 ReceiverMask)
    {
        return !bMultiPointMode || ReceiverMask == 1u;
    }
    bool Start(UWorld* World);
    void Update(const FOrbitFieldDefinition& Field, const FVector& UniformAcceleration, bool bPerParticlePoint);
    void UpdateMultiplePoint(const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution);
    void UpdateMixedPointDirectional(const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution);
    void UpdateExpandedFields(const TArray<FOrbitFieldDefinition>& Fields, EOrbitOverrideResolution Resolution);
    void Stop();
    bool IsActive() const { return Callback != nullptr; }
    bool IsPointMode() const { return bPointMode; }
    bool IsMultiPointMode() const { return bMultiPointMode; }
    bool IsMixedPointDirectionalMode() const { return bMixedPointDirectionalMode; }
    bool IsExpandedFieldsMode() const { return bExpandedFieldsMode; }
    bool IsMultiSourceMode() const { return bMultiPointMode || bMixedPointDirectionalMode || bExpandedFieldsMode; }
    ~FOrbitChaosGravityBridge() { Stop(); }

    // Return true only if the group was assigned successfully to a valid body.
    bool AssignBody(UPrimitiveComponent* Component, int32& OutPreviousGroup);
    static void RestoreBody(UPrimitiveComponent* Component, int32 PreviousGroup);

private:
    Chaos::FPhysicsSolverBase* Solver = nullptr;
    FOrbitUniformGravitySimCallback* Callback = nullptr;
    bool bPointMode = false;
    bool bMultiPointMode = false;
    bool bMixedPointDirectionalMode = false;
    bool bExpandedFieldsMode = false;
};
