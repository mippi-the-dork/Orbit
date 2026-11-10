# Orbit | Standard / Experimental Compatibility Inventory

**Living reference | v0.2.32, Iteration 043 | UE 5.8.x | Updated: November 8, 2026**

## Goal

One Orbit plugin, one authored field system, one set of public Blueprint nodes and C++ APIs. The Standard (`AddForce`) gravity-delivery path remains the default for existing and new projects. The opt-in Chaos method should eventually be compatible with all supported Standard physics behavior without duplicating user-facing authoring controls.

**Important:** A *source math / eligibility check* is not a verified runtime Chaos result. Status labels:

- **Shared:** Uses the same implementation regardless of gravity-delivery mode.
- **Experimental eligible:** Code permits solver ownership with a separate opt-in; UE 5.8.3 PIE verification still required for Iteration 040 capabilities.
- **Previously verified:** User-tested in UE 5.8.3 in the scenarios indicated.
- **Standard fallback:** Existing force method used; does not imply missing gravity or a failure.
- **Not implemented:** A feature or integration not in the current plugin; not a gravity-method parity regression.

## Source shape / physics-mode parity

| Standard authoring feature | Pre-040 Chaos method | 040 opt-in `Expanded Field Compatibility` | Status / remaining work |
|---|---|---|---|
| Single unlimited Constant Directional | Supported | Existing path takes precedence | Previously verified sleep/wake/resleep |
| Single ordinary Point, Constant / Linear / Inverse Square, finite or unlimited | Supported | Existing path takes precedence | Previously verified radial sleep, falloff |
| 2–32 ordinary Points | Separate Multiple Point opt-in | Existing path takes precedence when selected | Previously verified motion and sleep; scaling needs profiling |
| Mixed ordinary Point + unlimited Constant Directional | Separate Mixed opt-in | Existing path takes precedence when selected | Previously verified stationary and moving tests |
| Single/multiple Axis (inward/outward) | Standard fallback | Experimental eligible | NEW: test cylindrical field and runtime sleeping |
| Single/multiple Spline (inward/outward) | Standard fallback | Experimental eligible | NEW: validate orientation, curve sampling, performance |
| Spline Up / Spline Down | Standard fallback | Experimental eligible when CurveUp data matches | NEW: verify spline-aligned gravity |
| Box/Sphere/Cylinder/Capsule Volume | Standard fallback | Experimental eligible | NEW: verify local rotation and boundary handling |
| Finite/Linear/Inverse Square Directional | Standard fallback | Experimental eligible | NEW: verify attenuation and source boundaries |
| Rotated box-limited source | Standard fallback | Experimental eligible | NEW: verify same inside/outside classification |
| Mixed Axis/Spline/Volume/Point/Directional | Standard fallback | Experimental eligible up to 32 sources | NEW: use authored Additive / Override, priority and resolution |
| Vortex Inward / Outward; Spiral Capture / Continuous Force | Standard fallback | **Opt-in Iteration 042 Vortex Compatibility** | Point/Axis/Spline; live UE test pending, may not naturally sleep when steered |
| Field above 32 active physics sources | Standard | **Standard fallback** | Bounded first experiment; must never silently truncate |
| Source Channel 0 only | Supported | Experimental eligible for rigid body receiver on Channel 0 only | NEW shape coverage |
| Source or Receiver Channels 1/2 or multichannel masks | Supported | **Opt-in Iteration 042 Receiver Channel Compatibility** | Per-Chaos-particle UniqueIdx masks, live UE test pending |
| Nonphysics source (`Affect Physics = Off`) | Ignored by body physics | Ignored by body physics | Shared filtering |

**Eligibility:** All participating physics sources must pass validation. Malformed vectors, missing spline samples, unsupported direction/shape combinations and unsupported channels fall back as a whole; don't drop a source silently. The field evaluator remains the single authority for source geometry, finite ranges, falloff and combination. The new selection has *lower priority* than every previously available experimental path so earlier opted-in scenes select precisely their previous path.

## Components and rigid-body ownership

