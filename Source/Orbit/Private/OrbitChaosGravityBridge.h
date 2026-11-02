#pragma once

#include "CoreMinimal.h"
#include "OrbitGravityTypes.h"

class UWorld;
class UPrimitiveComponent;

namespace Chaos { class FPhysicsSolverBase; }
struct FOrbitUniformGravitySimCallback;

/** Opt-in, per-world Chaos gravity group 7 bridge. Never constructed in Standard mode.
 *  Uniform Directional and single Point fields are supported. The Point variant
 *  applies separate gravity acceleration to each marked particle on each
 *  physics substep, without AddForce / dirty-property wake side effects.
 *  Group 7 is solver-global and must not be assigned by another integration. */
class FOrbitChaosGravityBridge final
{
public:
    static constexpr int32 GravityGroup = 7;
    static bool TryGetUniformField(const TArray<FOrbitFieldDefinition>& Fields, FVector& OutAcceleration);
    /** The existing uniform mode OR one Point source evaluated individually per rigid body. */
    static bool TryGetSolverField(const TArray<FOrbitFieldDefinition>& Fields,
        FOrbitFieldDefinition& OutField, FVector& OutUniformAcceleration, bool& bOutPerParticlePoint);
    bool Start(UWorld* World);
    void Update(const FOrbitFieldDefinition& Field, const FVector& UniformAcceleration, bool bPerParticlePoint);
    void Stop();
    bool IsActive() const { return Callback != nullptr; }
    bool IsPointMode() const { return bPointMode; }
    ~FOrbitChaosGravityBridge() { Stop(); }

    // Return true only if the group was assigned successfully to a valid body.
    bool AssignBody(UPrimitiveComponent* Component, int32& OutPreviousGroup);
    static void RestoreBody(UPrimitiveComponent* Component, int32 PreviousGroup);

private:
    Chaos::FPhysicsSolverBase* Solver = nullptr;
    FOrbitUniformGravitySimCallback* Callback = nullptr;
    bool bPointMode = false;
};
