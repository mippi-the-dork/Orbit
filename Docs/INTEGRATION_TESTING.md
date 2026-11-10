# Orbit — Native Chaos integration test suite

This is the maintained reference for the **live-world** test suite. For implementation and the latest test list, see [Iteration 041](ITERATION_041.md). Maintain this document when adding or changing automated gravity-integration tests.

## Why these tests are separate

`Orbit.Physics.GravityIntegration` validates math, compatibility-selection policy, numerical field evaluation, and regression invariants, but does **not** step actual Chaos. `Orbit.Physics.IntegrationWorld` constructs an isolated temporary Game world through Unreal's native `FTestWorldWrapper`, spawns actual physics bodies and Orbit source Actors, advances the world, and inspects body state. The difference matters: policy tests can pass while runtime solver ownership or collisions are broken.

## Commands to copy into Unreal Output Log

```text
Automation RunTests Orbit.Physics.GravityIntegration
```

Expected: **34 cases** in Iteration 042 (29 previously verified + 5 new policy/field tests awaiting UE validation).

```text
Automation RunTests Orbit.Physics.IntegrationWorld
```

Expected: **14 cases** in Iteration 042 (9 user-verified in Iteration 041 Hotfix 2 + 5 new cases awaiting UE validation). Run them after compiling Orbit 0.2.31. Tests are intended to run with Project Settings left in Standard; the fixture temporarily controls and restores its own settings. Do not run simultaneously with PIE or another editor test suite that mutates the same developer-settings object.

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

## Iteration 041 Hotfix 2: Directional sleep/wake fixture correction

The first actual UE 5.8.3 integration suite passed eight of nine live-world tests. The only failure was the Directional sleep/wake fixture, which attempted a 90° **yaw** rotation after natural sleep. Orbit Directional gravity follows the source's **negative Up** vector, so yaw did not change gravity. The fixture now uses a 90° **pitch** rotation, checks that the authored and sampled gravity vectors are horizontal, then verifies wake and velocity toward the newly tilted gravity. No production code was modified; this specific retest remains pending.

## Iteration 042 — Additional native automation

User validation established **29/29** policy and **9/9** live-world tests successful for Iteration 041 Hotfix 2. Iteration 042 retains these and adds:

**Five field / selection tests:** `ChannelMaskOptInEligibility`, `ChannelMaskCombinationIsolation`, `ChannelAndVortexSelectionPriority`, `VortexExpandedOptInEligibility`, `VortexSpiralCaptureVelocityParity`.

**Five live-world tests:** `ChannelsIndependentBodiesAndUnion` (two independent receiver channels, one combined receiver, unmatched native-gravity body), `ChannelsRuntimeReceiverMaskChange`, `VortexContinuousForceChaos`, `VortexSpiralCaptureVelocityResponse`, `RuntimeSpawnedChannelReceiverDiscovery` (discovery, **not** first-substep ownership).

Commands (Unreal Editor Output Log, one at a time):

```text
Automation RunTests Orbit.Physics.GravityIntegration
Automation RunTests Orbit.Physics.IntegrationWorld
```

Expected new totals are **34** and **14**. **Do not call the new cases passed until they actually run in UE 5.8.3**. Tests use temporary game worlds, mutate only transient `UOrbitSettings` and restore them, save no assets, and do not require an external test plugin. Leave the user's Project Settings on Standard.

## Iteration 043 (v0.2.32) — explicit runtime claim

User-validated Iteration 042 baseline: 34 Gravity Integration and 14 live Chaos tests passed on UE 5.8.3. Two additional native live Chaos tests are authored in Iteration 043 and **have not been run or compiled in Unreal**:

- `Orbit.Physics.IntegrationWorld.ExplicitSpawnOwnershipBeforeFirstStep`: explicit call must synchronously register an eligible runtime-spawned rigid body before ticking; repeated call is safe; the body then accelerates under Chaos.
- `Orbit.Physics.IntegrationWorld.ExplicitSpawnChannelIsolation`: only the matching receiver channel may be explicitly claimed, and an unmatched body must not acquire that source's acceleration.

Run `Automation RunTests Orbit.Physics.GravityIntegration` (34 expected) and `Automation RunTests Orbit.Physics.IntegrationWorld` (16 expected). Explicit preparation requires the existing experimental early-ownership opt-in and cannot be interpreted as automatic first-step ownership for all spawned physics bodies.

## Iteration 044 (v0.2.33) — Actor multi-component explicit claim

User-validated Iteration 043: **34/34 Gravity Integration and 16/16 live Chaos** tests succeeded in Unreal. Iteration 044 adds two authored live-world tests, `ActorMultipleBodiesExplicitOwnership` and `ActorPreparationEligibility`; Unreal compilation/execution not yet verified. Expected total **34+18=52**. See [ITERATION_044.md](ITERATION_044.md).


## Iteration 045

Run `Automation RunTests Orbit.Physics.GravityIntegration` (34 expected) and `Automation RunTests Orbit.Physics.IntegrationWorld` (20 expected). New live tests: `SimulationToggleOwnershipLifecycle` and `RecreatedParticleIdentity`. See [ITERATION_045.md](ITERATION_045.md).
