# Orbit | Standard / Experimental Compatibility Inventory

**Living reference | v0.2.29, Iteration 040 | UE 5.8.x | Last updated: November 2026**

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
| Vortex Inward / Outward; Spiral Capture / Continuous Force | Standard fallback | **Standard fallback** | Needs velocity-dependent substep parity and native sleep assessment |
| Field above 32 active physics sources | Standard | **Standard fallback** | Bounded first experiment; must never silently truncate |
| Source Channel 0 only | Supported | Experimental eligible for rigid body receiver on Channel 0 only | NEW shape coverage |
| Source or Receiver Channels 1/2 or multichannel masks | Supported | **Standard fallback** for expanded/multi-source path | Requires per-body physics-thread receiver mask ownership |
| Nonphysics source (`Affect Physics = Off`) | Ignored by body physics | Ignored by body physics | Shared filtering |

**Eligibility:** All participating physics sources must pass validation. Malformed vectors, missing spline samples, unsupported direction/shape combinations and unsupported channels fall back as a whole; don't drop a source silently. The field evaluator remains the single authority for source geometry, finite ranges, falloff and combination. The new selection has *lower priority* than every previously available experimental path so earlier opted-in scenes select precisely their previous path.

## Components and rigid-body ownership

| Standard feature | Experimental status | Next validation |
|---|---|---|
| Auto-managed ordinary simulated Static Mesh bodies with original native gravity enabled | Previously verified for Point/Directional; newly eligible expanded fields | Test unusual simple/convex collisions, sleeping, switching |
| Source Actor (`AOrbitGravitySource`) | Shared snapshot source | Test all shapes in PIE |
| Source Component (`UOrbitGravitySourceComponent`) | Shared snapshot source | Test moving / nested / runtime-created components |
| Gravity Receiver on Actor | Shared for field queries; Channel 0-only for multi-source solver | Per-receiver independent channel masks remain a gap |
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

**Next: velocity-sensitive Vortex modes.** Pass the actual Chaos substep velocity into the existing velocity-aware evaluator; test Spiral Capture and Continuous Force with impulse/awake/sleep behavior and performance. Do not enable merely because position-only fields work.

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

A new native Editor-only world simulation suite now covers live Axis, Spline Up/Down, four Volume boundaries, mixed Axis + Directional priority, Vortex fallback, source relocation/destruction, nondefault receiver-channel fallback, and natural Chaos sleep/wake on static support. It runs with temporary Game worlds and project settings restored afterward. **Coverage authored, not yet Windows UE5.8.3 compiled or validated.** `Orbit.Physics.GravityIntegration` still holds the previously user-verified 29 math/policy tests. See [Integration Testing](INTEGRATION_TESTING.md). There is no change to the Standard default, gravity sources or Blueprint nodes.
