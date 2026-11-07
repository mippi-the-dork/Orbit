# Orbit — Native Chaos integration test suite

This is the maintained reference for the **live-world** test suite. For implementation and the latest test list, see [Iteration 041](ITERATION_041.md). Maintain this document when adding or changing automated gravity-integration tests.

## Why these tests are separate

`Orbit.Physics.GravityIntegration` validates math, compatibility-selection policy, numerical field evaluation, and regression invariants, but does **not** step actual Chaos. `Orbit.Physics.IntegrationWorld` constructs an isolated temporary Game world through Unreal's native `FTestWorldWrapper`, spawns actual physics bodies and Orbit source Actors, advances the world, and inspects body state. The difference matters: policy tests can pass while runtime solver ownership or collisions are broken.

## Commands to copy into Unreal Output Log

```text
Automation RunTests Orbit.Physics.GravityIntegration
```

Expected: 29 cases, previously user-verified in Iteration 040 Hotfix 1.

```text
Automation RunTests Orbit.Physics.IntegrationWorld
```

Expected: 9 cases, **not yet UE-verified**. Run them after compiling Orbit 0.2.30. Tests are intended to run with Project Settings left in Standard; the fixture temporarily controls and restores its own settings. Do not run simultaneously with PIE or another editor test suite that mutates the same developer-settings object.

## Important test fixture rules

- No external testing plugin or Probe integration.
- Game-world gravity is set to zero **in the test world only** to isolate Orbit field acceleration; it does not change a project setting or map.
- Real physics bodies use the built-in Engine Cube mesh; this is a built-in engine dependency rather than a new Orbit asset.
- All test worlds and bodies are transient. Test settings are restored on exit and never saved to disk.
- The tests assert measured runtime velocities and actual `GetPhysicsBodyDiagnostics()` mode flags. Any failed assertion is a real automated result, not a visual observation.
- No forced sleep or artificial damping is used to pass natural-sleep tests. The only manual-intervention-free sleep test waits for Chaos to put a supported cube to sleep.
- Keep real game runtime and the test module separate. The new test .cpp belongs to `OrbitEditor`, which is not compiled into packaged games.

## Report a failure

Please provide the first failed test, the exact assertion text, and any `LogOrbitWorld` / Chaos errors. If every test fails with "Create isolated game test world" or "FTestWorldWrapper failed", that indicates a **test-fixture initialization issue**, not automatically a production Orbit regression. Don't change production physics until the harness is validated.

## Remaining manual work

Only a short visual review of the source gizmos/preview shapes, Character/Mover gameplay, complex contact stability, and a Development/Shipping package run should remain after the nine world integration tests pass. Larger-scale performance analysis still requires Unreal Insights, because this suite deliberately uses small scenes and does not benchmark wall-clock frame time.
