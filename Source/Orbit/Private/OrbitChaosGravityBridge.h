#pragma once

#include "CoreMinimal.h"
#include "OrbitGravityTypes.h"

class UWorld;
class UPrimitiveComponent;

namespace Chaos { class FPhysicsSolverBase; }
struct FOrbitUniformGravitySimCallback;

/** Opt-in, per-world Chaos gravity group 7 bridge. Never constructed in Standard mode.
 *  Group 7 is solver-global; this first-stage experiment MUST be tested in a
 *  scene where no other system assigns particles to group 7. */
class FOrbitChaosGravityBridge final
{
public:
    static constexpr int32 GravityGroup = 7;
    static bool TryGetUniformField(const TArray<FOrbitFieldDefinition>& Fields, FVector& OutAcceleration);
    bool Start(UWorld* World);
    void Update(const FVector& Acceleration);
    void Stop();
    bool IsActive() const { return Callback != nullptr; }
    ~FOrbitChaosGravityBridge() { Stop(); }

    // Return true only if the group was assigned successfully to a valid body.
    bool AssignBody(UPrimitiveComponent* Component, int32& OutPreviousGroup);
    static void RestoreBody(UPrimitiveComponent* Component, int32 PreviousGroup);

private:
    Chaos::FPhysicsSolverBase* Solver = nullptr;
    FOrbitUniformGravitySimCallback* Callback = nullptr;
};
