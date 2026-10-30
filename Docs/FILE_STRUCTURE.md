# Orbit File Structure (0.2.15)

**One installable Unreal plugin**, containing three internal modules:

```text
Orbit/
  Orbit.uplugin
  README.md
  CHANGELOG.md
  Config/
    FilterPlugin.ini
  Resources/
    Icon128.png
    Icons/
      Orbit-Icon.svg
      OrbitActor_16.svg
      OrbitAsset_64.svg
      OrbitBillboard.png
  Docs/
    ARCHITECTURE.md
    COMPATIBILITY.md
    COMPONENTS_AND_BLUEPRINT_API.md
    GRAVITY_SOURCE_ACTOR.md
    MOVER_INTEGRATION.md
    ANIMATION_INTEGRATION.md
    ITERATION_021.md
    ...earlier iteration guides and test plan...
  Source/
    Orbit/
      Orbit.Build.cs
      Public/
      Private/
        Tests/
    OrbitEditor/
      OrbitEditor.Build.cs
      Private/
    OrbitMover/
      OrbitMover.Build.cs
      Public/
      Private/
        Tests/
```

The module `OrbitMover` is packaged INSIDE `Orbit` and is not a separate `.uplugin`. It does not link or include Epic Mover. Epic Mover is optional at runtime, and can be enabled in the project when needed. All source is intended for UE 5.8.3. No engine source changes.

## Iteration 022 additions

- `Source/Orbit/Public/OrbitGravityAnimInstance.h`
- `Source/Orbit/Private/OrbitGravityAnimInstance.cpp`
- `Docs/Examples/ABP_UNARMED_ORBIT_VERIFIED_WORKFLOW.md`
- `Docs/ITERATION_022.md`

The pure gravity-relative vertical-speed function resides in existing `OrbitGravityFrame` and `OrbitBlueprintLibrary` source files. The single `Orbit.uplugin` is currently version 0.2.17 (Iteration 028).


### Iteration 025 additions

- `OrbitGravityTypes.h`: reflected `FOrbitPhysicsBodyDiagnostics` read-only runtime diagnostic struct.
- `OrbitGravityReceiverComponent.h`: `LastPhysicsBodyDiagnostics` for the most recently sampled simulating primitive on its Actor.
- `OrbitWorldSubsystem.h/.cpp`: per-body native damping snapshots/restoration and `GetPhysicsBodyDiagnostics` per-primitive Blueprint accessor.
- `OrbitPhysicsRestMath.h`: native damping clamp helper.
- `Docs/ITERATION_025.md`: isolation checklist and limitations.

### Iteration 026 additions

- `Source/Orbit/Private/OrbitMotionHistory.cpp`: per-body bounded game-thread motion capture, CSV export and non-Shipping console commands.
- `Source/Orbit/Private/OrbitMotionHistoryMath.h`: pure math/CSV helpers with regression tests in `OrbitFieldTests.cpp`.
- `Source/Orbit/Public/OrbitWorldSubsystem.h`, `Private/OrbitWorldSubsystem.cpp`: recorder storage and hooks inside the existing body update, including sleep and support-query metadata.
- `Docs/PHYSICS_MOTION_HISTORY.md`: maintained console usage and constraints.
- `Docs/ITERATION_026.md`: detailed test plan and interpretation.

The runtime plugin still contains the same **three modules** (`Orbit`, `OrbitEditor`, `OrbitMover`); no separate Mover plugin and no engine changes.

### Iterations 027–028: whole-scene physics diagnostics

- `OrbitMotionHistory.cpp`: `Orbit.Motion.StartAll`, deterministic 256-body roster, independent per-body samples and 250,000-row bounded streamed CSV export.
- `OrbitWorldSubsystem.cpp`: evidence from the *existing* swept collision support test, including accepted and rejected normals, plus sleeping-body read-only diagnostic refresh.
- `OrbitMotionHistoryMath.h` and `OrbitFieldTests.cpp`: current velocity, probe outcome, first observed event and bounded-capture tests using Unreal's native Automation Framework.
- `Docs/PHYSICS_MOTION_HISTORY.md`: 95-column CSV schema and usage.
- `Docs/ITERATION_027.md` and `Docs/ITERATION_028.md`: per-iteration testing and limits.