| Standard feature | Experimental status | Next validation |
|---|---|---|
| Auto-managed ordinary simulated Static Mesh bodies with original native gravity enabled | Previously verified for Point/Directional; newly eligible expanded fields | Test unusual simple/convex collisions, sleeping, switching |
| Source Actor (`AOrbitGravitySource`) | Shared snapshot source | Test all shapes in PIE |
| Source Component (`UOrbitGravitySourceComponent`) | Shared snapshot source | Test moving / nested / runtime-created components |
| Gravity Receiver on Actor | Shared for field queries; opt-in channel-aware multi-source solver | Per-receiver isolation and mutable masks await UE integration-test results |
| `Orbit.Ignore` / original native-gravity-disabled bodies | Existing protection retained | Test fallback and cleanup |
| Runtime-spawned bodies | Existing periodic discovery | First physics step still may use native World Gravity |
| Pre-existing bodies before BeginPlay | Early-ownership separate default-off opt-in | Regression test expanded fields and no first-Z impulse |
| Skeletal / articulated physics or compound body group assignment | Standard fallback | Need multi-particle safe body ownership, no partial group assignment |
| Alternate actor/component simulation types | Usually Standard; depends on automatic body discovery | Inventory and live tests by component class |
| Chaos group 7 conflict | Solver refuses ownership | No stealing other integrations' groups |
| Original gravity/group/damping restore | Existing cleanup path shared | Runtime PIE test for mode switch and teardown |
| Physics Rest / Supported Damping | Existing settings unchanged | Legacy systems still available; no silent removal |

## Blueprint nodes, helpers, movement and Editor tools

**No duplicate Experimental nodes should be created.** The gravity-method selector controls how managed physics bodies receive acceleration; other consumers use the same public field-sampling and animation APIs.

| Surface | Parity assessment | Verification still required |
|---|---|---|
| `Orbit World Subsystem` gravity queries: `SampleGravity`, `SampleGravityWithChannels`, `SampleGravityForActor`, `GetGravityAcceleration`, `GetActiveSourceCount`, `RefreshActiveSources` | Shared field evaluator | Compare sample to body diagnostic in complex scenarios; physics-thread timing can differ |
| Blueprint Library gravity queries: `SampleOrbitGravity`, `SampleOrbitGravityForActor` | Shared | Channel-specific parity test |
| Alignment / frame / orientation Blueprint nodes | Shared; independent of rigid-body force delivery | Test on Axis, spline, rotating volume |
| Gravity Receiver queries + diagnostics | Shared; new `bExperimentalChaosExpandedFieldGravity` appended | Inspect correct method flag and sample |
| Actor + Component source property authoring and visualization | Shared | Runtime source geometry and updates |
| Character Gravity + native CharacterMovement | Shared field evaluation, separate character movement implementation | Regression on all shapes; not automatically made Chaos-rigidbody-driven |
| Character Controls, camera alignment, input helpers | Shared; independent of rigid-body method | Basic movement and gravity-relative look |
| Gravity Animation Blueprint nodes and native AnimInstance | Shared; derived from character orientation / velocity | Retest directional-to-spline transitions |
| OrbitMover module (optional Mover adapter) | Shared field queries, not Chaos body ownership | Mover packaging, async/rollback still unsupported |
| Editor source tools / debug streams | Shared | Source visualization matches runtime shape |
| Motion History & receiver physics diagnostics | Shared; 127th column added | Verify mutually exclusive five solver flags and CSV format |
| Packaging / runtime modules | Same plugin and source files | Prior user-reported packaged-game success; retest after new solver code |

## Live validation matrix / next passes

**Iteration 040 (this release):** eligible position-based shape expansion, a single default-off opt-in, seven native policy/math tests (29 total Gravity Integration), appended method diagnostic and living parity inventory. **UE build, automation, PIE and packaged-game validation of 040 are still pending.**

**Next: receiver channels and multiple source components.** Transfer per-body receiver mask safely to the physics thread, preserve channel masks 0/1/2 and combinations, prevent stale mask assignment and partial force application. Add physics-thread change tests.

**Iteration 042: velocity-sensitive Vortex modes authored (UE validation pending).** Pass the actual Chaos substep velocity into the existing velocity-aware evaluator; test Spiral Capture and Continuous Force with impulse/awake/sleep behavior and performance. Do not enable merely because position-only fields work.

**Next: body classes / lifetime.** Skeletal articulated and non-static primitives, spawned/streamed bodies, multiple simulated components, runtime source deletion, shared group conflicts, physics ownership switches, teardown and networking limitations.

**Next: per-feature runtime parity and release gate.** Full source-shape × body-type × source-type × channel × authoring-change matrix, performance/Insights, Development + Shipping packaging, Standalone Game / PIE, world transition, source replication caveats. Only after this work can we consider making Chaos the recommended default. Standard remains available for backward compatibility.

## User-facing rollback

Project Settings > Plugins > Orbit > Physics > Gravity Integration > `Rigid Body Gravity Application = Standard (Existing AddForce)`. Restart PIE or the packaged game. No asset migration, duplicated plugin, or replacement of Blueprint nodes is required.

## Implementation anchors

- `Source/Orbit/Private/OrbitGravityTypes.cpp` -- authoritative field geometry, falloff and combination.
- `Source/Orbit/Private/OrbitChaosGravityBridge.cpp` -- per-body solver callback and source eligibility.
- `Source/Orbit/Private/OrbitWorldSubsystem.cpp` -- ownership selection, preclaim, discovery, wake and fallback.
- `Source/Orbit/Private/OrbitPhysicsIntegrationMath.h` -- mode-selection policy.
- `Source/Orbit/Private/Tests/OrbitChaosGravityTests.cpp` -- native regression tests.
- [`EXPERIMENTAL_GRAVITY.md`](EXPERIMENTAL_GRAVITY.md) -- maintained how-to and validation evidence.

## Iteration 041 automation coverage

A new native Editor-only world simulation suite now covers live Axis, Spline Up/Down, four Volume boundaries, mixed Axis + Directional priority, Vortex fallback, source relocation/destruction, nondefault receiver-channel fallback, and natural Chaos sleep/wake on static support. It runs with temporary Game worlds and project settings restored afterward. **All 9 tests passed in user UE5.8.3 on Iteration 041 Hotfix 2.** `Orbit.Physics.GravityIntegration` still holds the previously user-verified 29 math/policy tests. See [Integration Testing](INTEGRATION_TESTING.md). There is no change to the Standard default, gravity sources or Blueprint nodes.

## Iteration 042 parity delta (pending UE 5.8.3 validation)

- New `Experimental: Receiver Channel Compatibility`, default **Off**. Explicitly opts in to a per-body channel-aware solver selection, including source channels 0,1,2 and compound receiver masks. This mode deliberately takes precedence over legacy single/Point/expanded selectors when enabled so uniform fields cannot bypass per-body mask handling. All channel masks are passed as copied Chaos-particle identity snapshots, never looked up from game-thread UObjects during a Chaos substep. Missing mapping is **0 acceleration** until discovered, never channel 0.
- New `Experimental: Vortex Compatibility`, default **Off**. Valid Point, Axis and Spline Vortex Continuous and Spiral Capture use the shared evaluator with actual physics-substep particle velocity. Unsupported source types/configurations remain Standard. Combined vortex+channel requires both opt-ins.
- Source masking, runtime `UOrbitGravityReceiverComponent` mutation, mode fallback and cleanup now have dedicated native regression coverage. 34 math/policy plus 14 live Chaos world tests are authored, **not yet executed** on UE5.8.3.
- The current experimental callback is still limited to a **single StaticMesh physics body per component**, ≤32 source fields, and post-BeginPlay discovery for dynamically spawned objects. It does not yet guarantee first-step gravity for runtime-spawned bodies. Articulated/skeletal or other compound multi-particle bodies remain on Standard. No back-compat defaults changed.
- The existing 127-column CSV remains unchanged. `experimental_chaos_expanded_field_gravity` indicates that the expanded per-particle evaluator is active for channel and Vortex paths; it does **not** distinguish the two options.

## Iteration 043 runtime-spawn update

An explicit `PrepareSpawnedPhysicsBody` call is available for fully configured runtime-spawned simulated Static Mesh components. It attempts immediate experimental ownership and commits channel masks without waiting for periodic discovery. **Automatic first-step ownership, skeletal/articulated particles, and full multi-body parity are not yet complete.** See [Iteration 043](ITERATION_043.md). Iteration 042: all 48 tests were reported passing by the user in UE 5.8.3; Iteration 043: 2 additional live tests authored, not yet run.

## Iteration 044 — Actor multi-component explicit preparation

Blueprint-callable `PrepareSpawnedPhysicsActor` prepares independent simulated Static Mesh components belonging to a single Actor before the next world step when invoked at the correct point in spawn setup. It does not provide automatic spawn interception, nor skeletal/articulated multi-particle ownership. See [Iteration 044](ITERATION_044.md). Iteration 043 tests: user-validated 50/50; Iteration 044 compilation and two new tests pending.
